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


def run_all():
    global _pass, _fail
    for fn in _tests:
        aw._file_cache.clear()
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
def test_main_returns_zero_on_clean():
    """Smoke test: main() runs without crashing on real repo."""
    rc = aw.main(quiet=True, json_mode=False, fast=True, save=False)
    assert rc == 0, f"main() returned {rc}, expected 0"


if __name__ == "__main__":
    print(f"=== audit_wheels.py test suite ===\n")
    sys.exit(run_all())
