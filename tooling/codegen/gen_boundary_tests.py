#!/usr/bin/env python3
"""gen_boundary_tests.py -- generate boundary/edge-case tests for wheel APIs.

For each wheel with a public .h file, parses function declarations that accept
scalar (non-pointer, non-struct) parameters and generates a
test_boundary_<name>.c that exercises each scalar param with type-appropriate
boundary values (0, MAX, MIN, NaN, INF, etc.).

Pointer parameters are skipped (covered by gen_null_tests.py).
Struct parameters are skipped (can't generate meaningful values).

Usage:
    python tooling/codegen/gen_boundary_tests.py [--dry-run]
    python tooling/codegen/gen_boundary_tests.py --wheel wheels/primitives/ewma
    python tooling/codegen/gen_boundary_tests.py wheels/primitives/ewma/ewma.h
"""

import argparse
import re
import sys
from pathlib import Path

from _wheel_parser import (
    REPO_ROOT, WHEELS_DIR,
    WheelInfo,
    read_header,
    parse_functions,
    classify_return_type,
    scalar_default,
    discover_wheels,
)

# ---------------------------------------------------------------------------
# Boundary value tables per C type
# ---------------------------------------------------------------------------

# Maps base C type -> list of (value_literal, short_label) pairs.
# short_label is used in test names (lowercase, underscore-safe).
BOUNDARY_TABLE = {
    'int32_t': [
        ('0', 'zero'),
        ('-1', 'neg1'),
        ('1', 'one'),
        ('INT32_MAX', 'int32_max'),
        ('INT32_MIN', 'int32_min'),
    ],
    'uint32_t': [
        ('0', 'zero'),
        ('1', 'one'),
        ('UINT32_MAX', 'uint32_max'),
    ],
    'int16_t': [
        ('0', 'zero'),
        ('-1', 'neg1'),
        ('1', 'one'),
        ('INT16_MAX', 'int16_max'),
        ('INT16_MIN', 'int16_min'),
    ],
    'uint16_t': [
        ('0', 'zero'),
        ('1', 'one'),
        ('UINT16_MAX', 'uint16_max'),
    ],
    'int8_t': [
        ('0', 'zero'),
        ('-1', 'neg1'),
        ('1', 'one'),
        ('INT8_MAX', 'int8_max'),
        ('INT8_MIN', 'int8_min'),
    ],
    'uint8_t': [
        ('0', 'zero'),
        ('1', 'one'),
        ('UINT8_MAX', 'uint8_max'),
    ],
    'float': [
        ('0.0f', 'zero'),
        ('-0.0f', 'neg_zero'),
        ('NAN', 'nan'),
        ('INFINITY', 'inf'),
        ('-INFINITY', 'neg_inf'),
        ('FLT_MAX', 'flt_max'),
        ('-FLT_MAX', 'neg_flt_max'),
    ],
    'double': [
        ('0.0', 'zero'),
        ('-0.0', 'neg_zero'),
        ('(double)NAN', 'nan'),
        ('(double)INFINITY', 'inf'),
        ('(double)(-INFINITY)', 'neg_inf'),
        ('DBL_MAX', 'dbl_max'),
        ('-DBL_MAX', 'neg_dbl_max'),
    ],
    'size_t': [
        ('0', 'zero'),
        ('1', 'one'),
        ('SIZE_MAX', 'size_max'),
    ],
    'bool': [
        ('true', 'val_true'),
        ('false', 'val_false'),
    ],
    # Plain int / unsigned int fallbacks
    'int': [
        ('0', 'zero'),
        ('-1', 'neg1'),
        ('1', 'one'),
        ('INT32_MAX', 'int32_max'),
        ('INT32_MIN', 'int32_min'),
    ],
    'unsigned int': [
        ('0', 'zero'),
        ('1', 'one'),
        ('UINT32_MAX', 'uint32_max'),
    ],
    'unsigned': [
        ('0', 'zero'),
        ('1', 'one'),
        ('UINT32_MAX', 'uint32_max'),
    ],
}

# Types that need float.h / math.h
FLOAT_TYPES = frozenset({'float', 'double'})

# Narrow types needing explicit casts to avoid -Wconversion
NARROW_TYPES = frozenset({'uint8_t', 'int8_t', 'uint16_t', 'int16_t'})

# Literals that are small enough to not need a narrowing cast
SMALL_LITERALS = frozenset({'0', '1', '-1', 'true', 'false', '0.0f', '-0.0f'})


# ---------------------------------------------------------------------------
# Type classification helpers
# ---------------------------------------------------------------------------

def get_boundary_values(base_type):
    """Return boundary (literal, label) pairs for a base C type, or None."""
    return BOUNDARY_TABLE.get(base_type)


def is_scalar_boundary_type(param):
    """Return True if this param has a known scalar type with boundary values,
    or is a bool."""
    return param.base_type in BOUNDARY_TABLE


def is_struct_type(param):
    """Heuristic: if base_type ends with _t but is not a known scalar or
    recognised enum suffix, it's likely a struct typedef."""
    bt = param.base_type
    if not bt:
        return False
    if bt in BOUNDARY_TABLE:
        return False
    # Known enum suffixes handled by is_enum_type
    if bt.endswith(('_err_t', '_state_t', '_mode_t', '_level_t')):
        return False
    # Likely a struct typedef
    if bt.endswith('_t') and not param.is_integer:
        return True
    return False


_callback_struct_cache: dict = {}

def struct_has_callback_field(struct_name, header_text, _depth=0):
    """Return True if the named struct typedef contains a "dangerous"
    field that breaks boundary testing — i.e. memset(0) leaves the
    struct in a state where the wheel will dereference NULL on first
    call.

    Detection evolution (each step added by a real FAIL):
      1. Direct function-pointer field: (*name)( ... )
      2. Pointer field to another _t struct that is itself dangerous
      3. Raw pointer to primitive (uint8_t*, char*) — most stateful
         wheels deref these (bitmap_t.data, ring_buffer_t.buf)
      4. Embedded value struct (no '*'), where the inner struct holds
         callbacks (max7219_t embeds max7219_spi_t by value)

    Known-but-not-yet-implemented blind spots (next time a FAIL
    appears here, extend detection rather than skip-list the wheel):
      - union { ... } fields where the active branch's NULL-state
        triggers a deref
      - flexible array member (T data[]) where the wheel writes
        beyond the zeroed end
      - bit field with enum where 0 is not a valid enum value and
        wheel dispatches on it

    Repo-wide scan 2026-05-14 (259 wheels): no real risk — all 8
    union users are discriminated unions with type=0 steering to a
    pointer-free arm; 0 wheels use flexible array members; 0 use
    enum bit fields. audit_wheels.py chk_boundary_blind_spots
    fires INFO on future wheels that introduce these patterns.
    """
    if not struct_name or _depth > 3:
        return False
    key = (struct_name, id(header_text))
    if key in _callback_struct_cache:
        return _callback_struct_cache[key]
    # Match: typedef struct [<tag>] { <body> } <struct_name>;
    pattern = re.compile(
        r'typedef\s+struct\s+\w*\s*\{([^{}]*(?:\{[^{}]*\}[^{}]*)*)\}\s*'
        + re.escape(struct_name) + r'\s*;',
        re.DOTALL,
    )
    m = pattern.search(header_text)
    if not m:
        _callback_struct_cache[key] = False
        return False
    body = m.group(1)
    # 1. Direct function pointer field: (*name)( ... )
    if re.search(r'\(\s*\*\s*\w+\s*\)\s*\(', body):
        _callback_struct_cache[key] = True
        return True
    # 2. Strip out any function-pointer field declarations so the next
    # checks don't false-match on their inner argument types.
    body_clean = re.sub(
        r'\(\s*\*\s*\w+\s*\)\s*\([^)]*\)\s*;', '', body)
    # 3. Transitive: any nested struct field — pointer OR embedded by
    # value — to a struct that has callback fields. e.g. max7219_t
    # embeds max7219_spi_t by value, and that inner struct holds
    # function pointers; memset(0) of the outer struct still zeroes
    # those pointers.
    for ftype in re.findall(r'\b(\w+_t)\b', body_clean):
        if ftype == struct_name:
            continue
        if struct_has_callback_field(ftype, header_text, _depth + 1):
            _callback_struct_cache[key] = True
            return True
    # 4. Any raw pointer field (uint8_t*, char*, etc.). memset(0) of a
    # struct with such fields leaves them NULL, and most stateful wheels
    # dereference them in non-init APIs (e.g. bitmap_t.data,
    # ring_buffer_t.buf).
    if re.search(r'\b\w+\s*\*\s*\w+\s*[;,]', body_clean):
        _callback_struct_cache[key] = True
        return True
    _callback_struct_cache[key] = False
    return False


def function_has_callback_struct_arg(func, header_text):
    """Return True if any param is a pointer to a struct that owns
    callback function pointer fields. Boundary tests for such functions
    cannot safely use memset(0) initialisation."""
    for param in func.params:
        if not param.is_pointer:
            continue
        base = param.base_type
        if not base or not base.endswith('_t'):
            continue
        if base in BOUNDARY_TABLE:
            continue
        if base.endswith(('_err_t', '_state_t', '_mode_t', '_level_t')):
            continue
        if struct_has_callback_field(base, header_text):
            return True
    return False


def is_enum_type(base_type, header_text):
    """Check if a type looks like an enum defined in the header."""
    if not base_type.endswith('_t'):
        return False
    pattern = re.compile(
        r'typedef\s+enum\s*\{([^}]*)\}\s*' + re.escape(base_type),
        re.DOTALL,
    )
    return pattern.search(header_text) is not None


def get_enum_max(base_type, header_text):
    """Try to extract the max numeric value from an enum definition.

    Returns (max_value: int, max_name: str) or None.
    """
    pattern = re.compile(
        r'typedef\s+enum\s*\{([^}]*)\}\s*' + re.escape(base_type),
        re.DOTALL,
    )
    m = pattern.search(header_text)
    if not m:
        return None

    body = m.group(1)
    max_val = None
    max_name = None
    current_val = 0

    for line in body.split(','):
        line = line.strip()
        if not line:
            continue
        assign = re.match(r'(\w+)\s*=\s*(-?\d+)', line)
        if assign:
            name = assign.group(1)
            val = int(assign.group(2))
            current_val = val
        else:
            name_match = re.match(r'(\w+)', line)
            if not name_match:
                continue
            name = name_match.group(1)
            val = current_val

        if max_val is None or val > max_val:
            max_val = val
            max_name = name
        current_val = val + 1

    if max_val is not None:
        return (max_val, max_name)
    return None


def needs_cast(param_ctype, literal):
    """Check if a boundary literal needs an explicit cast to the param type
    to avoid -Wconversion warnings."""
    if param_ctype in NARROW_TYPES and literal not in SMALL_LITERALS:
        return True
    return False


# ---------------------------------------------------------------------------
# Safe default for non-target params
# ---------------------------------------------------------------------------

def make_safe_arg(param, wheel_name, idx):
    """Generate a safe default argument for a param that is NOT the boundary
    target. Returns (decl_lines: list[str], expr: str)."""
    ptype = param.type_text.strip()

    if param.is_func_ptr:
        return ([], "NULL")

    base = ptype.replace('const ', '').replace('volatile ', '').strip()

    if param.is_pointer:
        star_count = base.count('*')
        pointed = base.replace('*', '').strip() or 'uint8_t'
        var = f"_safe_{param.name or f'arg{idx}'}_{idx}"

        if pointed == 'void':
            return ([], "NULL")

        if star_count >= 2:
            has_const = 'const' in ptype
            inner = ('const ' + pointed) if has_const else pointed
            return ([f"    {inner} *{var} = NULL;"], f"&{var}")

        if pointed in ('uint8_t', 'char', 'int8_t'):
            return (
                [f"    {pointed} {var}[64];",
                 f"    memset({var}, 0, sizeof({var}));"],
                var,
            )

        # Non-const struct pointer: allocate as a 128-element array.
        # Many wheels (encode/pack/serialize style) write multiple
        # struct elements through such a pointer, e.g. ir_nec_encode
        # writes ~68 ir_nec_pulse_t entries. A single struct on the
        # stack would smash. 128 covers typical encode patterns; for
        # ir_nec_pulse_t (8B) that's 1KB stack.
        has_const = 'const' in ptype
        if has_const:
            return (
                [f"    {pointed} {var};",
                 f"    memset(&{var}, 0, sizeof({var}));"],
                f"&{var}",
            )
        return (
            [f"    {pointed} {var}[128];",
             f"    memset({var}, 0, sizeof({var}));"],
            var,
        )

    return ([], scalar_default(ptype))


# ---------------------------------------------------------------------------
# Test file generation
# ---------------------------------------------------------------------------

def generate_test_file(wheel, header_text):
    """Generate the complete test_boundary_<name>.c file content."""
    name = wheel.name
    lines = []
    test_names = []
    uses_float = False

    lines.append(f"// Auto-generated boundary/edge-case tests for {name}")
    lines.append("// Generated by gen_boundary_tests.py -- do not edit manually")
    lines.append("")
    lines.append('#include "test_harness.h"')
    lines.append(f'#include "../{name}.h"')
    lines.append('#include <string.h>')
    lines.append('#include <stddef.h>')
    lines.append('#include <stdint.h>')
    lines.append('#include <limits.h>')
    lines.append('#include <float.h>')
    lines.append('#include <math.h>')
    lines.append("")

    skipped_callback_structs = 0
    for func in wheel.funcs:
        # Skip functions taking pointers to callback-bearing structs:
        # memset(0) + call would segfault on first dispatch. Boundary
        # coverage for these functions belongs in hw_model tests where
        # the wheel's mock HAL is wired up.
        if function_has_callback_struct_arg(func, header_text):
            skipped_callback_structs += 1
            continue

        ret_class = classify_return_type(func.return_type, name)
        has_err_return = (ret_class == 'err_t')

        for target_idx, target_param in enumerate(func.params):
            # Skip pointers (covered by gen_null_tests.py)
            if target_param.is_pointer:
                continue
            # Skip function pointers
            if target_param.is_func_ptr:
                continue

            bt = target_param.base_type

            # Determine boundary values for this param
            boundaries = get_boundary_values(bt)

            if boundaries is None:
                # Check if it's an enum
                if is_enum_type(bt, header_text):
                    boundaries = [('0', 'zero'), ('-1', 'neg1')]
                    enum_max = get_enum_max(bt, header_text)
                    if enum_max is not None:
                        max_val, _ = enum_max
                        boundaries.append((str(max_val + 1), 'past_max'))
                elif is_struct_type(target_param):
                    continue
                else:
                    continue

            if bt in FLOAT_TYPES:
                uses_float = True

            pname = target_param.name or f"arg{target_idx}"

            # Length-like parameters (len, size, cap, count, n_*) drive
            # buffer access in the function body. Boundary inputs at the
            # numeric type max (SIZE_MAX, UINT16_MAX, ...) only exercise
            # genuine wheel logic when matched with a similarly-large
            # buffer — but make_safe_arg() always allocates 64-byte
            # buffers. Passing UINT*_MAX as a length therefore violates
            # the function's "len <= sizeof(buf)" contract and segfaults
            # before any error code can be returned. Drop those entries
            # for length-like params.
            len_param_names = {
                'len', 'length', 'size', 'cap', 'capacity', 'count',
                'n', 'num', 'nbytes', 'nbits', 'src_len', 'dst_cap',
                'dst_len', 'buf_len', 'buf_size', 'in_len', 'out_len',
                'data_len', 'msg_len', 'cap_bytes', 'cap_bits',
            }
            len_param_prefixes = ('num_', 'n_', 'cnt_', 'count_')
            len_param_suffixes = (
                '_len', '_length', '_size', '_cap', '_capacity',
                '_count', '_n', '_bytes', '_bits', '_leds', '_pixels',
                '_samples', '_items', '_entries', '_chunks',
            )
            is_len_like = (
                pname in len_param_names
                or pname.startswith(len_param_prefixes)
                or pname.endswith(len_param_suffixes)
            )
            if is_len_like:
                boundaries = [
                    (lit, lab) for (lit, lab) in boundaries
                    if 'max' not in lab.lower()
                ]

            for literal, label in boundaries:
                test_name = f"test_boundary_{func.name}_{pname}_{label}"
                # Deduplicate if same name already exists
                if test_name in test_names:
                    test_name += f"_{target_idx}"
                test_names.append(test_name)

                lines.append(f"TEST({test_name}) {{")

                decl_lines = []
                call_args = []

                for i, param in enumerate(func.params):
                    if i == target_idx:
                        param_ctype = param.type_text.replace(
                            'const ', '').strip()
                        if needs_cast(param_ctype, literal):
                            call_args.append(
                                f"({param_ctype})({literal})")
                        else:
                            call_args.append(literal)
                    else:
                        dlines, expr = make_safe_arg(param, name, i)
                        decl_lines.extend(dlines)
                        call_args.append(expr)

                for dl in decl_lines:
                    lines.append(dl)

                args_str = ", ".join(call_args)

                if has_err_return:
                    # Per api-conventions §4: OK = 0, all error codes
                    # are explicit negative values. Boundary inputs can
                    # legitimately trigger any error variant
                    # (ERR_OVERFLOW, ERR_INVALID, ERR_RANGE, …), not
                    # only ERR_PARAM. Assert `ret <= 0` to reject
                    # accidental positive returns / undefined behaviour
                    # while accepting the full negative error space.
                    ret_var = f"ret"
                    lines.append(
                        f"    {func.return_type} {ret_var} = "
                        f"{func.name}({args_str});")
                    lines.append(
                        f"    ASSERT_TRUE((int){ret_var} <= 0);")
                else:
                    lines.append(f"    (void){func.name}({args_str});")
                    lines.append("    /* must not crash */")

                lines.append("}")
                lines.append("")

    if not test_names:
        # All functions involve callback-bearing structs (or no testable
        # params). Emit a placeholder test so the build / run pipeline
        # remains uniform, harness static state vars are referenced (no
        # -Wunused-variable), and the binary exits 0.
        if skipped_callback_structs > 0:
            reason = (f"all {skipped_callback_structs} function(s) take "
                      f"callback-bearing struct pointers")
        else:
            reason = "no scalar boundary params"
        lines.append(
            f"/* No boundary tests generated: {reason}. */")
        lines.append(
            "/* Boundary coverage for callback-bearing structs belongs in "
            "hw_model_*.c. */")
        lines.append("")
        lines.append(f"TEST(test_boundary_{name}_placeholder) {{")
        lines.append(
            "    ASSERT_TRUE(1);  /* no scalar boundary params; "
            "see hw_model tests */")
        lines.append("}")
        lines.append("")
        lines.append(f'TEST_MAIN("boundary_{name}") {{')
        lines.append(f"    RUN(test_boundary_{name}_placeholder);")
        lines.append("}")
        lines.append("TEST_MAIN_END()")
        lines.append("")
        return "\n".join(lines), uses_float

    lines.append(f'TEST_MAIN("boundary_{name}") {{')
    for tn in test_names:
        lines.append(f"    RUN({tn});")
    lines.append("}")
    lines.append("TEST_MAIN_END()")
    lines.append("")

    return "\n".join(lines), uses_float


# ---------------------------------------------------------------------------
# CMakeLists.txt auto-registration
# ---------------------------------------------------------------------------

def update_cmakelists(wheel, needs_m, dry_run):
    """Add test_boundary_<name> target to CMakeLists.txt if missing.

    Returns True if the file was updated (or would be in dry-run).
    """
    cmake_path = wheel.path / "test" / "CMakeLists.txt"
    if not cmake_path.is_file():
        return False

    content = cmake_path.read_text(encoding="utf-8")
    target = f"test_boundary_{wheel.name}"

    if target in content:
        return False

    # Extract source files from existing add_executable for the main test
    src_re = re.compile(
        r'add_executable\(\s*test_' + re.escape(wheel.name)
        + r'\s*\n(.*?)\)', re.DOTALL)
    src_match = src_re.search(content)
    sources = [f"../{wheel.name}.c"]
    if src_match:
        src_block = src_match.group(1)
        for line in src_block.strip().split('\n'):
            s = line.strip()
            if s.startswith('../') and s.endswith('.c'):
                if s not in sources:
                    sources.append(s)

    has_contract = 'CONTRACT_ENABLED' in content
    has_enable_testing = 'enable_testing()' in content

    src_list = "\n".join(f"    {s}" for s in [f"{target}.c"] + sources)
    block_lines = [
        "",
        f"add_executable({target}",
        src_list,
        ")",
        f"target_include_directories({target} PRIVATE",
        "    ${CMAKE_CURRENT_SOURCE_DIR}/..",
        "    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness",
        ")",
    ]
    if has_contract:
        block_lines.append(
            f"target_compile_definitions({target} PRIVATE CONTRACT_ENABLED)")
    block_lines.append(
        f"target_compile_options({target} PRIVATE"
        " -Wall -Wextra -Wpedantic -Werror -Wconversion)")
    if needs_m:
        block_lines.append(
            f"target_link_libraries({target} PRIVATE m)")

    test_line = (
        f"add_test(NAME {wheel.name}_boundary_tests COMMAND {target})")

    lines = content.rstrip('\n').split('\n')

    if has_enable_testing:
        et_idx = next(i for i, l in enumerate(lines)
                      if 'enable_testing()' in l)
        for j, bl in enumerate(block_lines):
            lines.insert(et_idx + j, bl)
        lines.append(test_line)
    else:
        lines.extend(block_lines)
        lines.append("")
        lines.append("enable_testing()")
        lines.append(test_line)

    new_content = "\n".join(lines) + "\n"

    if dry_run:
        print(f"  [DRY-RUN] Would update {cmake_path.relative_to(REPO_ROOT)}")
    else:
        cmake_path.write_text(new_content, encoding="utf-8")
        print(f"  Updated  {cmake_path.relative_to(REPO_ROOT)}"
              f" (added {target} target)")

    return True


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def resolve_single_wheel(path_arg):
    """Resolve a --wheel or positional argument to a single-element
    WheelInfo list."""
    p = Path(path_arg).resolve()
    if p.suffix == '.h':
        p = p.parent
    if not p.is_dir():
        print(f"Error: {path_arg} is not a valid wheel directory",
              file=sys.stderr)
        sys.exit(1)
    header = p / f"{p.name}.h"
    if not header.is_file():
        print(f"Error: no {p.name}.h found in {p}", file=sys.stderr)
        sys.exit(1)
    rel = p.relative_to(WHEELS_DIR)
    layer = rel.parts[0]
    return [WheelInfo(name=p.name, layer=layer, path=p,
                      header_path=header)]


def main():
    parser = argparse.ArgumentParser(
        description="Generate boundary/edge-case tests for wheel APIs."
    )
    parser.add_argument("--dry-run", action="store_true",
                        help="print actions without writing files")
    parser.add_argument("--wheel", default=None,
                        help="single wheel path (e.g. wheels/primitives/ewma or .h file)")
    parser.add_argument("header", nargs="?", default=None,
                        help="positional shortcut: path to a single .h file")
    args = parser.parse_args()
    dry_run = args.dry_run
    single = args.wheel or args.header

    if single:
        wheels = resolve_single_wheel(single)
        print(f"Single-wheel mode: {wheels[0].layer}/{wheels[0].name}")
    else:
        wheels = discover_wheels()
        print(f"Discovered {len(wheels)} wheels with public headers")

    generated = 0
    skipped_no_scalars = 0
    skipped_no_funcs = 0
    total_tests = 0
    errors = []

    for wheel in wheels:
        try:
            header_text = read_header(wheel.header_path)
            funcs = parse_functions(header_text, wheel.name,
                                    compute_extra=True)
            wheel.funcs = funcs

            if not funcs:
                skipped_no_funcs += 1
                continue

            result = generate_test_file(wheel, header_text)
            if not result:
                skipped_no_scalars += 1
                continue

            content, uses_float = result

            n_tests = content.count("TEST(test_")
            total_tests += n_tests

            test_dir = wheel.path / "test"
            out_path = test_dir / f"test_boundary_{wheel.name}.c"

            if dry_run:
                print(f"  [DRY-RUN] {wheel.layer}/{wheel.name}: "
                      f"{len(wheel.funcs)} funcs, {n_tests} boundary tests")
            else:
                test_dir.mkdir(parents=True, exist_ok=True)
                out_path.write_text(content, encoding="utf-8")
                print(f"  Generated {wheel.layer}/{wheel.name}: "
                      f"{len(wheel.funcs)} funcs, {n_tests} boundary tests"
                      f" -> {out_path.relative_to(REPO_ROOT)}")

            update_cmakelists(wheel, uses_float, dry_run)
            generated += 1

        except Exception as e:
            errors.append((wheel.name, str(e)))
            print(f"  ERROR {wheel.layer}/{wheel.name}: {e}")

    print(f"\n{'=' * 60}")
    print("Summary:")
    print(f"  Wheels discovered:         {len(wheels)}")
    print(f"  Files generated:           {generated}")
    print(f"  Total boundary tests:      {total_tests}")
    print(f"  Skipped (no functions):     {skipped_no_funcs}")
    print(f"  Skipped (no scalar params): {skipped_no_scalars}")
    print(f"  Errors:                     {len(errors)}")
    if errors:
        for name, err in errors:
            print(f"    {name}: {err}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
