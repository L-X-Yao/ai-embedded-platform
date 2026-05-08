#!/usr/bin/env python3
"""
audit_wheels.py — 52-check automated audit covering structure, API, code safety, and consistency.

Codifies feedback_wheel_commit_not_sunk.md as a CI guardrail. The orphan
problem we hit twice (w25qxx, qmi8658a both committed but never wired)
gets caught here automatically — no reliance on humans/AI remembering
the three-step sink checklist.

Rules:
  - Every wheel under wheels/<layer>/<name>/ MUST have a CI job entry
    mentioning its path in .github/workflows/ci.yml.
  - Wheels under drivers/, protocols/, modules/ SHOULD have at least one
    project consumer (CMakeLists.txt or #include reference in projects/).
    Missing consumer is WARN not FAIL — proactive building is encouraged
    (see feedback_no_human_cost_model.md). CI presence remains hard FAIL.
  - Wheels under primitives/, _draft/ MAY be unconsumed (warn, don't
    fail) — primitives are general building blocks, drafts are
    incubating.
  - Names listed in wheels/WISHLIST.md MUST NOT correspond to existing
    wheels (a stale candidate is finished work that wasn't pruned).
  - Every wheel README MUST have a "Why not VSF native?" section
    (ADR-006 / CLAUDE.md zero-rule).
  - Vendor wheels (those with vendor/ subdir) MUST have VENDOR.md
    documenting upstream origin (ADR-010).
  - Wheels with cross-wheel dependencies (detected via test CMakeLists)
    SHOULD document them in a "Dependencies" README section (ADR-009).
  - Non-vendor wheel .h/.c files SHOULD have the Layer/Status/Deps file
    header per api-conventions §8.
  - Non-draft wheel READMEs SHOULD have candidate-level sections:
    Supported platforms, Test status, Dependencies (wheel-contract §3.2).

Exit 0 if all checks pass; 1 if any failure.
"""

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
CI_YAML = REPO / ".github" / "workflows" / "ci.yml"
WISHLIST = WHEELS / "WISHLIST.md"
PROJECTS = REPO / "projects"

CONSUMER_REQUIRED = {"drivers", "protocols", "modules"}
KNOWN_EXCEPTIONS_FILE = REPO / "tooling" / "audit" / "known_exceptions.json"

from _audit_build_base import find_wheels as _find_wheels_base


def _load_known_exceptions() -> dict:
    if not KNOWN_EXCEPTIONS_FILE.exists():
        return {}
    try:
        data = json.loads(KNOWN_EXCEPTIONS_FILE.read_text(encoding="utf-8"))
        idx = {}
        for exc in data.get("exceptions", []):
            key = (exc.get("wheel", ""), exc.get("check", ""))
            idx[key] = exc.get("reason", "")
        return idx
    except (json.JSONDecodeError, OSError):
        return {}

# --- File cache: eliminate ~6000 redundant IO reads ---

_file_cache: dict[str, str] = {}


def _cached_read(path: Path) -> str:
    """Read file contents with caching. Returns '' for missing/unreadable files."""
    key = str(path)
    if key not in _file_cache:
        if not path.exists():
            _file_cache[key] = ""
        else:
            try:
                _file_cache[key] = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                _file_cache[key] = ""
    return _file_cache[key]


class WheelCtx:
    """Pre-loaded file contents for a single wheel."""
    __slots__ = ("layer", "name", "path", "dir", "readme", "header", "source",
                 "test_cmake", "is_vendor", "is_draft", "headings", "header_lines")

    def __init__(self, layer: str, name: str, wheel_dir: Path):
        self.layer = layer
        self.name = name
        self.path = f"wheels/{layer}/{name}"
        self.dir = wheel_dir
        self.readme = _cached_read(wheel_dir / "README.md")
        self.header = _cached_read(wheel_dir / f"{name}.h")
        self.source = _cached_read(wheel_dir / f"{name}.c")
        self.test_cmake = _cached_read(wheel_dir / "test" / "CMakeLists.txt")
        if not self.test_cmake:
            self.test_cmake = _cached_read(wheel_dir / "CMakeLists.txt")
        self.is_vendor = (wheel_dir / "vendor").is_dir()
        self.is_draft = layer == "_draft"
        self.headings = re.findall(r'^## (.+)$', self.readme, re.MULTILINE)
        self.header_lines = self.header.split("\n") if self.header else []


# --- Unified check functions (accept WheelCtx, return result or None) ---
# Each returns None (pass) or (message, fix_cmd) tuple.

def chk_ci_job(ctx, *, ci_text, **_):
    if ctx.path in ci_text:
        return None
    if "wheels/*/*/test/CMakeLists.txt" in ci_text and (ctx.dir / "test" / "CMakeLists.txt").exists():
        return None
    return (f"missing CI job — no mention of '{ctx.path}' in ci.yml",
            f"add '{ctx.path}' to .github/workflows/ci.yml wheel-test matrix")


def chk_vendor_md(ctx, **_):
    if not ctx.is_vendor:
        return None
    if (ctx.dir / "VENDOR.md").exists():
        return None
    return ("has vendor/ but no VENDOR.md",
            f"create {ctx.path}/VENDOR.md documenting upstream origin (ADR-010)")


def chk_cross_wheel_deps(ctx, **_):
    pat = re.compile(r"\.\./\.\./\.\./(primitives|drivers|protocols|modules)/([a-z0-9_]+)/")
    deps = list({m[1] for m in pat.findall(ctx.test_cmake)})
    if not deps:
        return None
    if "## Dependencies" in ctx.readme:
        return None
    return (f"depends on {', '.join(deps)} but README has no Dependencies section",
            f"add '## Dependencies' section to {ctx.path}/README.md")


def chk_api_review(ctx, **_):
    if "## API Review" in ctx.readme or "## API review" in ctx.readme:
        return None
    return ("README missing API review section",
            f"add '## API Review' section to {ctx.path}/README.md")


def chk_ctest_registration(ctx, **_):
    if not ctx.test_cmake:
        return None
    if "enable_testing()" in ctx.test_cmake and "add_test(" in ctx.test_cmake:
        return None
    return ("test CMakeLists.txt missing enable_testing()/add_test()",
            f"add enable_testing() + add_test() to {ctx.path}/test/CMakeLists.txt")


def chk_c99_standard(ctx, **_):
    if not ctx.test_cmake or "vsf.cmake" in ctx.test_cmake:
        return None
    if "CMAKE_C_STANDARD" in ctx.test_cmake:
        return None
    return ("test CMakeLists.txt missing CMAKE_C_STANDARD 99",
            f"add set(CMAKE_C_STANDARD 99) to {ctx.path}/test/CMakeLists.txt")


def chk_candidate_sections(ctx, **_):
    required = [
        ("Supported platforms", "支持的平台"),
        ("Test status", "测试状态"),
        ("Dependencies", "依赖"),
        ("How to test", "如何测试"),
        ("Known issues", "已知坑", "已知问题"),
        ("Origin", "来源"),
    ]
    missing = []
    for names in required:
        if not any(f"## {n}" in ctx.readme for n in names):
            missing.append(names[0])
    if not missing:
        return None
    return (f"README missing candidate sections: {', '.join(missing)}",
            f"add sections to {ctx.path}/README.md: {', '.join(missing)}")


def chk_file_header(ctx, **_):
    for text in (ctx.header, ctx.source):
        if not text:
            continue
        top = "\n".join(text.split("\n")[:10])
        if "Layer:" not in top or "Status:" not in top:
            return (".h/.c missing Layer/Status/Deps file header",
                    f"add header to {ctx.path}/{ctx.name}.h/.c (api-conventions §8)")
    return None


def chk_extern_c_guard(ctx, **_):
    if not ctx.header or 'extern "C"' in ctx.header:
        return None
    return ('.h missing extern "C" guard',
            f'add extern "C" guard to {ctx.path}/{ctx.name}.h')


def chk_thread_safe(ctx, **_):
    if not ctx.header or "@thread_safe" in ctx.header:
        return None
    return (".h missing @thread_safe annotations",
            "make audit-fix-apply --only doc_comments")


def chk_test_exit_code(ctx, **_):
    for glob_pat in ["test/test_*.c", "test_*.c"]:
        for src in ctx.dir.glob(glob_pat):
            text = _cached_read(src)
            if 'test_harness.h' in text:
                return None
            if "return" in text and any(p in text for p in ["tests_fail", "g_failures", "failures", "TEST_MAIN_END"]):
                return None
    if list(ctx.dir.glob("test_*.py")) or list(ctx.dir.glob("test/test_*.py")):
        return None
    return ("test harness may not return non-zero on failure",
            f'#include "test_harness.h" and use TEST_MAIN_BEGIN/END in {ctx.path}/test/')


def chk_balanced_test_run(ctx, **_):
    for glob_pat in ["test/test_*.c", "test_*.c"]:
        for src in ctx.dir.glob(glob_pat):
            text = _cached_read(src)
            if "test_harness.h" not in text:
                continue
            tests = set(re.findall(r'^TEST\((\w+)\)', text, re.MULTILINE))
            runs = set(re.findall(r'RUN\((\w+)\)', text))
            unrun = tests - runs
            if unrun:
                return (f"{len(tests)} TEST() but only {len(runs)} RUN() — orphan test functions",
                        f"add missing RUN() calls in {ctx.path}/test/")
    return None


def chk_werror(ctx, **_):
    if not ctx.test_cmake or "-Wall" not in ctx.test_cmake:
        return None
    if "-Werror" in ctx.test_cmake:
        return None
    return ("test CMakeLists.txt missing -Werror",
            f"add -Werror to {ctx.path}/test/CMakeLists.txt")


def chk_wconversion(ctx, **_):
    if not ctx.test_cmake or "-Wall" not in ctx.test_cmake:
        return None
    if "-Wconversion" in ctx.test_cmake:
        return None
    return ("test CMakeLists.txt missing -Wconversion",
            f"add -Wconversion to {ctx.path}/test/CMakeLists.txt")


def chk_responsible_human(ctx, **_):
    if "## Responsible human" in ctx.readme:
        return None
    return ("README missing '## Responsible human' section",
            f"add '## Responsible human' to {ctx.path}/README.md")


def chk_deviation_heading(ctx, **_):
    if "## Convention Deviations" not in ctx.readme:
        return None
    return ("use '## API convention deviations' not '## Convention Deviations'",
            f"rename heading in {ctx.path}/README.md")


def chk_error_code_sign(ctx, **_):
    if not has_negative_error_codes(ctx.dir, ctx.name):
        return ("error enum uses auto-increment (should be explicit negative)",
                "python3 tooling/audit/audit_fix.py --apply")
    return None


def chk_function_doc(ctx, **_):
    if not has_function_doc_all(ctx.dir, ctx.name):
        return ("some functions lack doc comment before declaration",
                "python3 tooling/audit/audit_fix.py --apply")
    return None


def chk_error_code_coverage(ctx, **_):
    uncovered = has_error_code_test_coverage(ctx.dir, ctx.name)
    if not uncovered:
        return None
    return (f"untested error codes: {', '.join(uncovered)}",
            f"add test cases covering {', '.join(uncovered)} in {ctx.path}/test/")


def chk_naming_convention(ctx, **_):
    if not has_naming_convention(ctx.dir, ctx.name):
        return (f"public functions not following {ctx.name}_<verb> naming pattern",
                f"rename public functions in {ctx.path}/{ctx.name}.h")
    return None


def chk_hal_injection(ctx, **_):
    if not has_consistent_hal_pattern(ctx.dir, ctx.name):
        return ("L1 driver missing HAL struct with function pointers",
                f"add HAL struct to {ctx.path}/{ctx.name}.h")
    return None


def chk_busy_wait_timeout(ctx, **_):
    unguarded = has_busy_wait_timeout(ctx.dir)
    if not unguarded:
        return None
    locs = "; ".join(f"{f}:{ln}" for f, ln, _ in unguarded)
    return (f"busy-wait without timeout guard at {locs}",
            f"add timeout counter in {ctx.path}")


def chk_dev_t_naming(ctx, **_):
    if f"{ctx.name}_dev_t" not in ctx.header:
        return None
    return (f"uses {ctx.name}_dev_t — should be {ctx.name}_t (api-conventions §1.3)",
            f"rename in {ctx.path}/{ctx.name}.h/.c")


def chk_err_plural(ctx, **_):
    if "ERR_PARAMS" not in ctx.header:
        return None
    return ("uses ERR_PARAMS (plural) — should be ERR_PARAM",
            f"rename in {ctx.path}/{ctx.name}.h/.c")


def chk_pragma_once(ctx, **_):
    if "#pragma once" not in ctx.header:
        return None
    return ("uses #pragma once — should use #ifndef guard (api-conventions §9.2)",
            f"replace in {ctx.path}/{ctx.name}.h")


def chk_spdx_header(ctx, **_):
    if not ctx.header or "SPDX-License-Identifier" in ctx.header:
        return None
    return (".h missing SPDX-License-Identifier line",
            f"add to {ctx.path}/{ctx.name}.h")


def chk_vsf_native_residual(ctx, **_):
    if "why not vsf native" not in ctx.readme.lower():
        return None
    return ("README still has 'Why not VSF native?' — ADR-006 superseded by ADR-011",
            f"remove from {ctx.path}/README.md")


def chk_init_caller_must_lock(ctx, **_):
    if has_init_caller_must_lock(ctx.dir, ctx.name):
        return ("init/deinit/reset tagged caller_must_lock — should be 'no'",
                f"change in {ctx.path}/{ctx.name}.h")
    return None


def chk_deps_format(ctx, **_):
    issue = has_deps_format_issue(ctx.dir, ctx.name)
    if not issue:
        return None
    return (f"file header {issue}",
            f"fix Deps: line in {ctx.path}/{ctx.name}.h/.c")


def chk_why_self_written(ctx, **_):
    if ctx.is_vendor:
        return None
    text_l = ctx.readme.lower()
    if ("why self-written" in text_l or "why not an open-source" in text_l or
            "why not open-source" in text_l or "why this upstream" in text_l):
        return None
    return ("README missing 'Why self-written?' section (ADR-010)",
            f"add to {ctx.path}/README.md")


def chk_status_format(ctx, **_):
    if "**STATUS:" not in ctx.readme:
        return None
    return ("README uses **STATUS:** — should be **Status:** (title case)",
            f"change in {ctx.path}/README.md")


def chk_instances_field(ctx, **_):
    if not has_instances_field(ctx.dir, ctx.name):
        return ("file header missing Instances: multi|single field",
                f"add to {ctx.path}/{ctx.name}.h")
    return None


def chk_isr_safe(ctx, **_):
    if not ctx.header or "@isr_safe" in ctx.header:
        return None
    return (".h missing @isr_safe annotation (api-conventions §4.1)",
            f"add @isr_safe to {ctx.path}/{ctx.name}.h")


def chk_config_h_placement(ctx, **_):
    issue = has_config_h_placement(ctx.dir, ctx.name)
    if not issue:
        return None
    return (issue, f"move config.h include from .h to .c in {ctx.path}/")


def chk_nan_guard(ctx, **_):
    violations = has_nan_guard_in_float_api(ctx.dir, ctx.name)
    if not violations:
        return None
    locs = "; ".join(f"L{ln}" for ln, _ in violations[:3])
    return (f"vulnerable float range check at {locs}",
            f"use !(val >= min && val <= max) in {ctx.path}/{ctx.name}.c")


def chk_deinit_safety(ctx, **_):
    if has_deinit_null_check(ctx.dir, ctx.name):
        return None
    return ("deinit function may not check for NULL pointer",
            f"add NULL check to {ctx.name}_deinit in {ctx.path}/{ctx.name}.c")


def chk_readme_section_order(ctx, **_):
    violations = has_readme_section_order(ctx.dir)
    if not violations:
        return None
    return (f"README sections out of order: {', '.join(violations)}",
            f"reorder in {ctx.path}/README.md per wheel-contract §3.2")


def chk_readme_section_names(ctx, **_):
    violations = has_readme_section_names(ctx.dir)
    if not violations:
        return None
    return (f"non-standard section names: {'; '.join(violations)}",
            f"rename sections in {ctx.path}/README.md")


def chk_readme_todo_mismatch(ctx, **_):
    """Detect README claiming 'no known issues' while code has TODO/FIXME."""
    readme_l = ctx.readme.lower()
    if "known issues" not in readme_l:
        return None
    section_start = readme_l.find("known issues")
    next_section = readme_l.find("\n##", section_start + 1)
    section_text = readme_l[section_start:next_section if next_section != -1 else section_start + 500]
    claims_none = "none" in section_text or "no known" in readme_l
    if not claims_none:
        return None
    if any(tag in ctx.source for tag in ("TODO", "FIXME", "HACK", "XXX")):
        return ("README says no known issues but code has TODO/FIXME",
                f"update Known issues in {ctx.path}/README.md or remove TODO from code")
    return None


def chk_instances_accuracy(ctx, **_):
    """Detect Instances: single without static/global state in .c."""
    top = "\n".join(ctx.header.split("\n")[:15])
    if "Instances: single" not in top:
        return None
    if re.search(r'\bstatic\s+(?!inline\b|const\b)\w+\s+\w+\s*[\[;=]', ctx.source, re.MULTILINE):
        return None
    if re.search(r'\bglobal\b', ctx.source.lower()):
        return None
    return ("Instances: single but no static/global state found in .c",
            f"verify Instances field in {ctx.path}/{ctx.name}.h (should it be multi?)")


def chk_signed_left_shift(ctx, **_):
    """Detect signed integer left shift (C99 UB for negative values).

    Excludes lines where the shift operand is already cast to unsigned.
    """
    if not ctx.source:
        return None
    decl_pat = re.compile(r'\bint(?:8|16|32|64)_t\s+\w+.*<<')
    hits = []
    for i, line in enumerate(ctx.source.splitlines()):
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        # Strip trailing line comment before searching, to avoid false positives
        # from patterns like: int32_t x = 1; // shift << is 2
        code_part = re.sub(r'//.*$', '', line)
        if not decl_pat.search(code_part):
            continue
        if "<<" not in code_part:
            continue
        before_shift = code_part[:code_part.index("<<")]
        if re.search(r'\(uint\d+_t\)\s*$', before_shift.rstrip()):
            continue
        if "(uint" in before_shift and "<<" not in before_shift.split("(uint")[0]:
            continue
        hits.append(i + 1)
    if not hits:
        return None
    locs = ", ".join(f"L{n}" for n in hits[:3])
    return (f"signed left shift (C99 UB) at {locs}",
            f"cast to unsigned before shift in {ctx.path}/{ctx.name}.c")


def chk_float_to_int_cast(ctx, **_):
    """Detect float-to-int cast without range check.

    Only flags casts where the variable is declared as float/double
    in the preceding 20 lines and no ternary/clamp guard is present.
    """
    if not ctx.source:
        return None
    cast_pat = re.compile(r'\(u?int(?:8|16|32)_t\)\s*(\w+)')
    clamp_pat = re.compile(r'<=\s*0|>=\s*\d|<\s*0|>\s*\d|\?\s*0\b|\?\s*\d')
    lines = ctx.source.splitlines()
    hits = []
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        m = cast_pat.search(stripped)
        if not m:
            continue
        var_name = m.group(1)
        context = "\n".join(lines[max(0, i - 20):i + 1])
        is_float_var = re.search(
            rf'\b(?:float|double)\s+\*?\s*{re.escape(var_name)}\b', context)
        is_float_literal = re.search(
            rf'\b{re.escape(var_name)}\s*[=]\s*.*\d+\.\d+f?\b', context)
        if not is_float_var and not is_float_literal:
            continue
        nearby = "\n".join(lines[max(0, i - 3):i + 1])
        if clamp_pat.search(nearby):
            continue
        hits.append(i + 1)
    if not hits:
        return None
    locs = ", ".join(f"L{n}" for n in hits[:3])
    return (f"float-to-int cast without range check at {locs}",
            f"add range check before cast in {ctx.path}/{ctx.name}.c")


def chk_strlen_truncation(ctx, **_):
    """Detect strlen result truncated to narrow type."""
    if not ctx.source:
        return None
    pat = re.compile(r'\(uint(?:8|16)_t\)\s*strlen')
    for i, line in enumerate(ctx.source.splitlines()):
        if pat.search(line):
            return (f"strlen truncated to narrow type at L{i+1}",
                    f"use size_t or add length check in {ctx.path}/{ctx.name}.c")
    return None


def chk_memcpy_overflow(ctx, **_):
    """Detect potential size_t addition overflow before memcpy."""
    if not ctx.source:
        return None
    pat = re.compile(r'memcpy\([^,]+,\s*[^,]+,\s*\w+\s*\+\s*\w+\)')
    for i, line in enumerate(ctx.source.splitlines()):
        if pat.search(line):
            return (f"potential size_t add overflow before memcpy at L{i+1}",
                    f"validate sum before memcpy in {ctx.path}/{ctx.name}.c")
    return None


def chk_unchecked_return(ctx, **_):
    """Detect function calls whose error return is discarded.

    Only flags calls to the wheel's own public API prefix (not static helpers).
    """
    if not ctx.source:
        return None
    prefix = ctx.name + "_"
    static_funcs = set(re.findall(r'static\s+\w+\s+(\w+)\s*\(', ctx.source))
    pat = re.compile(rf'\b({re.escape(prefix)}\w+)\s*\([^)]*\)\s*;')
    hits = []
    for i, line in enumerate(ctx.source.splitlines()):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("return"):
            continue
        if stripped.startswith("(void)"):
            continue
        m = pat.search(stripped)
        if not m:
            continue
        func = m.group(1)
        if func in static_funcs:
            continue
        if "=" not in stripped.split("(")[0]:
            hits.append(i + 1)
    if not hits:
        return None
    locs = ", ".join(f"L{n}" for n in hits[:3])
    return (f"unchecked error return at {locs}",
            f"check return value in {ctx.path}/{ctx.name}.c")


def chk_unit_suffix(ctx, **_):
    violations = has_unit_suffix_on_physical(ctx.dir, ctx.name)
    if not violations:
        return None
    fields = ", ".join(violations[:5])
    return (f"physical fields without unit suffix: {fields}",
            f"add unit suffix to fields in {ctx.path}/{ctx.name}.h")


LAYER_ORDER = {"primitives": 0, "drivers": 1, "protocols": 2, "modules": 3, "_draft": 99}

_SELF_CONTAINED_RULES = [
    (re.compile(r'\bu?int(?:8|16|32|64)_t\b'), ['stdint.h']),
    (re.compile(r'\bbool\b'), ['stdbool.h']),
    (re.compile(r'\bsize_t\b'), ['stddef.h', 'stdlib.h', 'string.h', 'stdio.h']),
    (re.compile(r'\bNULL\b'), ['stddef.h', 'stdlib.h', 'string.h', 'stdio.h']),
]


def _get_header_includes(text):
    return set(re.findall(r'#include\s*[<"]([^>"]+)[>"]', text))


def _strip_c_comments(text):
    text = re.sub(r'//[^\n]*', '', text)
    return re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)


def _strip_literals(line):
    """Remove string/char literals, block comments, and line comments so brace counting is accurate."""
    line = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)   # string literals
    line = re.sub(r"'(?:[^'\\]|\\.)'", "' '", line)    # char literals
    line = re.sub(r'/\*.*?\*/', '', line)               # same-line block comments
    line = re.sub(r'//.*', '', line)                     # line comments
    return line


def chk_header_self_contained(ctx, **_):
    """Check that .h includes standard headers for types it uses (Q10)."""
    if not ctx.header:
        return None
    includes = _get_header_includes(ctx.header)
    code = _strip_c_comments(ctx.header)
    missing = []
    for usage_re, providers in _SELF_CONTAINED_RULES:
        if usage_re.search(code):
            if not any(p in includes for p in providers):
                missing.append(providers[0])
    missing = list(dict.fromkeys(missing))
    if missing:
        return (f".h uses types requiring {', '.join(missing)} but missing include",
                f"add #include <{missing[0]}> to {ctx.path}/{ctx.name}.h")
    return None


_STATIC_FUNC_RE = re.compile(
    r'^\s*static\s+(?:inline\s+)?(?:const\s+)?'
    r'\w[\w\s*]*\b(\w+)\s*\('
)


def chk_dead_static_function(ctx, **_):
    """Detect static functions in .c that are defined but never referenced (Q13)."""
    if not ctx.source:
        return None
    lines = ctx.source.split('\n')
    static_funcs = {}
    for i, line in enumerate(lines):
        m = _STATIC_FUNC_RE.match(line)
        if m and '=' not in line[:m.start(1)]:
            static_funcs.setdefault(m.group(1), set()).add(i)
    if not static_funcs:
        return None
    dead = []
    for func, def_lines in sorted(static_funcs.items()):
        func_re = re.compile(r'\b' + re.escape(func) + r'\b')
        found_use = False
        for i, line in enumerate(lines):
            if i in def_lines:
                continue
            if func_re.search(line):
                if re.match(r'^\s*static\s+', line) and func + '(' in line:
                    continue
                found_use = True
                break
        if not found_use:
            dead.append(func)
    if dead:
        return (f"dead static function(s): {', '.join(dead[:5])}", None)
    return None


def chk_switch_default(ctx, **_):
    """Flag switch statements in .c without a default case (Q8)."""
    if not ctx.source:
        return None
    # Strip block comments first to prevent /* } */ from confusing brace counting.
    source = re.sub(r'/\*.*?\*/', '', ctx.source, flags=re.DOTALL)
    lines = source.split('\n')
    violations = []
    i = 0
    while i < len(lines):
        if re.search(r'\bswitch\s*\(', lines[i]):
            brace_depth = 0
            has_default = False
            j = i
            found_open = False
            while j < len(lines):
                stripped = _strip_literals(lines[j])
                for ch in stripped:
                    if ch == '{':
                        brace_depth += 1
                        found_open = True
                    elif ch == '}':
                        brace_depth -= 1
                if re.search(r'\bdefault\s*:', lines[j]):
                    has_default = True
                if found_open and brace_depth == 0:
                    break
                j += 1
            if found_open and not has_default:
                violations.append(i + 1)
            i = j + 1
        else:
            i += 1
    if violations:
        locs = ", ".join(f"L{ln}" for ln in violations[:3])
        return (f"switch without default at {locs}", None)
    return None


def chk_dep_direction(ctx, **_):
    """Verify wheel dependencies only go downward per layer hierarchy (ADR-009)."""
    if not ctx.test_cmake:
        return None
    my_level = LAYER_ORDER.get(ctx.layer, 99)
    if my_level == 99:
        return None
    dep_pattern = re.compile(
        r'\.\./\.\./\.\./(primitives|drivers|protocols|modules)/([a-z0-9_]+)/')
    deps = dep_pattern.findall(ctx.test_cmake)
    violations = []
    for dep_layer, dep_name in deps:
        dep_level = LAYER_ORDER.get(dep_layer, 99)
        if dep_level > my_level:
            violations.append(f"{dep_name} ({dep_layer})")
    if violations:
        return (f"depends on higher layer: {', '.join(violations)} (ADR-009)", None)
    return None


def chk_missing_null_tests(ctx, **_):
    """Every wheel should have auto-generated NULL parameter tests."""
    test_dir = ctx.dir / "test"
    if not test_dir.is_dir():
        return None
    null_file = test_dir / f"test_null_{ctx.name}.c"
    if null_file.exists():
        return None
    return ("missing test_null_*.c (auto-generated NULL parameter tests)",
            f"run: python3 tooling/test/gen_null_tests.py {ctx.path}/{ctx.name}.h")


def chk_missing_hw_model(ctx, **_):
    """L1 drivers with I2C/SPI HAL should have a hardware register model."""
    if not ctx.header:
        return None
    if f"{ctx.name}_i2c_t" not in ctx.header and \
       f"{ctx.name}_spi_t" not in ctx.header:
        return None
    test_dir = ctx.dir / "test"
    if not test_dir.is_dir():
        return None
    hw_model = test_dir / f"hw_model_{ctx.name}.h"
    if hw_model.exists():
        return None
    return ("L1 driver with I2C/SPI HAL missing hw_model (datasheet test)",
            f"add {ctx.path}/test/hw_model_{ctx.name}.h")


# --- Check table: metadata for each check ---

CHECK_TABLE = [
    # (name, heading, func, level, layers, skip_vendor, skip_draft)
    ("ci_job",              "CI job",                             chk_ci_job,              "FAIL", None,                      False, False),
    ("vendor_md",           "Vendor VENDOR.md (ADR-010)",         chk_vendor_md,           "FAIL", None,                      False, False),
    ("cross_wheel_deps",    "Cross-wheel deps (ADR-009)",         chk_cross_wheel_deps,    "WARN", None,                      False, False),
    ("api_review",          "API review metadata",                chk_api_review,          "WARN", None,                      True,  True),
    ("ctest_registration",  "ctest registration",                 chk_ctest_registration,  "WARN", None,                      False, False),
    ("c99_standard",        "CMAKE_C_STANDARD 99",               chk_c99_standard,        "WARN", None,                      False, False),
    ("candidate_sections",  "Candidate README sections",          chk_candidate_sections,  "WARN", None,                      True,  True),
    ("file_header",         "File header (api-conventions §8)",   chk_file_header,         "WARN", None,                      True,  False),
    ("extern_c_guard",      'extern "C" guard',                   chk_extern_c_guard,      "WARN", None,                      True,  False),
    ("thread_safe",         "@thread_safe annotations",           chk_thread_safe,         "WARN", None,                      True,  False),
    ("test_exit_code",      "Test exit code",                     chk_test_exit_code,      "WARN", None,                      True,  False),
    ("test_run_balance",    "TEST/RUN balance",                   chk_balanced_test_run,   "WARN", None,                      True,  False),
    ("werror",              "-Werror flag",                       chk_werror,              "WARN", None,                      True,  False),
    ("wconversion",         "-Wconversion flag",                  chk_wconversion,         "WARN", None,                      True,  False),
    ("responsible_human",   "Responsible human",                  chk_responsible_human,   "WARN", None,                      True,  True),
    ("deviation_heading",   "API convention deviations heading",  chk_deviation_heading,   "WARN", None,                      False, False),
    ("error_code_sign",     "Error code sign convention (Q4)",    chk_error_code_sign,     "WARN", None,                      True,  False),
    ("function_doc",        "Function doc completeness (Q29)",    chk_function_doc,        "WARN", None,                      True,  True),
    ("error_code_coverage", "Error code test coverage",           chk_error_code_coverage, "WARN", None,                      True,  False),
    ("naming_convention",   "Naming convention",                  chk_naming_convention,   "WARN", None,                      True,  False),
    ("hal_injection",       "HAL injection pattern (L1)",         chk_hal_injection,       "WARN", {"drivers"},               True,  False),
    ("busy_wait_timeout",   "Busy-wait timeout guard",            chk_busy_wait_timeout,   "WARN", None,                      True,  False),
    ("dev_t_naming",        "Context type naming (_t not _dev_t)", chk_dev_t_naming,       "WARN", None,                      True,  False),
    ("err_plural",          "Error code singular (ERR_PARAM)",    chk_err_plural,          "WARN", None,                      True,  False),
    ("pragma_once",         "Include guard style",                chk_pragma_once,         "WARN", None,                      True,  False),
    ("spdx_header",         "SPDX license identifier",            chk_spdx_header,        "WARN", None,                      True,  False),
    ("vsf_native_residual", "'Why not VSF native?' residual",    chk_vsf_native_residual, "WARN", None,                      False, False),
    ("init_thread_safe",    "@thread_safe on init/deinit/reset", chk_init_caller_must_lock, "WARN", None,                    True,  False),
    ("deps_format",         "Deps format (api-conventions §8)",  chk_deps_format,         "WARN", None,                      True,  False),
    ("why_self_written",    "'Why self-written?' section",        chk_why_self_written,    "WARN", None,                      False, True),
    ("status_format",       "Status format",                     chk_status_format,       "WARN", None,                      False, False),
    ("instances_field",     "Instances field",                   chk_instances_field,     "WARN", None,                      True,  False),
    ("isr_safe",            "@isr_safe annotation",              chk_isr_safe,            "WARN", {"primitives", "drivers"}, True,  False),
    ("config_h_placement",  "config.h placement (§6.3)",         chk_config_h_placement,  "WARN", None,                      True,  False),
    ("nan_guard",           "NaN guard pattern (§2.5)",          chk_nan_guard,           "WARN", None,                      True,  False),
    ("deinit_safety",       "deinit safety (§3.4)",              chk_deinit_safety,       "WARN", None,                      True,  False),
    ("readme_section_order", "README section order",             chk_readme_section_order, "WARN", None,                     False, True),
    ("readme_section_names", "README section names",             chk_readme_section_names, "WARN", None,                     False, False),
    ("unit_suffix",         "Physical unit suffixes (§1.6)",     chk_unit_suffix,         "WARN", {"drivers"},               True,  False),
    ("readme_todo_mismatch", "README vs code TODO consistency",   chk_readme_todo_mismatch, "WARN", None,                     True,  True),
    ("instances_accuracy",  "Instances field accuracy",           chk_instances_accuracy,  "WARN", None,                      True,  False),
    ("signed_left_shift",   "Signed left shift (C99 UB)",        chk_signed_left_shift,   "WARN", None,                      True,  False),
    ("float_to_int_cast",   "Float-to-int cast without check",   chk_float_to_int_cast,   "WARN", None,                      True,  False),
    ("strlen_truncation",   "strlen truncation to narrow type",   chk_strlen_truncation,   "WARN", None,                      True,  False),
    ("memcpy_overflow",     "memcpy size_t overflow risk",        chk_memcpy_overflow,     "WARN", None,                      True,  False),
    ("unchecked_return",    "Unchecked error return",             chk_unchecked_return,    "WARN", None,                      True,  False),
    ("header_self_contained", "Header self-contained (Q10)",      chk_header_self_contained, "WARN", None,                    True,  False),
    ("dead_static_func",  "Dead static functions (Q13)",          chk_dead_static_function, "WARN", None,                     True,  False),
    ("switch_default",    "Switch default case (Q8)",             chk_switch_default,       "WARN", None,                     True,  False),
    ("dep_direction",     "Dependency direction (ADR-009)",       chk_dep_direction,        "WARN", None,                     False, True),
    ("missing_null_tests", "NULL parameter tests",                chk_missing_null_tests,   "WARN", None,                     True,  True),
    ("missing_hw_model",   "Hardware model tests (L1 I2C/SPI)",   chk_missing_hw_model,     "WARN", {"drivers"},              True,  False),
]


def find_wheels():
    """Return list of (layer, name, rel_path_str) for each wheel directory.

    Delegates to the shared implementation in _audit_build_base; converts
    the Path third element to the ``wheels/<layer>/<name>`` string form that
    this module's callers expect.
    """
    return [
        (layer, name, f"wheels/{layer}/{name}")
        for layer, name, _dir in _find_wheels_base(
            skip_vendor=False,
            require_test_dir=False,
        )
    ]


def detect_cross_wheel_deps(wheel_dir: Path) -> list:
    """Detect cross-wheel deps from test/CMakeLists.txt paths like ../../<layer>/<name>/."""
    cmake = wheel_dir / "test" / "CMakeLists.txt"
    if not cmake.exists():
        return []
    try:
        text = cmake.read_text(encoding="utf-8")
    except OSError:
        return []
    pattern = re.compile(r"\.\./\.\./\.\./(primitives|drivers|protocols|modules)/([a-z0-9_]+)/")
    return list({m[1] for m in pattern.findall(text)})


_consumer_cache = None

def _load_consumer_cache():
    """Load all project files once into memory to avoid repeated rglob on slow FS."""
    global _consumer_cache
    if _consumer_cache is not None:
        return _consumer_cache
    _consumer_cache = {"cmake": [], "sources": []}
    if not PROJECTS.is_dir():
        return _consumer_cache
    for cm in PROJECTS.rglob("CMakeLists.txt"):
        try:
            text = cm.read_text(encoding="utf-8")
            _consumer_cache["cmake"].append((str(cm.relative_to(REPO)).replace("\\", "/"), text))
        except OSError:
            pass
    for src in PROJECTS.rglob("*.[ch]"):
        try:
            text = src.read_text(encoding="utf-8", errors="ignore")
            _consumer_cache["sources"].append((str(src.relative_to(REPO)).replace("\\", "/"), text))
        except OSError:
            pass
    return _consumer_cache


def find_consumers(wheel_path: str, wheel_name: str) -> list:
    """Find project files referencing this wheel (CMakeLists.txt or #include)."""
    cache = _load_consumer_cache()
    consumers = []
    for rel_path, text in cache["cmake"]:
        if wheel_path in text:
            consumers.append(rel_path)
    if not consumers:
        header = f"{wheel_name}.h"
        for rel_path, text in cache["sources"]:
            if f'#include "{header}"' in text or f"#include <{header}>" in text:
                consumers.append(rel_path)
    return consumers


def parse_wishlist_names() -> list:
    """Extract candidate wheel names from WISHLIST.md (backticked first cell)."""
    if not WISHLIST.exists():
        return []
    text = WISHLIST.read_text(encoding="utf-8")
    pattern = re.compile(r"^\|\s*`([a-zA-Z0-9_]+)`\s*\|", re.MULTILINE)
    return pattern.findall(text)


STRAY_EXTENSIONS = {".log", ".o", ".obj", ".a", ".lib", ".elf", ".hex", ".bin",
                    ".map", ".d", ".gcda", ".gcno", ".su", ".pyc"}
ROOT_CONFIGS = {".clang-format", ".editorconfig"}


def _git_tracked_files(directory: Path) -> set:
    """Return set of git-tracked file paths (relative to repo) under directory."""
    import subprocess
    rel = str(directory.relative_to(REPO)).replace("\\", "/")
    try:
        result = subprocess.run(
            ["git", "ls-files", "--", f"{rel}/"],
            capture_output=True, text=True, cwd=str(REPO)
        )
        return {line.strip() for line in result.stdout.splitlines() if line.strip()}
    except (OSError, subprocess.SubprocessError):
        return set()


def audit_projects() -> list:
    """Lightweight scan of projects/ for stray or duplicate tracked files."""
    issues = []
    if not PROJECTS.is_dir():
        return issues

    tracked = _git_tracked_files(PROJECTS)

    for proj_dir in sorted(PROJECTS.iterdir()):
        if not proj_dir.is_dir() or proj_dir.name.startswith("."):
            continue
        proj_name = proj_dir.name

        for cfg_name in ROOT_CONFIGS:
            rel_path = f"projects/{proj_name}/{cfg_name}"
            if rel_path not in tracked:
                continue
            cfg = proj_dir / cfg_name
            root_cfg = REPO / cfg_name
            if root_cfg.exists():
                try:
                    if cfg.read_bytes() == root_cfg.read_bytes():
                        issues.append(
                            f"{rel_path}: duplicate of repo root "
                            f"(clang-format inherits upward, safe to delete)"
                        )
                except OSError:
                    pass

        for f in proj_dir.iterdir():
            if not f.is_file():
                continue
            rel_path = f"projects/{proj_name}/{f.name}"
            if rel_path not in tracked:
                continue
            if f.suffix in STRAY_EXTENSIONS:
                issues.append(
                    f"{rel_path}: build artifact tracked in git"
                )
            if f.suffix == ".py" and f.name != "__init__.py":
                try:
                    text = f.read_text(encoding="utf-8", errors="ignore")
                    if re.search(r'[A-Z]:\\', text) or re.search(r'filepath\s*=\s*r"', text):
                        issues.append(
                            f"{rel_path}: "
                            f"looks like a one-time script with hardcoded paths"
                        )
                except OSError:
                    pass

    return issues


def has_negative_error_codes(wheel_dir: Path, name: str) -> bool:
    """Check that error enum uses explicit negative values (Q4 audit)."""
    text = _cached_read(wheel_dir / f"{name}.h")
    if not text or f"{name}_err_t" not in text:
        return True
    lines = text.split("\n")
    in_enum = False
    for line in lines:
        stripped = line.strip()
        if ("_OK" in stripped or "_SUCCESS" in stripped) and "= 0" in stripped:
            in_enum = True
            continue
        if re.match(r'typedef\s+enum\s*\{', stripped):
            in_enum = True
            continue
        if in_enum:
            if "}" in stripped:
                break
            if "_ERR_" in stripped and "=" not in stripped:
                return False
    return True


def has_error_code_test_coverage(wheel_dir: Path, name: str) -> list:
    """Check that every error enum value has at least one test triggering it.
    Returns list of uncovered error code names (empty = all covered)."""
    text = _cached_read(wheel_dir / f"{name}.h")
    if not text:
        return []
    err_pattern = re.compile(r'\b(' + re.escape(name.upper()) + r'_ERR_\w+)\b')
    err_codes = set(err_pattern.findall(text))
    if not err_codes:
        return []
    test_dir = wheel_dir / "test"
    if not test_dir.is_dir():
        return sorted(err_codes)
    test_text = ""
    for tf in test_dir.glob("*.c"):
        test_text += _cached_read(tf)
    uncovered = [e for e in sorted(err_codes) if e not in test_text]
    return uncovered


def has_naming_convention(wheel_dir: Path, name: str) -> bool:
    """Check that public function declarations follow <wheel_name>_<verb> pattern.

    Scans the wheel's .h file for function declarations and verifies they are
    prefixed with the wheel name (api-conventions S1.1).
    """
    text = _cached_read(wheel_dir / f"{name}.h")
    if not text:
        return True
    # Match function declarations: return_type wheel_name_verb(...);
    # Skip typedefs, comments, macros, and lines inside struct/enum bodies
    # Accepted prefix abbreviations (api-conventions §1.5)
    ACCEPTED_PREFIXES = {
        "fixed_point": "fp_",
        "protobuf_lite": "pb_",
        "hash_fnv": "fnv1a_",
        "lookup_table": "lut_",
        "lora_sx1276": "lora_",
        "nrf24l01": "nrf24_",
        "button_handler": "btn_handler_",
        "version_info": "version_",
        "keypad_matrix": "keypad_",
        "cbor_lite": "cbor_lite_",
        "hex_codec": "hex_",
        "printf_tiny": ["printf_tiny", "snprintf_tiny", "vprintf_tiny", "vsnprintf_tiny"],
        "string_utils": "str_",
        "dns_packet": "dns_",
        "lorawan_packet": "lorawan_",
        "midi_packet": "midi_",
        "mqtt5_packet": "mqtt5_",
        "syslog_packet": "syslog_",
        "tlv_codec": "tlv_",
    }
    raw_prefix = ACCEPTED_PREFIXES.get(name, name + "_")
    prefixes = raw_prefix if isinstance(raw_prefix, list) else [raw_prefix]
    violations = 0
    in_acsl = False
    for line in text.split("\n"):
        stripped = line.strip()
        if "/*@" in stripped:
            in_acsl = True
        if in_acsl:
            if "*/" in stripped:
                in_acsl = False
            continue
        # Skip non-declaration lines
        if not ("(" in stripped and ");" in stripped):
            continue
        if stripped.startswith("typedef"):
            continue
        if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
            continue
        if stripped.startswith("#"):
            continue
        # Skip callback field declarations inside structs (e.g. bool (*write_reg)(...))
        if stripped.startswith("(") or "(*" in stripped:
            continue
        # Look for a function name token before the opening paren
        m = re.search(r'\b([a-z_][a-z0-9_]*)\s*\(', stripped)
        if not m:
            continue
        func_name = m.group(1)
        # Skip common non-function identifiers
        if func_name in ("if", "for", "while", "switch", "return", "sizeof"):
            continue
        if not any(func_name.startswith(p) for p in prefixes):
            violations += 1
    return violations == 0


def has_consistent_hal_pattern(wheel_dir: Path, name: str) -> bool:
    """Check that L1 driver wheels use HAL injection (struct with function pointers
    passed to init).

    Verifies:
    - Header defines a HAL struct (e.g. <name>_i2c_t, <name>_spi_t, <name>_hal_t)
      containing at least one function pointer field.
    - The init function accepts a pointer to this HAL struct.
    """
    text = _cached_read(wheel_dir / f"{name}.h")
    if not text:
        return True
    # Look for a HAL struct typedef: <name>_i2c_t, <name>_spi_t, <name>_hal_t,
    # <name>_gpio_t, <name>_onewire_t, <name>_pwm_t
    # Also accept abbreviated prefixes (per api-conventions §1.5):
    # e.g., lora_spi_t for lora_sx1276, nrf24_spi_t for nrf24l01
    hal_pattern = re.compile(
        r'\b\w+_(i2c|spi|hal|gpio|onewire|ow|pwm|io|uart|bus)_t\b'
    )
    has_hal_struct = bool(hal_pattern.search(text))
    if not has_hal_struct:
        return False
    # Verify at least one function pointer in the HAL struct
    # Pattern: something like  (*func_name)(...)
    if "(*" not in text:
        return False
    return True


def has_function_doc_all(wheel_dir: Path, name: str) -> bool:
    """Check that every function declaration in .h has a doc comment (Q29)."""
    text = _cached_read(wheel_dir / f"{name}.h")
    if not text:
        return True
    lines = text.split("\n")
    undoc = 0
    in_struct = 0
    for i, line in enumerate(lines):
        stripped = line.strip()
        if ("typedef struct" in stripped and "{" in stripped) or \
           ("struct" in stripped and stripped.endswith("{")) or \
           (stripped == "{" and i > 0 and "struct" in lines[i-1]):
            in_struct += 1
        if stripped.startswith("}") and in_struct > 0:
            in_struct -= 1
        if in_struct > 0:
            continue
        if (f"{name}_" in stripped and "(" in stripped and ");" in stripped
                and not stripped.startswith("typedef")
                and not stripped.startswith("//")
                and not stripped.startswith("/*")
                and not stripped.startswith("*")
                and "(*" not in stripped):
            prev = lines[i - 1].strip() if i > 0 else ""
            if not prev.startswith("/**") and not prev.startswith("*") and not prev.endswith("*/"):
                undoc += 1
    return undoc == 0


def has_busy_wait_timeout(wheel_dir: Path) -> list:
    """Scan .c files for hardware busy-wait loops lacking timeout guards."""
    hw_while = re.compile(
        r'\bwhile\s*\(.*\b('
        r'FLAG|STAT|STATUS|BUSY|BSY|RDY|READY|TXE|RXNE|TBE|'
        r'DONE|COMPLETE|PENDING|TRANSFER|'
        r'flag|status|busy|ready|done|pending'
        r')\b'
    )
    guard = re.compile(
        r'\b(timeout|tmo|counter|retry|retries|attempts|deadline|'
        r'elapsed|tick|watchdog|max_wait|limit)\b',
        re.IGNORECASE
    )
    unguarded = []
    for src in wheel_dir.glob("*.c"):
        text = _cached_read(src)
        if not text:
            continue
        lines = text.splitlines()
        for i, line in enumerate(lines):
            if not hw_while.search(line):
                continue
            if guard.search(line) or '--' in line:
                continue
            ctx = lines[max(0, i - 5):min(len(lines), i + 16)]
            if guard.search("\n".join(ctx)):
                continue
            unguarded.append((src.name, i + 1, line.strip()))
    return unguarded


def has_init_caller_must_lock(wheel_dir: Path, name: str) -> bool:
    """Return True if init/deinit/reset is tagged caller_must_lock (should be 'no')."""
    text = _cached_read(wheel_dir / f"{name}.h")
    if not text:
        return False
    lines = text.split("\n")
    for i, line in enumerate(lines):
        ll = line.lower()
        if "caller_must_lock" in ll:
            for j in range(i, min(i + 3, len(lines))):
                fn_line = lines[j].lower()
                if f"{name}_init" in fn_line or f"{name}_deinit" in fn_line or f"{name}_reset" in fn_line:
                    return True
    return False


def has_deps_format_issue(wheel_dir: Path, name: str) -> str:
    """Return description of Deps format issue, or '' if OK."""
    for ext in (".h", ".c"):
        text = _cached_read(wheel_dir / f"{name}{ext}")
        if not text:
            continue
        for line in text.split("\n")[:15]:
            stripped = line.strip().lstrip("* ")
            if stripped.startswith("Deps:"):
                val = stripped[5:].strip()
                if not val:
                    return "Deps: line is empty"
                if val == "(none)" or val == "none":
                    return ""
                # Check format: should be comma-separated, each is either 'name' or 'name_hal (injected)'
                for part in val.split(","):
                    part = part.strip()
                    if not part:
                        continue
                    if part == "(injected)" or part == "injected":
                        return f"Deps: '{part}' missing hal name — use '<name>_hal (injected)'"
                return ""
    return ""


def has_instances_field(wheel_dir: Path, name: str) -> bool:
    """Check file header has Instances: multi|single field (api-conventions §8)."""
    for ext in (".h", ".c"):
        text = _cached_read(wheel_dir / f"{name}{ext}")
        if not text:
            continue
        lines = text.splitlines()[:15]
        for line in lines:
            stripped = line.strip().lstrip("* ")
            if re.match(r'^Instances:\s*(multi|single)\s*$', stripped):
                return True
    return False


def has_config_h_placement(wheel_dir: Path, name: str) -> str:
    """Check config.h is only included in .c, never in .h (api-conventions §6.3).

    Returns violation description or '' if OK.
    """
    text = _cached_read(wheel_dir / f"{name}.h")
    if not text:
        return ""
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("/*"):
            continue
        if re.search(r'#include\s+[<"].*config\.h[>"]', stripped):
            return ".h includes config.h directly (must only be in .c)"
    return ""


STANDARD_README_SECTIONS = [
    "Origin",
    "Why self-written?",
    "Dependencies",
    "API",
    "Configuration",
    "Supported platforms",
    "Known issues",
    "How to test",
    "Test status",
    "API convention deviations",
    "API review",
    "Responsible human",
]


def _match_standard_section(heading: str) -> str:
    """Match a heading to a standard section name.

    Matches longest-first to avoid 'API' matching 'API convention deviations'.
    """
    hl = heading.strip().lower()
    for std in sorted(STANDARD_README_SECTIONS, key=len, reverse=True):
        if hl == std.lower() or hl.startswith(std.lower() + " ") or hl.startswith(std.lower() + "("):
            return std
    return ""


def has_readme_section_order(wheel_dir: Path) -> list:
    """Check README sections appear in standard order (wheel-contract §3.2).

    Returns list of out-of-order section names (empty = correct).
    """
    text = _cached_read(wheel_dir / "README.md")
    if not text:
        return []
    headings = re.findall(r'^## (.+)$', text, re.MULTILINE)
    found_order = []
    for pos, h in enumerate(headings):
        std = _match_standard_section(h)
        if std and std not in [s for s, _ in found_order]:
            found_order.append((std, pos))
    if len(found_order) < 2:
        return []
    std_indices = {s: i for i, s in enumerate(STANDARD_README_SECTIONS)}
    violations = []
    for i in range(len(found_order) - 1):
        cur_std = found_order[i][0]
        nxt_std = found_order[i + 1][0]
        if std_indices[cur_std] > std_indices[nxt_std]:
            violations.append(cur_std)
    return violations


def has_readme_section_names(wheel_dir: Path) -> list:
    """Check README section names match standard English names.

    Returns list of non-standard heading names.
    """
    text = _cached_read(wheel_dir / "README.md")
    if not text:
        return []
    headings = re.findall(r'^## (.+)$', text, re.MULTILINE)
    LEGACY_MAP = {
        "convention deviations": "API convention deviations",
        "deviations": "API convention deviations",
        "configuration macros": "Configuration (compile-time)",
        "api review status": "API review",
        "test": "How to test",
        "testing": "How to test",
    }
    violations = []
    for h in headings:
        hl = h.strip().lower()
        if hl in LEGACY_MAP:
            violations.append(f"'{h}' -> '{LEGACY_MAP[hl]}'")
    return violations


def has_nan_guard_in_float_api(wheel_dir: Path, name: str) -> list:
    """Check that public float-parameter functions use NaN-safe guard pattern.

    Only flags range checks on variables that are actually float/double.
    Detects by scanning backward from the comparison to find the variable's
    type declaration, or by checking if the comparison uses float literals (e.g. 0.0f).
    Returns list of (line_no, line_text) with vulnerable range checks.
    """
    text = _cached_read(wheel_dir / f"{name}.c")
    if not text:
        return []
    lines = text.splitlines()
    vulnerable = re.compile(
        r'if\s*\(\s*(\w+)\s*<\s*([-\d.]+f?)\s*\|\|\s*\w+\s*>\s*([-\d.]+f?)\s*\)'
    )
    float_literal = re.compile(r'\d+\.\d+f?$|^\d+f$')
    violations = []
    for i, line in enumerate(lines):
        m = vulnerable.search(line)
        if not m:
            continue
        var_name, lo_lit, hi_lit = m.group(1), m.group(2), m.group(3)
        if float_literal.match(lo_lit) or float_literal.match(hi_lit):
            violations.append((i + 1, line.strip()))
            continue
        for j in range(max(0, i - 30), i):
            prev = lines[j].strip()
            if re.search(rf'\b(float|double)\b.*\b{re.escape(var_name)}\b', prev):
                violations.append((i + 1, line.strip()))
                break
    return violations


def has_deinit_null_check(wheel_dir: Path, name: str) -> bool:
    """Check deinit function validates input pointer (api-conventions §3.4).

    Returns True if no deinit exists or if deinit checks for NULL.
    """
    text = _cached_read(wheel_dir / f"{name}.c")
    if not text:
        return True
    deinit_pat = re.compile(rf'{re.escape(name)}_deinit\s*\(')
    m = deinit_pat.search(text)
    if not m:
        return True
    block = text[m.start():m.start() + 500]
    # Extract first param name from deinit signature
    sig_m = re.search(rf'{re.escape(name)}_deinit\s*\(\s*\w[\w\s\*]*?(\w+)\s*\)', text)
    param = sig_m.group(1) if sig_m else None
    if param and (f"!{param}" in block or f"{param} == NULL" in block or f"{param} != NULL" in block):
        return True
    return "!dev" in block or "NULL" in block or "!ctx" in block or "!self" in block


PHYSICAL_QUANTITIES = {
    "temperature", "pressure", "humidity", "altitude", "voltage",
    "current", "resistance", "illuminance", "distance", "speed",
    "acceleration", "angular_velocity", "magnetic", "heading",
    "frequency", "concentration", "power", "energy",
}
UNIT_SUFFIXES = {
    "_c", "_f", "_k", "_pa", "_hpa", "_pct", "_m", "_mm", "_km",
    "_mv", "_v", "_ma", "_a", "_ohm", "_lux", "_kmh", "_ms", "_us",
    "_hz", "_khz", "_mhz", "_deg", "_rad", "_dps", "_mg", "_g",
    "_gauss", "_ppm", "_raw", "_w", "_mw", "_wh",
}
EXEMPT_NAMES = {"temperature_raw", "pressure_raw", "humidity_raw"}


def has_unit_suffix_on_physical(wheel_dir: Path, name: str) -> list:
    """Check struct fields holding physical quantities have unit suffixes.

    Only checks L1 driver data structs (most likely to hold physical values).
    Returns list of (field_name, suggestion).
    """
    text = _cached_read(wheel_dir / f"{name}.h")
    if not text:
        return []
    if "_data_t" not in text and "_reading_t" not in text and "_result_t" not in text:
        return []
    violations = []
    in_data_struct = False
    for line in text.splitlines():
        stripped = line.strip()
        if re.search(r'(_data_t|_reading_t|_result_t)\s*\{', stripped):
            in_data_struct = True
            continue
        if in_data_struct and stripped.startswith("}"):
            in_data_struct = False
            continue
        if not in_data_struct:
            continue
        m = re.match(r'(?:float|double|int\d+_t|uint\d+_t)\s+(\w+)\s*;', stripped)
        if not m:
            continue
        field = m.group(1)
        if field in EXEMPT_NAMES:
            continue
        has_quantity = any(q in field.lower() for q in PHYSICAL_QUANTITIES)
        if not has_quantity:
            continue
        has_suffix = any(field.endswith(s) for s in UNIT_SUFFIXES)
        if not has_suffix:
            violations.append(field)
    return violations


def main(quiet: bool = False, json_mode: bool = False, fast: bool = False,
         save: bool = False, wheel_filter: str = None) -> int:
    def qprint(msg: str):
        if not quiet and not json_mode:
            print(msg)

    def tprint(msg: str):
        if not json_mode:
            print(msg)

    if not WHEELS.is_dir():
        print(f"ERROR: wheels/ not found at {WHEELS}", file=sys.stderr)
        return 2

    ci_text = CI_YAML.read_text(encoding="utf-8") if CI_YAML.exists() else ""
    if wheel_filter:
        wf = wheel_filter.rstrip("/")
        if not wf.startswith("wheels/"):
            wf = "wheels/" + wf
        parts = wf.split("/")
        if len(parts) < 3:
            print(f"ERROR: expected wheels/<layer>/<name>, got '{wf}'", file=sys.stderr)
            return 2
        layer, name = parts[1], parts[2]
        wheel_dir = WHEELS / layer / name
        if not wheel_dir.is_dir():
            print(f"ERROR: {wheel_dir} not found", file=sys.stderr)
            return 2
        wheels = [(layer, name, wf)]
    else:
        wheels = find_wheels()

    # Pre-load all wheel files into cache (one-time IO, all wheels × 4 files)
    contexts = {}
    for layer, name, path in wheels:
        contexts[path] = WheelCtx(layer, name, WHEELS / layer / name)

    failures = []
    warnings = []
    results = []
    known_exc = _load_known_exceptions()

    def record(check, wheel_path, level, msg, fix_cmd=None):
        if level == "WARN" and (wheel_path, check) in known_exc:
            level = "INFO"
            msg += f" [known exception: {known_exc[(wheel_path, check)]}]"
        entry = {"check": check, "wheel": wheel_path, "level": level, "message": msg}
        if fix_cmd:
            entry["fix"] = fix_cmd
        results.append(entry)
        if level == "FAIL":
            failures.append(f"{wheel_path}: {msg}")
        elif level == "WARN":
            warnings.append(f"{wheel_path}: {msg}")
        return level

    qprint(f"=== Wheel audit ({len(wheels)} wheels, {len(CHECK_TABLE)} checks) ===\n")

    # --- Table-driven check dispatch ---
    for chk_name, heading, chk_fn, level, layers, skip_vendor, skip_draft in CHECK_TABLE:
        qprint(f"\n=== {heading} ===\n")
        for layer, name, path in wheels:
            ctx = contexts[path]
            if skip_vendor and ctx.is_vendor:
                qprint(f"  [skip]  {path} (vendor)")
                continue
            if skip_draft and ctx.is_draft:
                continue
            if layers and layer not in layers:
                continue
            result = chk_fn(ctx, ci_text=ci_text)
            if result:
                msg, fix_cmd = result
                actual_level = record(chk_name, path, level, msg, fix_cmd)
                tprint(f"  [{actual_level}]  {path}: {msg}")
            else:
                qprint(f"  [ ok ]  {path}")

    # --- Consumer check (special: INFO level, needs consumer scan) ---
    for layer, name, path in wheels:
        ctx = contexts[path]
        consumers = [] if fast else find_consumers(path, name)
        consumer_required = layer in CONSUMER_REQUIRED
        origin = "Extracted from" in ctx.readme and "projects/" in ctx.readme

        if consumer_required and not consumers:
            reason = "extracted from project" if origin else "proactively built"
            record("consumer", path, "INFO", f"no consumer yet ({reason}, acceptable)",
                   fix_cmd=f"#include <{name}.h> in a project")
        elif not consumers:
            record("consumer", path, "INFO", f"no consumer yet (acceptable for {layer})",
                   fix_cmd=f"#include <{name}.h> in a project")

    # --- WISHLIST staleness ---

    qprint("\n=== WISHLIST staleness ===\n")
    wishlist_names = parse_wishlist_names()
    wheel_names = {name for _, name, _ in wheels}
    stale = [n for n in wishlist_names if n in wheel_names]
    if stale:
        for n in stale:
            record("wishlist_stale", f"WISHLIST/{n}", "FAIL",
                   f"'{n}' listed but already exists as a wheel",
                   fix_cmd=f"remove '{n}' row from wheels/WISHLIST.md")
            tprint(f"  [FAIL]  {n} listed in WISHLIST but already exists")
    else:
        qprint("  [ ok ]  no stale entries")

    # --- Projects directory hygiene ---
    qprint("\n=== Projects directory hygiene ===\n")
    proj_issues = audit_projects()
    for msg in proj_issues:
        record("project_hygiene", "projects/", "WARN", msg)
    if not proj_issues:
        qprint("  [ ok ]  no stray files detected")
    else:
        for msg in proj_issues:
            tprint(f"  [WARN]  {msg}")

    # --- Consumer mapping summary ---
    qprint("\n=== Consumer mapping ===\n")
    consumed = []
    unconsumed = []
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        cons = [] if fast else find_consumers(path, name)
        if cons:
            consumed.append((path, cons))
        else:
            unconsumed.append(path)
    if consumed:
        for wpath, cons in consumed:
            projs = set()
            for c in cons:
                parts = c.split("/")
                if len(parts) > 1 and parts[0] == "projects":
                    projs.add(parts[1])
            qprint(f"  {wpath:36s} ← {', '.join(sorted(projs))}")
    qprint(f"\n  consumed: {len(consumed)}/{len(wheels)}  |  unconsumed: {len(unconsumed)}")

    infos = [r for r in results if r["level"] == "INFO"]

    if save:
        save_snapshot(results, len(wheels), len(failures), len(warnings))

    if json_mode:
        output = {
            "wheels": len(wheels),
            "failures": len(failures),
            "warnings": len(warnings),
            "infos": len(infos),
            "results": [r for r in results if r["level"] not in ("OK", "INFO")],
            "consumed": len(consumed),
            "unconsumed": len(unconsumed),
        }
        print(json.dumps(output, indent=2, ensure_ascii=False))
        return 1 if failures else 0

    tprint("\n=== Summary ===")
    tprint(f"  wheels:   {len(wheels)}")
    tprint(f"  failures: {len(failures)}")
    tprint(f"  warnings: {len(warnings)}")
    if infos:
        tprint(f"  info:     {len(infos)} (unconsumed wheels, non-blocking)")

    if failures:
        tprint("\nFailures:")
        for r in results:
            if r["level"] == "FAIL":
                tprint(f"  - {r['wheel']}: {r['message']}")
                if "fix" in r:
                    tprint(f"    → {r['fix']}")
        return 1

    if warnings:
        tprint("\nWarnings (non-blocking):")
        for r in results:
            if r["level"] == "WARN":
                tprint(f"  - {r['wheel']}: {r['message']}")
                if "fix" in r:
                    tprint(f"    → {r['fix']}")

    return 0


def print_dep_graph(wheels):
    """Print wheel dependency graph in Mermaid format."""
    print("```mermaid")
    print("graph TD")
    for layer, name, path in wheels:
        print(f"    {name}[\"{name} ({layer[0].upper()}{layer[1:3]})\"]")
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        deps = detect_cross_wheel_deps(wheel_dir)
        for dep in deps:
            print(f"    {name} --> {dep}")
    consumers = {}
    for layer, name, path in wheels:
        wheel_dir = WHEELS / layer / name
        rel = str(wheel_dir.relative_to(REPO)).replace("\\", "/")
        found = find_consumers(rel, name)
        for c in found:
            proj = c.split("/")[1] if c.startswith("projects/") else None
            if proj:
                consumers.setdefault(proj, set()).add(name)
    for proj, wset in consumers.items():
        print("    " + proj + '{{"' + proj + '"}}')
        for w in sorted(wset):
            print(f"    {proj} -.-> {w}")
    print("```")


SNAPSHOT_DIR = REPO / "tooling" / "audit" / "snapshots"


def save_snapshot(results, wheels_count, failures, warnings):
    """Save audit results as timestamped JSON snapshot."""
    from datetime import datetime
    SNAPSHOT_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    all_check_names = [name for name, *_ in CHECK_TABLE]
    all_check_names.extend(["consumer", "wishlist_stale", "project_hygiene"])
    snapshot = {
        "timestamp": datetime.now().isoformat(),
        "wheels": wheels_count,
        "failures": failures,
        "warnings": warnings,
        "total_checks": len(all_check_names),
        "check_list": all_check_names,
        "findings": {},
    }
    for r in results:
        check = r["check"]
        if check not in snapshot["findings"]:
            snapshot["findings"][check] = {"count": 0, "wheels": []}
        snapshot["findings"][check]["count"] += 1
        snapshot["findings"][check]["wheels"].append(r["wheel"])

    path = SNAPSHOT_DIR / f"audit_{ts}.json"
    path.write_text(json.dumps(snapshot, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Snapshot saved: {path.relative_to(REPO)}")
    # Auto-cleanup: keep only the latest 20 snapshots
    all_snaps = sorted(SNAPSHOT_DIR.glob("audit_*.json"))
    for old in all_snaps[:-20]:
        old.unlink()
        print(f"  Cleaned up old snapshot: {old.name}")
    return path


def compare_snapshots():
    """Compare latest two snapshots and show delta."""
    if not SNAPSHOT_DIR.is_dir():
        print("No snapshots found. Run with --save first.")
        return
    files = sorted(SNAPSHOT_DIR.glob("audit_*.json"))
    if len(files) < 2:
        print(f"Need at least 2 snapshots to compare (found {len(files)}). Run --save again.")
        return
    prev = json.loads(files[-2].read_text(encoding="utf-8"))
    curr = json.loads(files[-1].read_text(encoding="utf-8"))

    print(f"Comparing: {files[-2].name} → {files[-1].name}\n")
    print(f"  Wheels:   {prev['wheels']} → {curr['wheels']}")
    print(f"  Failures: {prev['failures']} → {curr['failures']}")
    print(f"  Warnings: {prev['warnings']} → {curr['warnings']}")

    all_checks = set(list(prev.get("findings", prev.get("checks", {})).keys()) + list(curr.get("findings", curr.get("checks", {})).keys()))
    deltas = []
    for check in sorted(all_checks):
        p = prev.get("findings", prev.get("checks", {})).get(check, {}).get("count", 0)
        c = curr.get("findings", curr.get("checks", {})).get(check, {}).get("count", 0)
        if p != c:
            delta = c - p
            sign = "+" if delta > 0 else ""
            deltas.append((check, p, c, f"{sign}{delta}"))

    if deltas:
        print(f"\n  Check deltas:")
        for check, p, c, d in deltas:
            print(f"    {check:30s}  {p:3d} → {c:3d}  ({d})")
    else:
        print("\n  No changes between snapshots.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Wheel audit")
    parser.add_argument("--graph", action="store_true", help="Print Mermaid dependency graph")
    parser.add_argument("--quiet", "-q", action="store_true", help="Only show WARN/FAIL and summary")
    parser.add_argument("--json", action="store_true", help="Output results as JSON (machine-readable)")
    parser.add_argument("--fast", action="store_true", help="Skip consumer scan (fast mode for slow FS)")
    parser.add_argument("--save", action="store_true", help="Save results as timestamped snapshot")
    parser.add_argument("--compare", action="store_true", help="Compare latest two snapshots")
    parser.add_argument("--wheel", type=str, default=None,
                        help="Audit a single wheel (e.g. drivers/mcp4725)")
    args = parser.parse_args()
    if args.graph:
        wheels = find_wheels()
        print_dep_graph(wheels)
        sys.exit(0)
    if args.compare:
        compare_snapshots()
        sys.exit(0)
    sys.exit(main(quiet=args.quiet, json_mode=args.json, fast=args.fast,
                  save=args.save, wheel_filter=args.wheel))
