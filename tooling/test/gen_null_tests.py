#!/usr/bin/env python3
"""gen_null_tests.py -- generate NULL-parameter rejection tests for wheels.

For each wheel with a public .h file, parses function declarations that accept
pointer parameters and generates a test_null_<name>.c that passes NULL for each
pointer param (one test per pointer param), asserting the function returns an
error code or at least does not crash.

Usage:
    python tooling/test/gen_null_tests.py [--dry-run]
    python tooling/test/gen_null_tests.py --wheel wheels/drivers/mcp4725
    python tooling/test/gen_null_tests.py --wheel wheels/drivers/mcp4725/mcp4725.h
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

# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------

@dataclass
class Param:
    full_text: str      # e.g. "const uint8_t *data"
    type_text: str      # e.g. "const uint8_t *"
    name: str           # e.g. "data"
    is_pointer: bool
    is_func_ptr: bool   # function pointer param (skip NULL test)

@dataclass
class FuncDecl:
    return_type: str    # e.g. "ring_buffer_err_t"
    name: str           # e.g. "ring_buffer_init"
    params: list        # list of Param
    raw_line: str       # original declaration text

@dataclass
class WheelInfo:
    name: str
    layer: str
    path: Path
    header_path: Path
    funcs: list = field(default_factory=list)

# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------

# Matches a C function declaration: <return_type> <func_name>(<params>);
# Must handle multi-line declarations joined into one.
FUNC_RE = re.compile(
    r'^'
    r'([\w\s\*]+?)'           # return type (greedy but minimal)
    r'\s+'
    r'(\w+)'                   # function name
    r'\s*\('
    r'([^)]*)'                 # parameter list
    r'\)\s*;',
    re.MULTILINE
)

# Detect typedef'd function pointer params like: soft_timer_cb_t cb
# These are names ending in _cb_t, _fn_t, _fn, _handler_t, etc.
FUNC_PTR_TYPEDEF_RE = re.compile(r'_(?:cb|fn|handler|callback)_t$|_fn$')


def is_vendor_wheel(wheel_path: Path) -> bool:
    """Check if wheel has a vendor/ subdirectory (skip these)."""
    return (wheel_path / "vendor").is_dir()


def read_header(header_path: Path) -> str:
    """Read and return header content, stripping comments."""
    text = header_path.read_text(encoding="utf-8", errors="replace")
    # Remove block comments
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.DOTALL)
    # Remove line comments
    text = re.sub(r'//[^\n]*', '', text)
    return text


def join_multiline_decls(text: str) -> str:
    """Join lines that are part of a single function declaration.

    A declaration starts with a known return type and ends with );
    We join continuation lines (lines that don't end with ; or { or })
    """
    lines = text.split('\n')
    joined = []
    buf = ""
    for line in lines:
        stripped = line.strip()
        if not stripped:
            if buf:
                buf += " "
            continue
        # Skip preprocessor lines
        if stripped.startswith('#'):
            if buf:
                joined.append(buf)
                buf = ""
            continue
        buf += " " + stripped if buf else stripped
        # If we see a semicolon that closes a function declaration
        if stripped.endswith(';'):
            joined.append(buf)
            buf = ""
        elif stripped.endswith('{') or stripped.endswith('}'):
            joined.append(buf)
            buf = ""
    if buf:
        joined.append(buf)
    return '\n'.join(joined)


def parse_param(param_text: str) -> Optional[Param]:
    """Parse a single parameter from its text representation."""
    param_text = param_text.strip()
    if not param_text or param_text == 'void':
        return None

    # Handle: "..." (variadic)
    if param_text == '...' or param_text.startswith('...'):
        return None

    # Detect function pointer params: e.g. "bool (*write_reg)(void *, ...)"
    # These have pattern: type (*name)(...)
    fp_match = re.match(r'.*\(\s*\*\s*(\w+)\s*\)\s*\(', param_text)
    if fp_match:
        return Param(
            full_text=param_text,
            type_text=param_text,
            name=fp_match.group(1),
            is_pointer=True,
            is_func_ptr=True,
        )

    # Normal param: split into type + name
    # Remove array brackets: "uint8_t buf[16]" -> "uint8_t buf"
    param_clean = re.sub(r'\[[^\]]*\]', '', param_text).strip()

    # Split off the last word as name, rest is type
    parts = param_clean.rsplit(None, 1)
    if len(parts) < 2:
        # Could be just a type like "void" or a bare type
        return Param(
            full_text=param_text,
            type_text=param_clean,
            name="",
            is_pointer='*' in param_clean,
            is_func_ptr=False,
        )

    type_text, name = parts

    # The name might have leading * that belongs to the type
    while name.startswith('*'):
        type_text += ' *'
        name = name[1:]

    is_pointer = '*' in type_text
    # Check if it's a typedef'd function pointer (like soft_timer_cb_t)
    base_type = re.sub(r'\s*\*\s*', '', type_text).replace('const', '').strip()
    is_func_ptr_typedef = bool(FUNC_PTR_TYPEDEF_RE.search(base_type))

    return Param(
        full_text=param_text,
        type_text=type_text.strip(),
        name=name.strip(),
        is_pointer=is_pointer,
        is_func_ptr=is_func_ptr_typedef,
    )


def detect_prefix(header_text: str, wheel_name: str) -> str:
    """Detect the common function prefix from the header.

    Many wheels use abbreviated prefixes (hex_codec -> hex_,
    fixed_point -> fp_, string_utils -> str_).  We try the full
    wheel name first, then look at the error-enum prefix, then
    fall back to the most common function prefix.
    """
    text = join_multiline_decls(header_text)

    # Collect all candidate function names (non-static, non-typedef)
    candidates = []
    for m in FUNC_RE.finditer(text):
        ret = m.group(1).strip()
        name = m.group(2).strip()
        if 'static' in ret or 'typedef' in ret:
            continue
        candidates.append(name)

    if not candidates:
        return wheel_name + '_'

    # 1) If wheel_name_ matches any function, use it
    full_prefix = wheel_name + '_'
    if any(c.startswith(full_prefix) for c in candidates):
        return full_prefix

    # 2) Try to find the error enum prefix: <PREFIX>_ERR_PARAM
    err_re = re.compile(r'(\w+)_ERR_PARAM')
    for em in err_re.finditer(header_text):
        prefix = em.group(1).lower() + '_'
        if any(c.startswith(prefix) for c in candidates):
            return prefix

    # 3) Find longest common prefix among all candidates
    if len(candidates) >= 2:
        lcp = candidates[0]
        for c in candidates[1:]:
            while not c.startswith(lcp):
                lcp = lcp[:-1]
                if not lcp:
                    break
            if not lcp:
                break
        # Trim to last underscore to get a clean prefix
        if '_' in lcp:
            lcp = lcp[:lcp.rfind('_') + 1]
        if lcp and len(lcp) >= 2:
            return lcp

    # 4) Fallback: accept everything
    return ''


def parse_functions(header_text: str, wheel_name: str) -> list:
    """Parse all public function declarations from header text."""
    text = join_multiline_decls(header_text)
    prefix = detect_prefix(header_text, wheel_name)
    funcs = []

    for m in FUNC_RE.finditer(text):
        ret_type = m.group(1).strip()
        func_name = m.group(2).strip()
        params_str = m.group(3).strip()

        # Only match functions that start with the detected prefix
        if prefix and not func_name.startswith(prefix):
            continue

        # Skip static inline functions (they're in the header as helpers)
        if 'static' in ret_type:
            continue

        # Skip typedef'd function types (not real functions)
        if 'typedef' in ret_type:
            continue

        # Parse params -- split on commas, but respect nested parens
        params = split_params(params_str)
        parsed_params = []
        for p in params:
            pp = parse_param(p)
            if pp:
                parsed_params.append(pp)

        funcs.append(FuncDecl(
            return_type=ret_type,
            name=func_name,
            params=parsed_params,
            raw_line=m.group(0),
        ))

    return funcs


def split_params(params_str: str) -> list:
    """Split parameter list on commas, respecting nested parentheses."""
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


# ---------------------------------------------------------------------------
# Return-type classification for assertion generation
# ---------------------------------------------------------------------------

def classify_return_type(ret_type: str, wheel_name: str):
    """Classify return type for assertion strategy.

    Returns: 'err_t', 'int_return', 'unsigned', 'bool', 'void', 'pointer', 'other'
    """
    rt = ret_type.strip()

    # _err_t types: <name>_err_t, or generic like int with error semantics
    if '_err_t' in rt:
        return 'err_t'

    # int return (like mqtt_packet_build_connect returning int: <0 on error)
    if rt == 'int':
        return 'int_return'

    # unsigned integer returns (return 0 on NULL)
    if rt in ('uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
              'size_t', 'unsigned', 'unsigned int'):
        return 'unsigned'

    # signed integer returns that are not plain int
    if rt in ('int8_t', 'int16_t', 'int32_t', 'int64_t'):
        return 'unsigned'  # treat as "return 0"

    # State/enum types that map to uint16_t or similar
    if rt.startswith('sm_state_t') or rt.startswith('sm_event_t'):
        return 'unsigned'

    if rt == 'bool':
        return 'bool'

    if rt == 'void':
        return 'void'

    if '*' in rt:
        return 'pointer'

    # Enum types used as return
    return 'other'


# ---------------------------------------------------------------------------
# Default value generation for non-NULL params
# ---------------------------------------------------------------------------

def make_default_value(param: Param, wheel_name: str, idx: int) -> tuple:
    """Generate a variable declaration + value for a non-NULL param.

    Returns (decl_lines: list[str], var_name: str).
    """
    ptype = param.type_text.strip()
    pname = param.name or f"arg{idx}"

    # Function pointer typedef: use a dummy function or NULL
    if param.is_func_ptr:
        return ([], "NULL")

    # Strip const for the local variable
    base = ptype.replace('const ', '').strip()

    # Pointer to a struct: declare zero-initialized struct
    # e.g. "ring_buffer_t *" -> "ring_buffer_t var = {0};"
    if '*' in base:
        pointed_type = base.replace('*', '').strip()
        if not pointed_type:
            pointed_type = "uint8_t"
        var = f"__{pname}_{idx}"
        # For opaque types, just zero-init
        decl = f"    {pointed_type} {var};"
        init = f"    memset(&{var}, 0, sizeof({var}));"
        return ([decl, init], f"&{var}")

    # Non-pointer: just use a literal
    return ([], "0")


def make_dummy_for_type(type_text: str, var_name: str) -> list:
    """Create a dummy variable declaration for a given type."""
    base = type_text.replace('const ', '').replace('*', '').strip()
    if not base:
        return [f"    uint8_t {var_name} = 0;"]
    return [
        f"    {base} {var_name};",
        f"    memset(&{var_name}, 0, sizeof({var_name}));",
    ]

# ---------------------------------------------------------------------------
# Test file generation
# ---------------------------------------------------------------------------

def generate_test_file(wheel: WheelInfo) -> str:
    """Generate the complete test_null_<name>.c file content."""
    name = wheel.name
    lines = []
    test_names = []

    lines.append(f"// Auto-generated NULL parameter tests for {name}")
    lines.append(f"// Generated by gen_null_tests.py -- do not edit manually")
    lines.append("")
    lines.append('#include "test_harness.h"')
    lines.append(f'#include "../{name}.h"')
    lines.append('#include <string.h>')
    lines.append('#include <stddef.h>')
    lines.append("")

    for func in wheel.funcs:
        ptr_params = [(i, p) for i, p in enumerate(func.params)
                      if p.is_pointer and not p.is_func_ptr]
        if not ptr_params:
            continue

        ret_class = classify_return_type(func.return_type, name)

        for null_idx, null_param in ptr_params:
            pname = null_param.name or f"arg{null_idx}"
            # Use the full function name in the test name for clarity
            test_name = f"test_{func.name}_null_{pname}"
            test_names.append(test_name)

            lines.append(f"TEST({test_name}) {{")

            # Generate declarations for all non-NULL params
            call_args = []
            decl_lines = []

            for i, param in enumerate(func.params):
                if i == null_idx:
                    call_args.append("NULL")
                    continue

                if param.is_pointer and not param.is_func_ptr:
                    # Need a valid pointer: stack-allocate the pointed-to type
                    ptype_base = param.type_text.replace('const ', '').replace('*', '').strip()
                    local_name = param.name or f"arg{i}"
                    var = f"_{local_name}"

                    if not ptype_base:
                        ptype_base = "uint8_t"

                    # void* params (opaque context): pass NULL -- can't
                    # stack-allocate void.  These are almost always optional
                    # user-context pointers.
                    if ptype_base == 'void':
                        call_args.append("NULL")
                    # For buffer params (uint8_t*/char*), allocate an array
                    elif ptype_base in ('uint8_t', 'char', 'int8_t'):
                        decl_lines.append(f"    {ptype_base} {var}[64];")
                        decl_lines.append(f"    memset({var}, 0, sizeof({var}));")
                        call_args.append(var)
                    else:
                        decl_lines.append(f"    {ptype_base} {var};")
                        decl_lines.append(f"    memset(&{var}, 0, sizeof({var}));")
                        call_args.append(f"&{var}")
                elif param.is_func_ptr:
                    # Function pointer: pass NULL (we're testing another param)
                    call_args.append("NULL")
                else:
                    # Scalar: use a safe default
                    call_args.append(scalar_default(param.type_text))

            for dl in decl_lines:
                lines.append(dl)

            # Build the function call
            args_str = ", ".join(call_args)

            if ret_class == 'void':
                lines.append(f"    {func.name}({args_str});")
                lines.append(f"    /* must not crash */")
            elif ret_class == 'err_t':
                lines.append(f"    {func.return_type} err = {func.name}({args_str});")
                lines.append(f"    ASSERT_TRUE(err < 0);")
            elif ret_class == 'int_return':
                lines.append(f"    int ret = {func.name}({args_str});")
                lines.append(f"    ASSERT_TRUE(ret < 0);")
            elif ret_class == 'unsigned':
                lines.append(f"    {func.return_type} ret = {func.name}({args_str});")
                lines.append(f"    ASSERT_EQ(ret, 0);")
            elif ret_class == 'bool':
                lines.append(f"    (void){func.name}({args_str});")
                lines.append(f"    /* must not crash */")
            elif ret_class == 'pointer':
                lines.append(f"    {func.return_type} ret = {func.name}({args_str});")
                lines.append(f"    ASSERT_TRUE(ret == NULL);")
            else:
                # Unknown return type -- just call and check it doesn't crash
                lines.append(f"    (void){func.name}({args_str});")
                lines.append(f"    /* must not crash */")

            lines.append("}")
            lines.append("")

    # TEST_MAIN
    if not test_names:
        return ""

    lines.append(f'TEST_MAIN("null_param_{name}") {{')
    for tn in test_names:
        lines.append(f"    RUN({tn});")
    lines.append("}")
    lines.append("TEST_MAIN_END()")
    lines.append("")

    return "\n".join(lines)


def scalar_default(type_text: str) -> str:
    """Return a safe scalar default value for a non-pointer param."""
    t = type_text.replace('const ', '').strip()
    if t == 'bool':
        return 'false'
    if t == 'float':
        return '0.0f'
    if t == 'double':
        return '0.0'
    if t in ('char', 'int8_t', 'uint8_t'):
        return '0'
    if t in ('size_t', 'uint16_t', 'uint32_t', 'uint64_t',
             'int16_t', 'int32_t', 'int64_t', 'int', 'unsigned int',
             'unsigned', 'sm_state_t', 'sm_event_t'):
        return '0'
    # Enum types: 0 is usually safe
    if '_t' in t:
        return '0'
    return '0'


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def discover_wheels() -> list:
    """Discover all non-vendor wheels with a matching .h file."""
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

            # Skip vendor wheels
            if is_vendor_wheel(entry):
                continue

            # Look for <name>.h in the wheel root
            header = entry / f"{entry.name}.h"
            if not header.is_file():
                continue

            wheels.append(WheelInfo(
                name=entry.name,
                layer=layer,
                path=entry,
                header_path=header,
            ))

    return wheels


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def resolve_single_wheel(path_arg: str) -> list:
    """Resolve a --wheel argument to a single-element WheelInfo list."""
    p = Path(path_arg).resolve()
    if p.suffix == '.h':
        p = p.parent
    if not p.is_dir():
        print(f"Error: {path_arg} is not a valid wheel directory", file=sys.stderr)
        sys.exit(1)
    header = p / f"{p.name}.h"
    if not header.is_file():
        print(f"Error: no {p.name}.h found in {p}", file=sys.stderr)
        sys.exit(1)
    rel = p.relative_to(WHEELS_DIR)
    layer = rel.parts[0]
    return [WheelInfo(name=p.name, layer=layer, path=p, header_path=header)]


def main():
    dry_run = '--dry-run' in sys.argv

    single = None
    for i, arg in enumerate(sys.argv[1:], 1):
        if arg == '--wheel' and i < len(sys.argv) - 1:
            single = sys.argv[i + 1]
            break

    if single:
        wheels = resolve_single_wheel(single)
        print(f"Single-wheel mode: {wheels[0].layer}/{wheels[0].name}")
    else:
        wheels = discover_wheels()
        print(f"Discovered {len(wheels)} wheels with public headers")

    generated = 0
    skipped_no_ptrs = 0
    skipped_no_funcs = 0
    total_tests = 0
    errors = []

    for wheel in wheels:
        try:
            header_text = read_header(wheel.header_path)
            funcs = parse_functions(header_text, wheel.name)
            wheel.funcs = funcs

            if not funcs:
                skipped_no_funcs += 1
                continue

            # Check if any function has pointer params
            has_ptr = False
            for f in funcs:
                for p in f.params:
                    if p.is_pointer and not p.is_func_ptr:
                        has_ptr = True
                        break
                if has_ptr:
                    break

            if not has_ptr:
                skipped_no_ptrs += 1
                continue

            content = generate_test_file(wheel)
            if not content:
                skipped_no_ptrs += 1
                continue

            # Count tests
            n_tests = content.count("TEST(test_")
            total_tests += n_tests

            # Write file
            test_dir = wheel.path / "test"
            out_path = test_dir / f"test_null_{wheel.name}.c"

            if dry_run:
                print(f"  [DRY-RUN] {wheel.layer}/{wheel.name}: "
                      f"{len(wheel.funcs)} funcs, {n_tests} null tests")
            else:
                test_dir.mkdir(parents=True, exist_ok=True)
                out_path.write_text(content, encoding="utf-8")
                print(f"  Generated {wheel.layer}/{wheel.name}: "
                      f"{len(wheel.funcs)} funcs, {n_tests} null tests -> "
                      f"{out_path.relative_to(REPO_ROOT)}")
            generated += 1

        except Exception as e:
            errors.append((wheel.name, str(e)))
            print(f"  ERROR {wheel.layer}/{wheel.name}: {e}")

    print(f"\n{'=' * 60}")
    print(f"Summary:")
    print(f"  Wheels discovered:      {len(wheels)}")
    print(f"  Files generated:        {generated}")
    print(f"  Total NULL tests:       {total_tests}")
    print(f"  Skipped (no functions):  {skipped_no_funcs}")
    print(f"  Skipped (no ptr params): {skipped_no_ptrs}")
    print(f"  Errors:                  {len(errors)}")
    if errors:
        for name, err in errors:
            print(f"    {name}: {err}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
