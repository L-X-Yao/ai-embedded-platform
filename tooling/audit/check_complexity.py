#!/usr/bin/env python3
"""
check_complexity.py — cyclomatic complexity checker for wheel .c files.

Q14 audit dimension: measures cyclomatic complexity per function and flags
functions that exceed a configurable threshold (default 15).

Cyclomatic complexity = 1 + decision points, where decision points are:
  if, else if, for, while, do, case, &&, ||, ?

Usage:
  check_complexity.py                     # scan all wheels, threshold 15
  check_complexity.py --threshold 10      # stricter threshold
  check_complexity.py --wheel modbus_rtu  # single wheel
  check_complexity.py --json              # machine-readable output
  check_complexity.py --quiet             # suppress OK output

Exit 0 if no functions exceed threshold, 1 if any do.
"""

import argparse
import json
import re
import sys
from pathlib import Path
from typing import List, NamedTuple, Optional

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"

LAYERS = ["primitives", "drivers", "protocols", "modules", "_draft"]

# Tokens that each contribute +1 to cyclomatic complexity
_DECISION_RE = re.compile(
    r'\b(?:if|else\s+if|for|while|do|case)\b'  # control flow keywords
    r'|(\|\|)'                                  # logical OR
    r'|(\&\&)'                                  # logical AND
    r'|(?<![=!<>])\?(?!=)'                      # ternary ? (not ?= or == etc)
)

# Pattern for function opening: a line that ends with ')' or '){' or '){'
# preceded by something that looks like a function definition.
# We track brace depth and use the line where depth goes 0→1.
_FUNC_START_RE = re.compile(
    r'^'
    r'(?![\s#/\*])'           # not a preprocessor, comment, or whitespace line
    r'(?!.*\b(?:if|else|for|while|do|switch|return)\b\s*\()'  # not control flow
    r'(?!.*=\s*\{)'           # not struct/array initialiser
    r'(?!typedef\b)'          # not typedef
    r'(?!static\s+const\b|static\s+(?:const\s+)?[a-z_A-Z0-9]+\s+[a-z_A-Z0-9]+\s*\[)'  # not static const array
    r'.*'
    r'\)\s*\{?\s*$'           # ends with ) or ) {
)


class FunctionResult(NamedTuple):
    wheel: str
    func_name: str
    complexity: int
    file_path: str
    line: int


def _strip_comments_and_strings(text: str) -> str:
    """Remove C block comments, line comments, and string literals.

    Replaces removed content with spaces to preserve line numbering.
    """
    result = []
    i = 0
    n = len(text)
    while i < n:
        # Block comment
        if text[i:i+2] == '/*':
            end = text.find('*/', i + 2)
            if end == -1:
                # Unterminated — consume to end
                chunk = text[i:]
                result.append('\n' * chunk.count('\n'))
                break
            chunk = text[i:end+2]
            result.append('\n' * chunk.count('\n'))
            i = end + 2
        # Line comment
        elif text[i:i+2] == '//':
            end = text.find('\n', i + 2)
            if end == -1:
                break
            result.append('\n')
            i = end  # keep the newline
        # String literal
        elif text[i] == '"':
            result.append('"')
            i += 1
            while i < n:
                c = text[i]
                i += 1
                if c == '\\':
                    if i < n:
                        i += 1  # skip escaped char
                    continue
                if c == '"':
                    result.append('"')
                    break
                result.append(' ' if c != '\n' else '\n')
        # Char literal
        elif text[i] == "'":
            result.append("'")
            i += 1
            while i < n:
                c = text[i]
                i += 1
                if c == '\\':
                    if i < n:
                        i += 1
                    continue
                if c == "'":
                    result.append("'")
                    break
                result.append(' ' if c != '\n' else '\n')
        else:
            result.append(text[i])
            i += 1
    return ''.join(result)


def _count_decisions(text: str) -> int:
    """Count decision points in a block of (comment/string-stripped) code."""
    count = 0
    for m in _DECISION_RE.finditer(text):
        count += 1
    return count


def _extract_func_name(line: str) -> Optional[str]:
    """Heuristically extract the function name from a function-definition line.

    Handles patterns like:
      int foo_bar(...)
      static void foo(...)
      state_machine_err_t sm_process(state_machine_t *ctx, ...)
      type (*fn_ptr)(...)     <- skip, this is a function pointer typedef
    """
    # Remove leading storage/type qualifiers progressively
    # Strip: static, inline, const, volatile, unsigned, signed, extern
    stripped = re.sub(
        r'^\s*(static\s+)?(inline\s+)?(extern\s+)?(const\s+)?(volatile\s+)?'
        r'(unsigned\s+|signed\s+)?',
        '', line.strip()
    )
    # Match: return_type func_name( ...
    m = re.match(r'^[\w_*\s]+?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', stripped)
    if not m:
        return None
    name = m.group(1)
    # Skip C keywords that look like function calls
    if name in ('if', 'else', 'for', 'while', 'do', 'switch', 'return',
                 'sizeof', 'typeof', 'alignof', 'offsetof'):
        return None
    return name


def analyze_file(c_file: Path, wheel_name: str) -> List[FunctionResult]:
    """Parse a .c file and return complexity results for each function."""
    try:
        raw = c_file.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    clean = _strip_comments_and_strings(raw)
    lines = clean.splitlines()

    results: List[FunctionResult] = []

    brace_depth = 0
    func_start_line = 0
    func_name = ""
    func_body_lines: List[str] = []
    in_function = False

    # Track the "candidate" function header lines (may span multiple lines
    # before the opening {)
    header_buf: List[str] = []
    header_start = 0

    for lineno, line in enumerate(lines, start=1):
        # Count braces on this line
        open_braces = line.count('{')
        close_braces = line.count('}')

        if not in_function:
            # Accumulate potential function header lines
            stripped = line.strip()

            # Reset header buffer if we see a preprocessor directive,
            # blank line (with brace_depth == 0), or semicolon-terminated line
            if brace_depth == 0 and (
                stripped.startswith('#')
                or stripped == ''
                or (stripped.endswith(';') and '(' not in stripped)
            ):
                header_buf = []
                header_start = lineno + 1

            # Add line to candidate header
            if stripped:
                header_buf.append(line)

            # Check if opening brace appears at file scope (depth 0 → 1)
            if open_braces > close_braces and brace_depth == 0:
                # This line contains the opening { of a potential function
                # Check that header_buf looks like a function definition
                combined = ' '.join(header_buf)
                if _looks_like_function(combined):
                    fname = _find_name_in_header(header_buf)
                    if fname:
                        func_name = fname
                        func_start_line = header_start
                        func_body_lines = [line]
                        in_function = True
                        brace_depth += open_braces - close_braces
                        header_buf = []
                        header_start = lineno + 1
                        continue

            brace_depth += open_braces - close_braces
            if brace_depth < 0:
                brace_depth = 0

        else:
            # Inside a function body
            func_body_lines.append(line)
            brace_depth += open_braces - close_braces

            if brace_depth == 0:
                # Function ended
                body = '\n'.join(func_body_lines)
                complexity = 1 + _count_decisions(body)
                results.append(FunctionResult(
                    wheel=wheel_name,
                    func_name=func_name,
                    complexity=complexity,
                    file_path=str(c_file),
                    line=func_start_line,
                ))
                in_function = False
                func_body_lines = []
                func_name = ""
                header_buf = []
                header_start = lineno + 1

    return results


def _looks_like_function(combined: str) -> bool:
    """Return True if the combined header lines look like a function definition."""
    stripped = combined.strip()
    # Must have parentheses and end with ) or ) {
    if '(' not in stripped or ')' not in stripped:
        return False
    # Skip preprocessor
    if stripped.startswith('#'):
        return False
    # Skip struct/union/enum definitions
    if re.match(r'\b(struct|union|enum|typedef)\b', stripped):
        return False
    # Skip control-flow statements
    if re.match(r'\s*(if|else|for|while|do|switch)\s*\(', stripped):
        return False
    # Skip function-pointer typedefs / variable declarations
    if re.search(r'\(\s*\*', stripped):
        return False
    # Skip macro invocations that look like MACRO_NAME(
    # (all-caps identifiers right before '(')
    if re.match(r'^[A-Z_][A-Z0-9_]*\s*\(', stripped):
        return False
    # Must have something that looks like an identifier before '('
    if not re.search(r'[A-Za-z_][A-Za-z0-9_]*\s*\(', stripped):
        return False
    return True


def _find_name_in_header(header_buf: List[str]) -> Optional[str]:
    """Find the function name from accumulated header lines."""
    # The function name is on the line that directly precedes or contains '('
    for line in reversed(header_buf):
        name = _extract_func_name(line)
        if name:
            return name
    return None


def find_c_files(wheel_filter: Optional[str] = None) -> List[tuple]:
    """Yield (wheel_name, c_file_path) for all wheels."""
    found = []
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for wheel_dir in sorted(layer_dir.iterdir()):
            if not wheel_dir.is_dir():
                continue
            name = wheel_dir.name
            if wheel_filter and name != wheel_filter:
                continue
            # Collect all .c files directly in the wheel directory
            for c_file in sorted(wheel_dir.glob("*.c")):
                # Skip test files
                if c_file.name.startswith("test_") or c_file.name.startswith("prop_"):
                    continue
                found.append((name, c_file))
    return found


def run(threshold: int, wheel_filter: Optional[str],
        quiet: bool, as_json: bool) -> int:
    """Run the complexity check. Returns exit code."""

    c_files = find_c_files(wheel_filter)
    if not c_files:
        if not quiet:
            print("No .c files found.")
        return 0

    all_results: List[FunctionResult] = []
    for wheel_name, c_file in c_files:
        results = analyze_file(c_file, wheel_name)
        all_results.extend(results)

    total_funcs = len(all_results)
    flagged = [r for r in all_results if r.complexity > threshold]
    flagged_sorted = sorted(flagged, key=lambda r: -r.complexity)

    if as_json:
        output = {
            "threshold": threshold,
            "total_functions": total_funcs,
            "flagged_count": len(flagged),
            "flagged": [
                {
                    "wheel": r.wheel,
                    "function": r.func_name,
                    "complexity": r.complexity,
                    "file": r.file_path,
                    "line": r.line,
                }
                for r in flagged_sorted
            ],
            "top10_worst": [
                {
                    "wheel": r.wheel,
                    "function": r.func_name,
                    "complexity": r.complexity,
                    "file": r.file_path,
                    "line": r.line,
                }
                for r in sorted(all_results, key=lambda r: -r.complexity)[:10]
            ],
        }
        print(json.dumps(output, indent=2))
        return 1 if flagged else 0

    # Human-readable output
    if flagged:
        if not quiet:
            print(f"Functions exceeding complexity threshold ({threshold}):")
            print()
        for r in flagged_sorted:
            # Relative path for readability
            try:
                rel = Path(r.file_path).relative_to(REPO)
            except ValueError:
                rel = Path(r.file_path)
            severity = "HIGH" if r.complexity >= 20 else "MODERATE"
            print(f"  [{severity}] {r.wheel}:{r.func_name}:{r.complexity}:{rel}:{r.line}")
        print()

    # Summary always printed (unless quiet and no flagged)
    if not quiet or flagged:
        print(f"Summary: {total_funcs} functions scanned, "
              f"{len(flagged)} above threshold {threshold}")

    if not quiet and flagged:
        print()
        worst = sorted(all_results, key=lambda r: -r.complexity)[:10]
        print("Top 10 worst offenders:")
        for i, r in enumerate(worst, 1):
            try:
                rel = Path(r.file_path).relative_to(REPO)
            except ValueError:
                rel = Path(r.file_path)
            print(f"  {i:2d}. {r.wheel}:{r.func_name}  complexity={r.complexity}  ({rel}:{r.line})")

    return 1 if flagged else 0


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Cyclomatic complexity checker for wheel .c files (Q14)."
    )
    parser.add_argument(
        "--threshold", "-t",
        type=int, default=15,
        metavar="N",
        help="Flag functions with complexity > N (default: 15)",
    )
    parser.add_argument(
        "--wheel", "-w",
        metavar="NAME",
        help="Check only the named wheel (e.g. modbus_rtu)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output results as JSON",
    )
    parser.add_argument(
        "--quiet", "-q",
        action="store_true",
        help="Suppress OK-level output (only show flagged functions)",
    )
    args = parser.parse_args()

    sys.exit(run(
        threshold=args.threshold,
        wheel_filter=args.wheel,
        quiet=args.quiet,
        as_json=args.json,
    ))


if __name__ == "__main__":
    main()
