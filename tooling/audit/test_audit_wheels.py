#!/usr/bin/env python3
"""
test_audit_wheels.py — unit tests for audit_wheels.py check functions.

Creates temporary mock wheel directories and verifies each check
returns the expected result (pass/fail) for known inputs.
"""

import os
import sys
import tempfile
import shutil
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import audit_wheels as aw


class TmpWheel:
    """Helper to create a temporary wheel directory for testing."""

    def __init__(self, layer="primitives", name="test_wheel"):
        self.root = Path(tempfile.mkdtemp())
        self.layer = layer
        self.name = name
        self.dir = self.root / "wheels" / layer / name
        self.dir.mkdir(parents=True)

    def write_h(self, content):
        (self.dir / f"{self.name}.h").write_text(content, encoding="utf-8")

    def write_c(self, content):
        (self.dir / f"{self.name}.c").write_text(content, encoding="utf-8")

    def write_readme(self, content):
        (self.dir / "README.md").write_text(content, encoding="utf-8")

    def write_test_cmake(self, content):
        td = self.dir / "test"
        td.mkdir(exist_ok=True)
        (td / "CMakeLists.txt").write_text(content, encoding="utf-8")

    def make_vendor(self):
        (self.dir / "vendor").mkdir(exist_ok=True)

    def ctx(self):
        aw._file_cache.clear()
        return aw.WheelCtx(self.layer, self.name, self.dir)

    def cleanup(self):
        shutil.rmtree(self.root, ignore_errors=True)


# --- Test infrastructure ---

_pass = 0
_fail = 0
_tests = []


def test(fn):
    _tests.append(fn)
    return fn


test.__test__ = False   # the decorator itself: pytest must not collect it


def run_all():
    global _pass, _fail
    for fn in _tests:
        aw._file_cache.clear()
        w = None
        try:
            fn()
            _pass += 1
            print(f"  PASS  {fn.__name__}")
        except AssertionError as e:
            _fail += 1
            print(f"  FAIL  {fn.__name__}: {e}")
        except Exception as e:
            _fail += 1
            print(f"  ERR   {fn.__name__}: {type(e).__name__}: {e}")
        finally:
            # Clean up any TmpWheel instances left by failed tests
            import gc
            for obj in gc.get_objects():
                if isinstance(obj, TmpWheel) and hasattr(obj, 'root') and obj.root.exists():
                    obj.cleanup()
    print(f"\n{_pass + _fail} tests: {_pass} passed, {_fail} failed")
    return _fail


# === Check function tests ===

@test
def test_chk_extern_c_guard_pass():
    w = TmpWheel()
    w.write_h('#ifdef __cplusplus\nextern "C" {\n#endif\nvoid test_wheel_init(void);\n')
    assert aw.chk_extern_c_guard(w.ctx()) is None
    w.cleanup()


@test
def test_chk_extern_c_guard_fail():
    w = TmpWheel()
    w.write_h("void test_wheel_init(void);\n")
    result = aw.chk_extern_c_guard(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_extern_c_guard_no_header():
    w = TmpWheel()
    assert aw.chk_extern_c_guard(w.ctx()) is None
    w.cleanup()


@test
def test_chk_spdx_header_pass():
    w = TmpWheel()
    w.write_h("// SPDX-License-Identifier: MIT\nvoid test_wheel_init(void);\n")
    assert aw.chk_spdx_header(w.ctx()) is None
    w.cleanup()


@test
def test_chk_spdx_header_fail():
    w = TmpWheel()
    w.write_h("void test_wheel_init(void);\n")
    result = aw.chk_spdx_header(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_pragma_once_pass():
    w = TmpWheel()
    w.write_h("#ifndef TEST_WHEEL_H\n#define TEST_WHEEL_H\n#endif\n")
    assert aw.chk_pragma_once(w.ctx()) is None
    w.cleanup()


@test
def test_chk_pragma_once_fail():
    w = TmpWheel()
    w.write_h("#pragma once\nvoid test_wheel_init(void);\n")
    result = aw.chk_pragma_once(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_thread_safe_pass():
    w = TmpWheel()
    w.write_h("/** Init. @thread_safe: no */\nvoid test_wheel_init(void);\n")
    assert aw.chk_thread_safe(w.ctx()) is None
    w.cleanup()


@test
def test_chk_thread_safe_fail():
    w = TmpWheel()
    w.write_h("void test_wheel_init(void);\n")
    result = aw.chk_thread_safe(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_dev_t_naming_pass():
    w = TmpWheel()
    w.write_h("typedef struct { int x; } test_wheel_t;\n")
    assert aw.chk_dev_t_naming(w.ctx()) is None
    w.cleanup()


@test
def test_chk_dev_t_naming_fail():
    w = TmpWheel()
    w.write_h("typedef struct { int x; } test_wheel_dev_t;\n")
    result = aw.chk_dev_t_naming(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_err_plural_pass():
    w = TmpWheel()
    w.write_h("TEST_WHEEL_ERR_PARAM = -1,\n")
    assert aw.chk_err_plural(w.ctx()) is None
    w.cleanup()


@test
def test_chk_err_plural_fail():
    w = TmpWheel()
    w.write_h("TEST_WHEEL_ERR_PARAMS = -1,\n")
    result = aw.chk_err_plural(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_status_format_pass():
    w = TmpWheel()
    w.write_readme("# test_wheel\n\n**Status: candidate**\n")
    assert aw.chk_status_format(w.ctx()) is None
    w.cleanup()


@test
def test_chk_status_format_fail():
    w = TmpWheel()
    w.write_readme("# test_wheel\n\n**STATUS: candidate**\n")
    result = aw.chk_status_format(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_why_self_written_pass():
    w = TmpWheel()
    w.write_readme("## Why self-written?\n\nBecause reasons.\n")
    assert aw.chk_why_self_written(w.ctx()) is None
    w.cleanup()


@test
def test_chk_why_self_written_fail():
    w = TmpWheel()
    w.write_readme("# test_wheel\n\nNo why section.\n")
    result = aw.chk_why_self_written(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_why_self_written_vendor_skip():
    w = TmpWheel()
    w.make_vendor()
    w.write_readme("# test_wheel\n\nVendor wheel.\n")
    assert aw.chk_why_self_written(w.ctx()) is None
    w.cleanup()


@test
def test_chk_file_header_pass():
    w = TmpWheel()
    w.write_h("/*\n * test_wheel\n * Layer: L0\n * Status: candidate\n */\n")
    assert aw.chk_file_header(w.ctx()) is None
    w.cleanup()


@test
def test_chk_file_header_fail():
    w = TmpWheel()
    w.write_h("// no file header\nvoid test_wheel_init(void);\n")
    result = aw.chk_file_header(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_ctest_registration_pass():
    w = TmpWheel()
    w.write_test_cmake("enable_testing()\nadd_test(NAME t COMMAND t)\n")
    assert aw.chk_ctest_registration(w.ctx()) is None
    w.cleanup()


@test
def test_chk_ctest_registration_fail():
    w = TmpWheel()
    w.write_test_cmake("add_executable(test_x test.c)\n")
    result = aw.chk_ctest_registration(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_werror_pass():
    w = TmpWheel()
    w.write_test_cmake("-Wall -Werror\n")
    assert aw.chk_werror(w.ctx()) is None
    w.cleanup()


@test
def test_chk_werror_fail():
    w = TmpWheel()
    w.write_test_cmake("-Wall -Wextra\n")
    result = aw.chk_werror(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_werror_every_target():
    w = TmpWheel()
    w.write_test_cmake("add_executable(test_a a.c)\n"
                       "target_compile_options(test_a PRIVATE -Wall -Werror)\n"
                       "add_executable(test_b b.c)\n"
                       "target_compile_options(test_b PRIVATE -Wall)\n"
                       "add_executable(test_c c.c)\n")
    result = aw.chk_werror(w.ctx())
    assert result is not None
    assert "test_b" in result[0] and "test_c" in result[0]
    assert "test_a" not in result[0]
    w.cleanup()


@test
def test_chk_werror_flags_variable():
    w = TmpWheel()
    w.write_test_cmake("set(STRICT_FLAGS -Wall -Werror)\n"
                       "add_executable(test_a a.c)\n"
                       "target_compile_options(test_a PRIVATE ${STRICT_FLAGS})\n")
    assert aw.chk_werror(w.ctx()) is None
    w.cleanup()


@test
def test_chk_werror_function_target():
    w = TmpWheel()
    w.write_test_cmake("function(add_max name)\n"
                       "    add_executable(${name} a.c)\n"
                       "    target_compile_options(${name} PRIVATE -Wall -Werror)\n"
                       "endfunction()\n")
    assert aw.chk_werror(w.ctx()) is None
    w.cleanup()


@test
def test_chk_werror_global_options():
    w = TmpWheel()
    w.write_test_cmake("add_compile_options(-Wall -Werror)\n"
                       "add_executable(test_a a.c)\n")
    assert aw.chk_werror(w.ctx()) is None
    w.cleanup()


@test
def test_chk_werror_comment_does_not_count():
    w = TmpWheel()
    w.write_test_cmake("# -Werror\n"
                       "add_executable(test_a a.c)\n"
                       "target_compile_options(test_a PRIVATE -Wall)\n")
    assert aw.chk_werror(w.ctx()) is not None
    w.cleanup()


@test
def test_chk_werror_single_diagnostic_does_not_count():
    w = TmpWheel()
    w.write_test_cmake("add_executable(test_a a.c)\n"
                       "target_compile_options(test_a PRIVATE -Wall -Werror=format)\n")
    assert aw.chk_werror(w.ctx()) is not None
    w.cleanup()


@test
def test_chk_c99_pass():
    w = TmpWheel()
    w.write_test_cmake("set(CMAKE_C_STANDARD 99)\n")
    assert aw.chk_c99_standard(w.ctx()) is None
    w.cleanup()


@test
def test_chk_c99_wrong_value():
    w = TmpWheel()
    w.write_test_cmake("set(CMAKE_C_STANDARD 11)\n")
    result = aw.chk_c99_standard(w.ctx())
    assert result is not None and "11" in result[0]
    w.cleanup()


@test
def test_chk_c99_last_set_wins():
    w = TmpWheel()
    w.write_test_cmake("set(CMAKE_C_STANDARD 99)\nset(CMAKE_C_STANDARD 11)\n")
    assert aw.chk_c99_standard(w.ctx()) is not None
    w.cleanup()


@test
def test_chk_c99_comment_does_not_count():
    w = TmpWheel()
    w.write_test_cmake("# CMAKE_C_STANDARD set as a variable, on purpose\n")
    assert aw.chk_c99_standard(w.ctx()) is not None
    w.cleanup()


@test
def test_c99_and_werror_block_ci():
    levels = {row[0]: row[3] for row in aw.CHECK_TABLE}
    assert levels["c99_standard"] == "FAIL"
    assert levels["werror"] == "FAIL"


@test
def test_chk_isr_safe_pass():
    w = TmpWheel()
    w.write_h("/** Push. @thread_safe: yes @isr_safe: yes */\nvoid test_wheel_push(void);\n")
    assert aw.chk_isr_safe(w.ctx()) is None
    w.cleanup()


@test
def test_chk_isr_safe_fail():
    w = TmpWheel()
    w.write_h("/** Push. @thread_safe: yes */\nvoid test_wheel_push(void);\n")
    result = aw.chk_isr_safe(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_vendor_md_pass():
    w = TmpWheel()
    w.make_vendor()
    (w.dir / "VENDOR.md").write_text("upstream: github.com/x\n")
    assert aw.chk_vendor_md(w.ctx()) is None
    w.cleanup()


@test
def test_chk_vendor_md_fail():
    w = TmpWheel()
    w.make_vendor()
    result = aw.chk_vendor_md(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_vendor_md_not_vendor():
    w = TmpWheel()
    assert aw.chk_vendor_md(w.ctx()) is None
    w.cleanup()


@test
def test_wheel_ctx_caches_content():
    w = TmpWheel()
    w.write_h("// header\n")
    w.write_c("// source\n")
    w.write_readme("# readme\n")
    ctx = w.ctx()
    assert "// header" in ctx.header
    assert "// source" in ctx.source
    assert "# readme" in ctx.readme
    assert ctx.is_vendor is False
    assert ctx.is_draft is False
    w.cleanup()


@test
def test_wheel_ctx_missing_files():
    w = TmpWheel()
    ctx = w.ctx()
    assert ctx.header == ""
    assert ctx.source == ""
    assert ctx.readme == ""
    w.cleanup()


@test
def test_chk_readme_section_order_pass():
    w = TmpWheel()
    w.write_readme("## Origin\n\n## Dependencies\n\n## API\n\n## Responsible human\n")
    assert aw.chk_readme_section_order(w.ctx()) is None
    w.cleanup()


@test
def test_chk_readme_section_order_fail():
    w = TmpWheel()
    w.write_readme("## Responsible human\n\n## Origin\n\n## API\n")
    result = aw.chk_readme_section_order(w.ctx())
    assert result is not None
    w.cleanup()


@test
def test_chk_header_self_contained_pass():
    w = TmpWheel()
    w.write_h("#include <stdint.h>\n#include <stdbool.h>\ntypedef struct { uint8_t x; bool y; } test_wheel_t;\n")
    assert aw.chk_header_self_contained(w.ctx()) is None
    w.cleanup()


@test
def test_chk_header_self_contained_fail():
    w = TmpWheel()
    w.write_h("typedef struct { uint8_t x; } test_wheel_t;\n")
    result = aw.chk_header_self_contained(w.ctx())
    assert result is not None
    assert "stdint.h" in result[0]
    w.cleanup()


@test
def test_chk_header_self_contained_null_in_comment():
    w = TmpWheel()
    w.write_h("#include <stdint.h>\n/** @param ptr May be NULL. */\nvoid test_wheel_init(uint8_t x);\n")
    assert aw.chk_header_self_contained(w.ctx()) is None
    w.cleanup()


@test
def test_chk_header_self_contained_no_header():
    w = TmpWheel()
    assert aw.chk_header_self_contained(w.ctx()) is None
    w.cleanup()


@test
def test_chk_dead_static_function_pass():
    w = TmpWheel()
    w.write_c("static int helper(int x) { return x + 1; }\nint test_wheel_init(void) { return helper(0); }\n")
    assert aw.chk_dead_static_function(w.ctx()) is None
    w.cleanup()


@test
def test_chk_dead_static_function_fail():
    w = TmpWheel()
    w.write_c("static int dead_func(void) { return 42; }\nint test_wheel_init(void) { return 0; }\n")
    result = aw.chk_dead_static_function(w.ctx())
    assert result is not None
    assert "dead_func" in result[0]
    w.cleanup()


@test
def test_chk_dead_static_function_no_source():
    w = TmpWheel()
    assert aw.chk_dead_static_function(w.ctx()) is None
    w.cleanup()


@test
def test_chk_dead_static_function_callback_use():
    w = TmpWheel()
    w.write_c("static void handler(void) { }\nvoid (*cb)(void) = handler;\n")
    assert aw.chk_dead_static_function(w.ctx()) is None
    w.cleanup()


@test
def test_chk_switch_default_pass():
    w = TmpWheel()
    w.write_c("void f(int x) { switch(x) { case 0: break; default: break; } }\n")
    assert aw.chk_switch_default(w.ctx()) is None
    w.cleanup()


@test
def test_chk_switch_default_fail():
    w = TmpWheel()
    w.write_c("void f(int x) {\n  switch(x) {\n  case 0: break;\n  case 1: break;\n  }\n}\n")
    result = aw.chk_switch_default(w.ctx())
    assert result is not None
    assert "switch without default" in result[0]
    w.cleanup()


@test
def test_chk_switch_default_no_source():
    w = TmpWheel()
    assert aw.chk_switch_default(w.ctx()) is None
    w.cleanup()


@test
def test_chk_dep_direction_pass():
    w = TmpWheel(layer="drivers")
    w.write_test_cmake("add_subdirectory(../../../primitives/log/ build_log)\n")
    assert aw.chk_dep_direction(w.ctx()) is None
    w.cleanup()


@test
def test_chk_dep_direction_fail():
    w = TmpWheel(layer="primitives")
    w.write_test_cmake("add_subdirectory(../../../protocols/mqtt_packet/ build_mqtt)\n")
    result = aw.chk_dep_direction(w.ctx())
    assert result is not None
    assert "mqtt_packet" in result[0]
    assert "ADR-009" in result[0]
    w.cleanup()


@test
def test_chk_dep_direction_same_layer():
    w = TmpWheel(layer="primitives")
    w.write_test_cmake("add_subdirectory(../../../primitives/ring_buffer/ build_rb)\n")
    assert aw.chk_dep_direction(w.ctx()) is None
    w.cleanup()


@test
def test_chk_dep_direction_no_cmake():
    w = TmpWheel()
    assert aw.chk_dep_direction(w.ctx()) is None
    w.cleanup()


@test
def test_main_returns_zero_on_clean():
    """Smoke test: main() runs without crashing on real repo."""
    rc = aw.main(quiet=True, json_mode=False, fast=True, save=False)
    assert rc == 0, f"main() returned {rc}, expected 0"


if __name__ == "__main__":
    print(f"=== audit_wheels.py test suite ===\n")
    sys.exit(run_all())
