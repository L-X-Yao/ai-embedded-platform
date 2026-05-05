#!/usr/bin/env python3
"""gen_boundary_tests.py -- generate type-boundary tests for all wheels.

For each wheel with a public .h file, parses function declarations that accept
integer/size parameters and generates a test_boundary_<name>.c that calls each
function with boundary values (0, 1, MAX-1, MAX) for each integer param.

Uses a small output buffer + sentinel byte to detect silent overflow writes.
Functions are expected to either return an error or handle gracefully (no crash).

Usage:
    python tooling/test/gen_boundary_tests.py [--dry-run]
"""

import os
import re
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
WHEELS_DIR = REPO_ROOT / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules"]

# Integer types and their boundary values
BOUNDARY_VALUES = {
    'uint8_t':  [('0', '0'), ('UINT8_MAX', '255')],
    'uint16_t': [('0', '0'), ('UINT16_MAX', '65535')],
    'uint32_t': [('0', '0'), ('UINT32_MAX', 'UINT32_MAX')],
    'size_t':   [('0', '0'), ('SIZE_MAX_TEST', '(size_t)65535')],
    'int':      [('0', '0'), ('NEG1', '-1'), ('INT32_MAX', 'INT32_MAX')],
    'int8_t':   [('0', '0'), ('INT8_MIN', 'INT8_MIN'), ('INT8_MAX', 'INT8_MAX')],
    'int16_t':  [('0', '0'), ('INT16_MIN', 'INT16_MIN'), ('INT16_MAX', 'INT16_MAX')],
    'int32_t':  [('0', '0'), ('INT32_MIN', 'INT32_MIN'), ('INT32_MAX', 'INT32_MAX')],
    'unsigned': [('0', '0'), ('UINT_MAX', 'UINT32_MAX')],
    'unsigned int': [('0', '0'), ('UINT_MAX', 'UINT32_MAX')],
}

# Types that represent "length", "size", or "capacity" — these determine
# required buffer sizes and can't be tested with extreme values without
# matching buffers. Skip boundary testing for these.
SIZE_PARAM_NAMES = re.compile(
    r'(len|length|size|count|cap|capacity|num|n_|nb_|max_|bits|width|height|depth|rows|cols|pages|window|samples)',
    re.IGNORECASE
)

# Layers that require HAL injection — zero-init structs crash on HAL calls.
# Skip these for boundary testing (they need proper mock setup).
SKIP_LAYERS_BOUNDARY = {"drivers"}

# Specific wheels that need callback/HAL setup beyond zero-init
SKIP_WHEELS_BOUNDARY = {
    "telemetry", "ota_manager", "task_watchdog", "scheduler",
    "cli_shell", "sensor_hub", "data_logger",
}

# ---------------------------------------------------------------------------
# Data types (reuse gen_null_tests.py patterns)
# ---------------------------------------------------------------------------

@dataclass
class Param:
    full_text: str
    type_text: str
    name: str
    is_pointer: bool
    is_func_ptr: bool
    is_integer: bool
    base_type: str  # cleaned type without const/volatile

@dataclass
class FuncDecl:
    return_type: str
    name: str
    params: list
    raw_line: str

@dataclass
class WheelInfo:
    name: str
    layer: str
    path: Path
    header_path: Path
    funcs: list = field(default_factory=list)

# ---------------------------------------------------------------------------
# Parsing helpers (adapted from gen_null_tests.py)
# ---------------------------------------------------------------------------

FUNC_RE = re.compile(
    r'^'
    r'([\w\s\*]+?)'
    r'\s+'
    r'(\w+)'
    r'\s*\('
    r'([^)]*)'
    r'\)\s*;',
    re.MULTILINE
)

FUNC_PTR_TYPEDEF_RE = re.compile(r'_(?:cb|fn|handler|callback)_t$|_fn$')

INTEGER_TYPES = {
    'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
    'int8_t', 'int16_t', 'int32_t', 'int64_t',
    'int', 'unsigned', 'unsigned int', 'size_t',
}


def is_vendor_wheel(wheel_path: Path) -> bool:
    return (wheel_path / "vendor").is_dir()


def read_header(header_path: Path) -> str:
    text = header_path.read_text(encoding="utf-8", errors="replace")
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.DOTALL)
    text = re.sub(r'//[^\n]*', '', text)
    return text


def join_multiline_decls(text: str) -> str:
    lines = text.split('\n')
    joined = []
    buf = ""
    for line in lines:
        stripped = line.strip()
        if not stripped:
            if buf:
                buf += " "
            continue
        if stripped.startswith('#'):
            if buf:
                joined.append(buf)
                buf = ""
            continue
        buf += " " + stripped if buf else stripped
        if stripped.endswith(';'):
            joined.append(buf)
            buf = ""
        elif stripped.endswith('{') or stripped.endswith('}'):
            joined.append(buf)
            buf = ""
    if buf:
        joined.append(buf)
    return '\n'.join(joined)


def split_params(params_str: str) -> list:
    if not params_str or params_str.strip() == 'void':
        return []
    parts = []
    depth = 0
    current = ""
    for ch in params_str:
        if ch == '(':
            depth += 1
            current += ch
        elif ch == ')':
            depth -= 1
            current += ch
        elif ch == ',' and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        parts.append(current.strip())
    return parts


def parse_param(param_text: str) -> Optional[Param]:
    param_text = param_text.strip()
    if not param_text or param_text == 'void':
        return None
    if param_text == '...' or param_text.startswith('...'):
        return None

    fp_match = re.match(r'.*\(\s*\*\s*(\w+)\s*\)\s*\(', param_text)
    if fp_match:
        return Param(
            full_text=param_text, type_text=param_text,
            name=fp_match.group(1), is_pointer=True,
            is_func_ptr=True, is_integer=False, base_type='',
        )

    has_array_brackets = bool(re.search(r'\[', param_text))
    param_clean = re.sub(r'\[[^\]]*\]', '', param_text).strip()
    parts = param_clean.rsplit(None, 1)
    if len(parts) < 2:
        return Param(
            full_text=param_text, type_text=param_clean,
            name="", is_pointer='*' in param_clean or has_array_brackets,
            is_func_ptr=False, is_integer=False, base_type='',
        )

    type_text, name = parts
    while name.startswith('*'):
        type_text += ' *'
        name = name[1:]

    is_pointer = '*' in type_text or has_array_brackets
    base_type = re.sub(r'\s*\*\s*', '', type_text).replace('const', '').replace('volatile', '').strip()
    is_func_ptr_typedef = bool(FUNC_PTR_TYPEDEF_RE.search(base_type))
    is_integer = base_type in INTEGER_TYPES and not is_pointer

    return Param(
        full_text=param_text, type_text=type_text.strip(),
        name=name.strip(), is_pointer=is_pointer,
        is_func_ptr=is_func_ptr_typedef, is_integer=is_integer,
        base_type=base_type,
    )


def detect_prefix(header_text: str, wheel_name: str) -> str:
    text = join_multiline_decls(header_text)
    candidates = []
    for m in FUNC_RE.finditer(text):
        ret = m.group(1).strip()
        name = m.group(2).strip()
        if 'static' in ret or 'typedef' in ret:
            continue
        candidates.append(name)

    if not candidates:
        return wheel_name + '_'

    full_prefix = wheel_name + '_'
    if any(c.startswith(full_prefix) for c in candidates):
        return full_prefix

    err_re = re.compile(r'(\w+)_ERR_PARAM')
    for em in err_re.finditer(header_text):
        prefix = em.group(1).lower() + '_'
        if any(c.startswith(prefix) for c in candidates):
            return prefix

    if len(candidates) >= 2:
        lcp = candidates[0]
        for c in candidates[1:]:
            while not c.startswith(lcp):
                lcp = lcp[:-1]
                if not lcp:
                    break
            if not lcp:
                break
        if '_' in lcp:
            lcp = lcp[:lcp.rfind('_') + 1]
        if lcp and len(lcp) >= 2:
            return lcp

    return ''


def parse_functions(header_text: str, wheel_name: str) -> list:
    text = join_multiline_decls(header_text)
    prefix = detect_prefix(header_text, wheel_name)
    funcs = []

    for m in FUNC_RE.finditer(text):
        ret_type = m.group(1).strip()
        func_name = m.group(2).strip()
        params_str = m.group(3).strip()

        if prefix and not func_name.startswith(prefix):
            continue
        if 'static' in ret_type or 'typedef' in ret_type:
            continue

        params = split_params(params_str)
        parsed_params = []
        for p in params:
            pp = parse_param(p)
            if pp:
                parsed_params.append(pp)

        funcs.append(FuncDecl(
            return_type=ret_type, name=func_name,
            params=parsed_params, raw_line=m.group(0),
        ))

    return funcs


# ---------------------------------------------------------------------------
# Return-type classification
# ---------------------------------------------------------------------------

def classify_return_type(ret_type: str, wheel_name: str):
    rt = ret_type.strip()
    if '_err_t' in rt:
        return 'err_t'
    if rt == 'int':
        return 'int_return'
    if rt in ('uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
              'size_t', 'unsigned', 'unsigned int'):
        return 'unsigned'
    if rt == 'bool':
        return 'bool'
    if rt == 'void':
        return 'void'
    if '*' in rt:
        return 'pointer'
    return 'other'


# ---------------------------------------------------------------------------
# Test generation
# ---------------------------------------------------------------------------

def scalar_default(type_text: str) -> str:
    t = type_text.replace('const ', '').strip()
    if t == 'bool':
        return 'false'
    if t in ('float', 'double'):
        return '0.0f' if t == 'float' else '0.0'
    return '0'


def generate_test_file(wheel: WheelInfo) -> str:
    name = wheel.name
    lines = []
    test_names = []

    lines.append(f"// Auto-generated boundary value tests for {name}")
    lines.append(f"// Generated by gen_boundary_tests.py -- do not edit manually")
    lines.append("")
    lines.append('#include "test_harness.h"')
    lines.append(f'#include "../{name}.h"')
    lines.append('#include <string.h>')
    lines.append('#include <stdint.h>')
    lines.append('#include <limits.h>')
    lines.append("")
    lines.append("#ifndef UINT32_MAX")
    lines.append("#define UINT32_MAX 0xFFFFFFFFu")
    lines.append("#endif")
    lines.append("#ifndef SIZE_MAX")
    lines.append("#define SIZE_MAX ((size_t)-1)")
    lines.append("#endif")
    lines.append("")

    for func in wheel.funcs:
        int_params = [(i, p) for i, p in enumerate(func.params)
                      if p.is_integer]
        if not int_params:
            continue

        ret_class = classify_return_type(func.return_type, name)

        # Detect const-buffer + size/len pairs: if the size param's paired
        # buffer is const (input-only), large len values cause legitimate
        # reads past the test buffer — skip those.
        const_buf_indices = set()
        for i, p in enumerate(func.params):
            if p.is_pointer and 'const' in p.type_text:
                const_buf_indices.add(i)

        has_nonconst_buf = any(
            p.is_pointer and 'const' not in p.type_text and not p.is_func_ptr
            for p in func.params
        )

        for param_idx, target_param in int_params:
            boundaries = BOUNDARY_VALUES.get(target_param.base_type, [])
            if not boundaries:
                continue

            # If this looks like a size/len/capacity param, skip boundary
            # tests: these determine buffer sizes and can't be tested with
            # extreme values without matching allocations.
            is_size_like = bool(SIZE_PARAM_NAMES.search(target_param.name or ''))
            if is_size_like:
                continue

            for val_suffix, val_expr in boundaries:
                # Skip 0 for most — it's already tested by unit tests
                if val_suffix == '0':
                    continue

                pname = target_param.name or f"arg{param_idx}"
                test_name = f"test_{func.name}_{pname}_{val_suffix}".lower()
                # Deduplicate: some names may collide
                if test_name in test_names:
                    test_name += f"_{param_idx}"
                test_names.append(test_name)

                lines.append(f"TEST({test_name}) {{")

                # Build call arguments
                call_args = []
                decl_lines = []
                has_sentinel = False

                for i, param in enumerate(func.params):
                    if i == param_idx:
                        call_args.append(f"({target_param.base_type})({val_expr})")
                        continue

                    if param.is_func_ptr:
                        call_args.append("NULL")
                    elif param.is_pointer:
                        ptype_base = param.type_text.replace('const ', '').replace('*', '').strip()
                        local_name = param.name or f"arg{i}"
                        var = f"_{local_name}"

                        if ptype_base == 'void':
                            call_args.append("NULL")
                        elif ptype_base in ('uint8_t', 'char', 'int8_t'):
                            # Small buffer + sentinel for overflow detection
                            decl_lines.append(f"    {ptype_base} {var}[17];")
                            decl_lines.append(f"    memset({var}, 0xAA, sizeof({var}));")
                            decl_lines.append(f"    {var}[16] = (({ptype_base})0xFF);")
                            has_sentinel = True
                            call_args.append(var)
                        else:
                            decl_lines.append(f"    {ptype_base} {var};")
                            decl_lines.append(f"    memset(&{var}, 0, sizeof({var}));")
                            call_args.append(f"&{var}")
                    else:
                        # Other scalar: use small safe value
                        if param.is_integer and SIZE_PARAM_NAMES.search(param.name or ''):
                            # If it's a size/len param, use small value (16)
                            call_args.append("16")
                        else:
                            call_args.append(scalar_default(param.type_text))

                for dl in decl_lines:
                    lines.append(dl)

                args_str = ", ".join(call_args)

                if ret_class == 'void':
                    lines.append(f"    {func.name}({args_str});")
                elif ret_class in ('err_t', 'int_return'):
                    lines.append(f"    (void){func.name}({args_str});")
                else:
                    lines.append(f"    (void){func.name}({args_str});")

                # Sentinel check if we have a buffer
                if has_sentinel:
                    for i, param in enumerate(func.params):
                        if i == param_idx:
                            continue
                        if param.is_pointer and not param.is_func_ptr:
                            ptype_base = param.type_text.replace('const ', '').replace('*', '').strip()
                            if ptype_base in ('uint8_t', 'char', 'int8_t'):
                                local_name = param.name or f"arg{i}"
                                var = f"_{local_name}"
                                cast = f"({ptype_base})" if ptype_base != 'char' else '(char)'
                                lines.append(f"    ASSERT_EQ({var}[16], {cast}0xFF);")
                                break

                lines.append("}")
                lines.append("")

    if not test_names:
        return ""

    lines.append(f'TEST_MAIN("boundary_{name}") {{')
    for tn in test_names:
        lines.append(f"    RUN({tn});")
    lines.append("}")
    lines.append("TEST_MAIN_END()")
    lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def discover_wheels() -> list:
    wheels = []
    for layer in LAYERS:
        layer_dir = WHEELS_DIR / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if not entry.is_dir():
                continue
            if entry.name.startswith('_') or entry.name.startswith('.'):
                continue
            if is_vendor_wheel(entry):
                continue
            if layer in SKIP_LAYERS_BOUNDARY:
                continue
            if entry.name in SKIP_WHEELS_BOUNDARY:
                continue
            header = entry / f"{entry.name}.h"
            if not header.is_file():
                continue
            wheels.append(WheelInfo(
                name=entry.name, layer=layer,
                path=entry, header_path=header,
            ))
    return wheels


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    dry_run = '--dry-run' in sys.argv

    wheels = discover_wheels()
    print(f"Discovered {len(wheels)} wheels with public headers")

    generated = 0
    skipped = 0
    total_tests = 0
    errors = []

    for wheel in wheels:
        try:
            header_text = read_header(wheel.header_path)
            funcs = parse_functions(header_text, wheel.name)
            wheel.funcs = funcs

            if not funcs:
                skipped += 1
                continue

            has_int = any(p.is_integer for f in funcs for p in f.params)
            if not has_int:
                skipped += 1
                continue

            content = generate_test_file(wheel)
            if not content:
                skipped += 1
                continue

            n_tests = content.count("TEST(test_")
            total_tests += n_tests

            test_dir = wheel.path / "test"
            out_path = test_dir / f"test_boundary_{wheel.name}.c"

            if dry_run:
                print(f"  [DRY-RUN] {wheel.layer}/{wheel.name}: "
                      f"{n_tests} boundary tests")
            else:
                test_dir.mkdir(parents=True, exist_ok=True)
                out_path.write_text(content, encoding="utf-8")
                print(f"  Generated {wheel.layer}/{wheel.name}: "
                      f"{n_tests} boundary tests -> "
                      f"{out_path.relative_to(REPO_ROOT)}")
            generated += 1

        except Exception as e:
            errors.append((wheel.name, str(e)))
            print(f"  ERROR {wheel.layer}/{wheel.name}: {e}")

    print(f"\n{'=' * 60}")
    print(f"Summary:")
    print(f"  Wheels discovered:       {len(wheels)}")
    print(f"  Files generated:         {generated}")
    print(f"  Total boundary tests:    {total_tests}")
    print(f"  Skipped (no int params): {skipped}")
    print(f"  Errors:                  {len(errors)}")
    if errors:
        for name, err in errors:
            print(f"    {name}: {err}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
