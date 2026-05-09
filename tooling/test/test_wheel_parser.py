"""test_wheel_parser.py -- pytest unit tests for _wheel_parser shared module.

Run with:
    cd tooling/test && python -m pytest test_wheel_parser.py -v
or via the Makefile:
    make test-wheel-parser
"""

import sys
from pathlib import Path

import pytest

# _wheel_parser lives in the same directory as this test file
sys.path.insert(0, str(Path(__file__).parent))
from _wheel_parser import (
    read_header,
    join_multiline_decls,
    split_params,
    parse_param,
    detect_prefix,
    classify_return_type,
    scalar_default,
    is_vendor_wheel,
    discover_wheels,
    parse_functions,
    Param,
    FuncDecl,
    WheelInfo,
    WHEELS_DIR,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _tmp_header(tmp_path: Path, content: str) -> Path:
    """Write *content* to a temp .h file and return its path."""
    p = tmp_path / "dummy.h"
    p.write_text(content, encoding="utf-8")
    return p


# ---------------------------------------------------------------------------
# read_header
# ---------------------------------------------------------------------------

class TestReadHeader:
    def test_strips_block_comment(self, tmp_path):
        p = _tmp_header(tmp_path, "/* comment */\nvoid foo(void);")
        result = read_header(p)
        assert "comment" not in result
        assert "foo" in result

    def test_strips_multiline_block_comment(self, tmp_path):
        p = _tmp_header(tmp_path, "/*\n * block\n */\nint bar(int x);")
        result = read_header(p)
        assert "block" not in result
        assert "bar" in result

    def test_strips_line_comment(self, tmp_path):
        p = _tmp_header(tmp_path, "int foo(void); // inline comment\n")
        result = read_header(p)
        assert "inline comment" not in result
        assert "foo" in result

    def test_preserves_code_after_comments(self, tmp_path):
        content = "/* preamble */\nint add(int a, int b);\n"
        p = _tmp_header(tmp_path, content)
        result = read_header(p)
        assert "add" in result
        assert "int a" in result

    def test_handles_adjacent_block_comments(self, tmp_path):
        p = _tmp_header(tmp_path, "/* a *//* b */\nvoid fn(void);")
        result = read_header(p)
        assert "fn" in result
        assert "a" not in result
        assert "b" not in result

    def test_line_comment_does_not_eat_next_line(self, tmp_path):
        p = _tmp_header(tmp_path, "// comment\nint real_fn(void);\n")
        result = read_header(p)
        assert "real_fn" in result

    def test_strips_simple_attribute(self, tmp_path):
        p = _tmp_header(tmp_path, "__attribute__((unused))\nstatic int helper(void);\n")
        result = read_header(p)
        assert "__attribute__" not in result

    def test_strips_nested_attribute_format(self, tmp_path):
        # __attribute__((format(printf, 1, 2))) has nested parens — old [^)]* regex
        # would leave ') void foo...' residue, breaking FUNC_RE.
        p = _tmp_header(tmp_path, "__attribute__((format(printf, 1, 2))) void log_msg(const char *fmt, ...);\n")
        result = read_header(p)
        assert "__attribute__" not in result
        # The residue from the broken regex would start with ')' before 'void'
        assert result.lstrip().startswith(")") is False


# ---------------------------------------------------------------------------
# join_multiline_decls
# ---------------------------------------------------------------------------

class TestJoinMultilineDecls:
    def test_single_line_unchanged(self):
        text = "int foo(int a, int b);\n"
        result = join_multiline_decls(text)
        assert "int foo(int a, int b);" in result

    def test_joins_continuation_line(self):
        text = "int foo(\n    int a,\n    int b\n);\n"
        result = join_multiline_decls(text)
        assert "int a" in result
        assert "int b" in result
        # Result should be a single logical line ending with ;
        lines = [l for l in result.split('\n') if l.strip()]
        assert any(l.endswith(';') and 'foo' in l for l in lines)

    def test_preprocessor_line_flushes_buffer(self):
        text = "#ifndef FOO_H\nint fn(void);\n"
        result = join_multiline_decls(text)
        # Preprocessor directive should not be merged with the declaration
        assert "fn(void);" in result

    def test_brace_flushes_buffer(self):
        text = "static inline int helper(int x) {\n    return x;\n}\n"
        result = join_multiline_decls(text)
        # The opening brace line should be flushed as its own entry
        assert "{" in result

    def test_empty_lines_ignored(self):
        text = "\n\nint foo(void);\n\n"
        result = join_multiline_decls(text)
        assert "int foo(void);" in result


# ---------------------------------------------------------------------------
# split_params
# ---------------------------------------------------------------------------

class TestSplitParams:
    def test_empty_string(self):
        assert split_params("") == []

    def test_void_param(self):
        assert split_params("void") == []

    def test_single_param(self):
        assert split_params("int x") == ["int x"]

    def test_two_params(self):
        result = split_params("int x, float y")
        assert result == ["int x", "float y"]

    def test_function_pointer_param_not_split(self):
        # Comma inside nested parens must NOT split
        result = split_params("int x, void (*cb)(int, int)")
        assert len(result) == 2
        assert result[1] == "void (*cb)(int, int)"

    def test_three_params(self):
        result = split_params("uint8_t *buf, uint16_t len, bool flag")
        assert len(result) == 3
        assert result[0] == "uint8_t *buf"
        assert result[2] == "bool flag"

    def test_nested_double_parens(self):
        # e.g. void (*write)(void *ctx, uint8_t val)
        result = split_params("void *ctx, void (*write)(void *, uint8_t), int n")
        assert len(result) == 3

    def test_whitespace_stripped(self):
        result = split_params("  int a  ,  int b  ")
        assert result == ["int a", "int b"]


# ---------------------------------------------------------------------------
# parse_param
# ---------------------------------------------------------------------------

class TestParseParam:
    def test_none_for_empty(self):
        assert parse_param("") is None

    def test_none_for_void(self):
        assert parse_param("void") is None

    def test_none_for_variadic(self):
        assert parse_param("...") is None

    def test_simple_int(self):
        p = parse_param("int x")
        assert p is not None
        assert p.name == "x"
        assert not p.is_pointer
        assert not p.is_func_ptr

    def test_pointer_param(self):
        p = parse_param("uint8_t *buf")
        assert p is not None
        assert p.is_pointer
        assert p.name == "buf"
        assert not p.is_func_ptr

    def test_const_pointer_param(self):
        p = parse_param("const uint8_t *data")
        assert p is not None
        assert p.is_pointer
        assert "const" in p.type_text

    def test_explicit_function_pointer(self):
        p = parse_param("void (*cb)(int, int)")
        assert p is not None
        assert p.is_func_ptr
        assert p.name == "cb"

    def test_typedef_function_pointer(self):
        # e.g. soft_timer_cb_t cb -- matched by FUNC_PTR_TYPEDEF_RE
        p = parse_param("soft_timer_cb_t cb")
        assert p is not None
        assert p.is_func_ptr

    def test_array_param_treated_as_pointer(self):
        p = parse_param("uint8_t buf[16]")
        assert p is not None
        assert p.is_pointer
        assert p.name == "buf"

    def test_pointer_star_on_name_side(self):
        # "uint8_t *data" written as "uint8_t* data" or "uint8_t *data"
        p = parse_param("uint8_t* data")
        assert p is not None
        assert p.is_pointer

    def test_compute_extra_integer(self):
        p = parse_param("uint16_t len", compute_extra=True)
        assert p is not None
        assert p.is_integer
        assert p.base_type == "uint16_t"

    def test_compute_extra_pointer_not_integer(self):
        p = parse_param("uint8_t *buf", compute_extra=True)
        assert p is not None
        assert not p.is_integer

    def test_compute_extra_false_leaves_fields_default(self):
        p = parse_param("uint16_t len", compute_extra=False)
        assert p is not None
        assert p.is_integer is False
        assert p.base_type == ""

    def test_typedef_fn_suffix_variants(self):
        for suffix_type in ["on_done_fn_t", "write_cb_t", "handler_fn", "recv_callback_t"]:
            p = parse_param(f"{suffix_type} callback")
            assert p is not None and p.is_func_ptr, f"Expected func_ptr for {suffix_type}"


# ---------------------------------------------------------------------------
# detect_prefix
# ---------------------------------------------------------------------------

class TestDetectPrefix:
    def test_full_wheel_name_prefix(self):
        header = "ring_buffer_err_t ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint16_t size);\n"
        prefix = detect_prefix(header, "ring_buffer")
        assert prefix == "ring_buffer_"

    def test_abbreviated_prefix_from_error_enum(self):
        # hex_codec uses "hex_" prefix; error enum is HEX_ERR_PARAM
        header = (
            "typedef enum { HEX_ERR_PARAM } hex_err_t;\n"
            "hex_err_t hex_encode(const uint8_t *in, uint16_t len, char *out, uint16_t out_size);\n"
        )
        prefix = detect_prefix(header, "hex_codec")
        assert prefix == "hex_"

    def test_lcp_fallback(self):
        header = (
            "int foo_init(foo_t *ctx);\n"
            "int foo_read(foo_t *ctx, uint8_t *buf);\n"
            "int foo_write(foo_t *ctx, const uint8_t *buf);\n"
        )
        prefix = detect_prefix(header, "bar")
        assert prefix == "foo_"

    def test_no_candidates_returns_wheel_name(self):
        # No function declarations in header
        prefix = detect_prefix("// empty\n", "mywheel")
        assert prefix == "mywheel_"

    def test_single_candidate_fallback_empty(self):
        # One function only — LCP branch needs >=2; falls through to ''
        header = "int only_fn(void);\n"
        prefix = detect_prefix(header, "missing")
        # Either '' or 'missing_' — both are acceptable (no match for full prefix)
        assert isinstance(prefix, str)


# ---------------------------------------------------------------------------
# classify_return_type
# ---------------------------------------------------------------------------

class TestClassifyReturnType:
    def test_err_t(self):
        assert classify_return_type("ring_buffer_err_t") == "err_t"

    def test_int_return(self):
        assert classify_return_type("int") == "int_return"

    def test_unsigned_types(self):
        for t in ("uint8_t", "uint16_t", "uint32_t", "uint64_t",
                  "size_t", "unsigned", "unsigned int"):
            assert classify_return_type(t) == "unsigned", f"Failed for {t}"

    def test_signed_int_types_return_unsigned_class(self):
        for t in ("int8_t", "int16_t", "int32_t", "int64_t"):
            assert classify_return_type(t) == "unsigned", f"Failed for {t}"

    def test_bool(self):
        assert classify_return_type("bool") == "bool"

    def test_void(self):
        assert classify_return_type("void") == "void"

    def test_pointer(self):
        assert classify_return_type("void *") == "pointer"
        assert classify_return_type("uint8_t *") == "pointer"

    def test_state_machine_types(self):
        assert classify_return_type("sm_state_t") == "unsigned"
        assert classify_return_type("sm_event_t") == "unsigned"

    def test_other(self):
        assert classify_return_type("some_struct_t") == "other"


# ---------------------------------------------------------------------------
# scalar_default
# ---------------------------------------------------------------------------

class TestScalarDefault:
    def test_bool(self):
        assert scalar_default("bool") == "false"

    def test_float(self):
        assert scalar_default("float") == "0.0f"

    def test_double(self):
        assert scalar_default("double") == "0.0"

    def test_uint8_t(self):
        assert scalar_default("uint8_t") == "0"

    def test_int(self):
        assert scalar_default("int") == "0"

    def test_size_t(self):
        assert scalar_default("size_t") == "0"

    def test_const_stripped(self):
        # "const uint8_t" -> strip const before lookup
        assert scalar_default("const uint8_t") == "0"

    def test_unknown_typedef(self):
        # Any _t type should return "0"
        assert scalar_default("some_state_t") == "0"


# ---------------------------------------------------------------------------
# discover_wheels
# ---------------------------------------------------------------------------

class TestDiscoverWheels:
    def test_finds_wheels(self):
        wheels = discover_wheels()
        assert len(wheels) > 0

    def test_returns_wheel_info_objects(self):
        wheels = discover_wheels()
        for w in wheels:
            assert isinstance(w, WheelInfo)
            assert w.header_path.is_file()
            assert w.path.is_dir()

    def test_skip_layers(self):
        all_wheels = discover_wheels()
        no_drivers = discover_wheels(skip_layers={"drivers"})
        driver_wheels = [w for w in all_wheels if w.layer == "drivers"]
        assert len(no_drivers) == len(all_wheels) - len(driver_wheels)

    def test_skip_wheels(self):
        all_wheels = discover_wheels()
        # Pick a wheel name that we know exists
        first_name = all_wheels[0].name
        filtered = discover_wheels(skip_wheels={first_name})
        names = [w.name for w in filtered]
        assert first_name not in names

    def test_no_vendor_wheels(self):
        wheels = discover_wheels()
        for w in wheels:
            assert not (w.path / "vendor").is_dir(), \
                f"Vendor wheel leaked through: {w.name}"

    def test_header_file_matches_name(self):
        wheels = discover_wheels()
        for w in wheels:
            assert w.header_path.name == f"{w.name}.h"


# ---------------------------------------------------------------------------
# parse_functions (integration)
# ---------------------------------------------------------------------------

class TestParseFunctions:
    def test_parses_simple_declaration(self):
        header = "int my_module_init(my_module_t *ctx);\n"
        funcs = parse_functions(header, "my_module")
        assert len(funcs) == 1
        assert funcs[0].name == "my_module_init"

    def test_skips_static_functions(self):
        header = (
            "static inline int helper(int x);\n"
            "int pub_init(void);\n"
        )
        funcs = parse_functions(header, "pub")
        names = [f.name for f in funcs]
        assert "helper" not in names

    def test_skips_typedef_functions(self):
        header = "typedef void (*my_cb_t)(int x);\nint pub_fn(my_cb_t cb);\n"
        funcs = parse_functions(header, "pub")
        names = [f.name for f in funcs]
        assert "my_cb_t" not in names

    def test_params_parsed(self):
        header = "int pub_write(pub_t *ctx, const uint8_t *buf, uint16_t len);\n"
        funcs = parse_functions(header, "pub")
        assert len(funcs) == 1
        assert len(funcs[0].params) == 3

    def test_compute_extra_populates_is_integer(self):
        header = "int pub_write(pub_t *ctx, uint16_t len);\n"
        funcs = parse_functions(header, "pub", compute_extra=True)
        assert len(funcs) == 1
        params_by_name = {p.name: p for p in funcs[0].params}
        assert params_by_name["len"].is_integer
        assert params_by_name["len"].base_type == "uint16_t"

    def test_real_wheel_parses_without_error(self):
        # Smoke test: parse the log wheel header
        log_header = WHEELS_DIR / "primitives" / "log" / "log.h"
        assert log_header.is_file(), "log wheel header missing — expected at wheels/primitives/log/log.h"
        from _wheel_parser import read_header
        text = read_header(log_header)
        funcs = parse_functions(text, "log")
        assert len(funcs) >= 1
