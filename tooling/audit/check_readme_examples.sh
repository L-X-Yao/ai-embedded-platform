#!/usr/bin/env bash
# check_readme_examples.sh — extract C code blocks from wheel README.md files
# and attempt to compile each one with gcc -fsyntax-only.
#
# Purpose: Q6 audit dimension — catch broken API examples before users copy them.
#
# Leniency policy (README examples are illustrative, not complete programs):
#   PASS — gcc -fsyntax-only exits 0
#   SKIP — errors that are acceptable for snippets:
#            • undeclared identifier (variable/function from calling context)
#            • implicit function declaration (function not declared in snippet)
#            • missing header file (dependency wheel not on include path)
#            • passing argument of incompatible pointer type (from opaque types)
#   FAIL — genuine syntax errors: unexpected token, mismatched braces/parens,
#           unknown type names that prevent parsing, etc.
#          — stale API: a wrong argument count, or conflicting types that gcc
#            shows as different, for a function the wheel's own headers
#            declare (about any other name these stay SKIP: it comes from the
#            calling context)
#
# Usage:
#   check_readme_examples.sh                  # check all wheels
#   check_readme_examples.sh --wheel <name>   # check a single wheel by name
#   check_readme_examples.sh --fix            # same as default (no auto-fix)
#   check_readme_examples.sh --quiet          # suppress PASS/SKIP, show only FAIL
#   check_readme_examples.sh --json           # machine-readable JSON to stdout
#
# Exit codes:
#   0 — all examples pass or skip
#   1 — one or more examples have hard errors
#   2 — --wheel named no wheel with a README: nothing was checked

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"

BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

WHEEL_FILTER=""
QUIET=false
JSON_MODE=false
FIX_MODE=false   # accepted but no-op — consistent with other audit tools

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wheel)    WHEEL_FILTER="$2"; shift 2 ;;
        --quiet|-q) QUIET=true; shift ;;
        --json)     JSON_MODE=true; QUIET=true; shift ;;
        --fix)      FIX_MODE=true; shift ;;
        -h|--help)
            echo "Usage: check_readme_examples.sh [--wheel <name>] [--quiet] [--json] [--fix]"
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

if ! $QUIET; then
    echo -e "${BOLD}=== README Example Compiler Check ===${NC}"
    echo ""
fi

# -----------------------------------------------------------------------
# Temporary workspace — cleaned on exit
# -----------------------------------------------------------------------
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

# -----------------------------------------------------------------------
# Helper: extract fenced ```c blocks from a markdown file.
# Writes one file per block: <out_dir>/block_NNN.c
# Prints the number of blocks extracted.
# -----------------------------------------------------------------------
extract_c_blocks() {
    local md_file="$1"
    local out_dir="$2"
    local count=0
    local in_block=false
    local current_file=""

    while IFS= read -r line; do
        if ! $in_block; then
            # Opening fence: ```c or ```C (with optional trailing whitespace)
            if [[ "$line" =~ ^[\`]{3}[cC][[:space:]]*$ ]]; then
                count=$((count + 1))
                current_file="$out_dir/block_$(printf '%03d' "$count").c"
                : > "$current_file"
                in_block=true
            fi
        else
            # Closing fence: ``` (with optional trailing whitespace)
            if [[ "$line" =~ ^[\`]{3}[[:space:]]*$ ]]; then
                in_block=false
                current_file=""
            else
                printf '%s\n' "$line" >> "$current_file"
            fi
        fi
    done < "$md_file"

    echo "$count"
}

# -----------------------------------------------------------------------
# Helper: find the primary header for a wheel.
# -----------------------------------------------------------------------
find_wheel_header() {
    local wheel_dir="$1"
    local name="$2"

    if [[ -f "$wheel_dir/$name.h" ]]; then
        echo "$wheel_dir/$name.h"
        return
    fi
    if [[ -f "$wheel_dir/include/$name/$name.h" ]]; then
        echo "$wheel_dir/include/$name/$name.h"
        return
    fi
    # Fallback: first .h in wheel root
    local h
    h=$(ls "$wheel_dir"/*.h 2>/dev/null | head -1 || true)
    echo "${h:-}"
}

# -----------------------------------------------------------------------
# Helper: assemble the compile unit for a code block.
#
# We emit:
#   1. Standard includes (stdint, stddef, stdbool, string)
#   2. The wheel header (if any) — but AFTER any #define the block begins with,
#      so macros like #define LOG_MODULE "x" take effect before the include.
#   3. The code block content, wrapped in a static function body when the
#      block is a statement sequence (not a full translation unit).
# -----------------------------------------------------------------------
prepare_compile_unit() {
    local block_file="$1"
    local header_path="$2"
    local out_file="$3"

    local content
    content="$(cat "$block_file")"

    # Split the block into a "preamble" section (leading preprocessor lines that
    # must precede the wheel header) and a "body" section (everything else).
    #
    # Leading preprocessor preamble consists of:
    #   - #define lines (e.g. #define LOG_MODULE "app")
    #   - #include lines that ARE the wheel's own header (we skip them —
    #     the outer preamble already adds it)
    #
    # Anything else (including other #include, code) goes to body.
    local leading_preamble=""
    local body_content=""
    local found_body=false
    local wheel_hdr_base=""
    [[ -n "$header_path" ]] && wheel_hdr_base="$(basename "$header_path")"

    while IFS= read -r line; do
        if ! $found_body; then
            if [[ "$line" =~ ^#define[[:space:]] ]]; then
                leading_preamble+="${line}"$'\n'
            elif [[ -n "$wheel_hdr_base" && "$line" =~ ^#include.*\"${wheel_hdr_base}\" ]]; then
                # Skip — we emit this in the outer preamble already
                :
            elif [[ "$line" =~ ^[[:space:]]*$ ]]; then
                # Skip blank lines at start
                :
            else
                found_body=true
                body_content+="${line}"$'\n'
            fi
        else
            body_content+="${line}"$'\n'
        fi
    done <<< "$content"

    # Standard outer preamble (system headers + user defines + wheel header)
    {
        printf '#include <stdint.h>\n'
        printf '#include <stddef.h>\n'
        printf '#include <stdbool.h>\n'
        printf '#include <string.h>\n'
        if [[ -n "$leading_preamble" ]]; then
            printf '%s' "$leading_preamble"
        fi
        if [[ -n "$header_path" ]]; then
            printf '#include "%s"\n' "$(basename "$header_path")"
        fi
    } > "$out_file"

    # Decide wrapping strategy for the body.
    # Wrap if content looks like statement-level code (needs a function body).
    # Do NOT wrap if content looks like a translation unit:
    #   1. Starts with a function definition header (return_type name(...) {)
    #   2. Contains only preprocessor directives, comments, and declarations
    #   3. Is a typedef/struct/enum definition
    local needs_wrap=true

    # Heuristic 1: block starts at file scope with a function definition.
    # Pattern: line starting with optional storage class + type + name + '(' + ... + ')'
    # followed later by a line that is just '}'.
    # Specifically: the FIRST non-blank, non-comment, non-preprocessor line must
    # look like a function signature start (contains '(' and does NOT end with ';').
    local first_code_line=""
    while IFS= read -r fline; do
        # Skip blank lines, comments, and preprocessor directives
        if [[ -z "${fline// /}" ]] || \
           [[ "$fline" =~ ^[[:space:]]*/[/*] ]] || \
           [[ "$fline" =~ ^[[:space:]]*# ]]; then
            continue
        fi
        first_code_line="$fline"
        break
    done <<< "$body_content"

    # If the first real line looks like a function definition opener
    # (contains parens, does not end with semicolon)
    if [[ -n "$first_code_line" ]] && \
       echo "$first_code_line" | grep -qE '\(' && \
       ! echo "$first_code_line" | grep -qE ';[[:space:]]*(//.*)?$'; then
        # Check it also closes at file scope (has a '}' on its own line)
        if echo "$body_content" | grep -qE '^[}][[:space:]]*(//.*)?$|^[}][[:space:]]*$'; then
            needs_wrap=false
        fi
    fi

    # Heuristic 2: preprocessor/comment/declaration only block
    # (no lines that look like statements with operators or function calls)
    if ! echo "$body_content" | grep -qvE \
        '^[[:space:]]*(#|/[/*]|[[:space:]]*$|typedef|struct |enum |extern |[a-zA-Z_][a-zA-Z0-9_]*[[:space:]]+[a-zA-Z_][a-zA-Z0-9_]*[[:space:]]*\()'; then
        needs_wrap=false
    fi

    # Diagnostic suppressions for snippet-style code
    local diag_push
    diag_push='#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
'
    local diag_pop='#pragma GCC diagnostic pop
'

    if $needs_wrap; then
        {
            printf 'static void _readme_snippet(void) {\n'
            printf '%s' "$diag_push"
            printf '%s' "$body_content"
            printf '%s' "$diag_pop"
            printf '}\n'
        } >> "$out_file"
    else
        {
            printf '%s' "$diag_push"
            printf '%s' "$body_content"
            printf '%s' "$diag_pop"
        } >> "$out_file"
    fi
}

# -----------------------------------------------------------------------
# Error classification
#
# LENIENT errors (→ SKIP): these are expected in illustrative snippets.
#   - 'undeclared' identifier/function  → pseudo-code uses context variables
#   - 'implicit function declaration'   → functions from other TUs
#   - 'No such file or directory'       → missing dependency header
#   - 'passing argument'/'incompatible pointer type' → opaque handle coercion
#   - 'conflicting types'               → duplicate include or wrapper clash
#
# HARD errors (→ FAIL): genuine syntax problems a user would hit even with
# proper includes/declarations in place.
#   - 'expected' (unexpected token, missing semicolon, mismatched parens)
#   - 'unknown type name'               → type used but never declared anywhere
#   - 'error: expected declaration'
#   - 'error: stray'                    → stray Unicode characters
#   - 'conflicting types', 'too few/many arguments' for a function of the
#     wheel under test                  → the README disagrees with its header
#
# Heuristic: if ALL errors in a block are lenient, classify as SKIP.
# If ANY error is hard, classify as FAIL.
# -----------------------------------------------------------------------
LENIENT_PATTERNS=(
    'undeclared'
    'implicit declaration of function'
    'implicit function declaration'
    'No such file or directory'
    'passing argument'
    'incompatible pointer type'
    'conflicting types'
    'too few arguments'
    'too many arguments'
    'makes pointer from integer'
    'makes integer from pointer'
)

is_lenient_error() {
    local line="$1"
    for pat in "${LENIENT_PATTERNS[@]}"; do
        if [[ "$line" == *"$pat"* ]]; then
            return 0
        fi
    done
    return 1
}

# gcc runs under LC_ALL=C, so the name it reports sits in plain quotes.
API_MISMATCH_RE="(conflicting types for|too few arguments to function|too many arguments to function) '([A-Za-z_][A-Za-z0-9_]*)'"
HAVE_TYPE_RE="; have '([^']*)'"
PREV_TYPE_RE=" with type '([^']*)'"

is_own_api_mismatch() {
    local line="$1" wheel_dir="$2" err_file="$3"
    [[ "$line" =~ $API_MISMATCH_RE ]] || return 1
    local kind="${BASH_REMATCH[1]}" name="${BASH_REMATCH[2]}"
    grep -qE "\b${name}[[:space:]]*\(" \
        "$wheel_dir"/*.h "$wheel_dir"/include/*/*.h 2>/dev/null || return 1
    [[ "$kind" == "conflicting types for" ]] || return 0

    # A block that repeats the header's typedefs inside the snippet wrapper
    # makes even a correct prototype conflict, because the local typedef is a
    # new type with the same name. Only a difference gcc can show is stale.
    local have="" prev="" note
    [[ "$line" =~ $HAVE_TYPE_RE ]] && have="${BASH_REMATCH[1]}"
    note="$(grep -E "note: previous (declaration|definition) of '$name' with type '" \
        "$err_file" | head -1 || true)"
    [[ "$note" =~ $PREV_TYPE_RE ]] && prev="${BASH_REMATCH[1]}"
    [[ "$have" != "$prev" ]]
}

# Prints the ": error:" lines that count against the example.
hard_error_lines() {
    local err_file="$1" wheel_dir="$2" eline
    while IFS= read -r eline; do
        [[ -n "$eline" ]] || continue
        if is_own_api_mismatch "$eline" "$wheel_dir" "$err_file" || ! is_lenient_error "$eline"; then
            printf '%s\n' "$eline"
        fi
    done < <(grep ": error:" "$err_file" 2>/dev/null || true)
}

# READMEs known to disagree with their header, waiting for a fix that this
# list only postpones: nobody has reviewed or approved these. They stay SKIP
# as long as a stale API is their only hard error. Delete an entry once its
# README is fixed; the run prints a note when an entry has gone clean.
PENDING_README_FIXES=()

is_pending_readme() {
    local label="$1" entry
    for entry in "${PENDING_README_FIXES[@]}"; do
        [[ "$entry" == "$label" ]] && return 0
    done
    return 1
}

classify_errors() {
    local err_file="$1" wheel_dir="$2" wheel_label="$3"
    if ! grep -q ": error:" "$err_file" 2>/dev/null; then
        echo "pass"
        return
    fi

    local hard
    hard="$(hard_error_lines "$err_file" "$wheel_dir")"
    if [[ -z "$hard" ]]; then
        echo "skip"
    elif is_pending_readme "$wheel_label" && ! grep -qvE "$API_MISMATCH_RE" <<< "$hard"; then
        echo "pending"
    else
        echo "fail"
    fi
}

# -----------------------------------------------------------------------
# Counters
# -----------------------------------------------------------------------
WHEELS_CHECKED=0
WHEELS_WITH_BLOCKS=0
WHEELS_FAILED=0
TOTAL_BLOCKS=0
TOTAL_PASS=0
TOTAL_SKIP=0
TOTAL_FAIL=0
TOTAL_PENDING=0
pending_seen=""

# JSON accumulation (simple array of result objects)
json_results=""

json_append_result() {
    local wheel="$1" block="$2" status="$3" detail="$4"
    local entry
    entry="$(printf '{"wheel":"%s","block":%d,"status":"%s","detail":"%s"}' \
        "$wheel" "$block" "$status" "$detail")"
    if [[ -z "$json_results" ]]; then
        json_results="$entry"
    else
        json_results="${json_results},${entry}"
    fi
}

# -----------------------------------------------------------------------
# Main loop
# -----------------------------------------------------------------------
for wheel_dir in "$REPO"/wheels/*/*/; do
    [[ -d "$wheel_dir" ]] || continue
    name="$(basename "$wheel_dir")"
    layer="$(basename "$(dirname "$wheel_dir")")"

    [[ "$name" == "__pycache__" ]] && continue

    if [[ -n "$WHEEL_FILTER" && "$name" != "$WHEEL_FILTER" ]]; then
        continue
    fi

    readme="$wheel_dir/README.md"
    [[ -f "$readme" ]] || continue

    WHEELS_CHECKED=$((WHEELS_CHECKED + 1))

    # Extract C code blocks
    block_dir="$WORK_DIR/${layer}_${name}"
    mkdir -p "$block_dir"
    block_count="$(extract_c_blocks "$readme" "$block_dir")"

    [[ "$block_count" -eq 0 ]] && continue

    WHEELS_WITH_BLOCKS=$((WHEELS_WITH_BLOCKS + 1))
    TOTAL_BLOCKS=$((TOTAL_BLOCKS + block_count))

    header="$(find_wheel_header "$wheel_dir" "$name")"
    wheel_label="${layer}/${name}"
    wheel_failed=false

    if ! $QUIET; then
        echo -e "${CYAN}--- ${wheel_label} (${block_count} block(s)) ---${NC}"
    fi

    for block_file in "$block_dir"/block_*.c; do
        [[ -f "$block_file" ]] || continue
        block_num="${block_file##*block_}"
        block_num="${block_num%.c}"
        block_num=$((10#$block_num))

        unit_file="$WORK_DIR/unit_${layer}_${name}_${block_num}.c"
        prepare_compile_unit "$block_file" "$header" "$unit_file"

        # Compile flags:
        #   -fsyntax-only    — parse + typecheck, no object output
        #   -Wall            — standard warnings (for real errors)
        #   -Wno-implicit-function-declaration  — lenient on undeclared fns
        #   -I<wheel_dir>    — find the wheel's own header
        #   -I<layers>       — cross-wheel type references (best-effort)
        err_file="$WORK_DIR/err_${layer}_${name}_${block_num}.txt"
        compile_rc=0

        # A wheel's header may include a dependency's header, whose directory
        # the layer paths below do not reach.
        dep_incs=()
        if [[ -n "${header:-}" ]]; then
            while IFS= read -r inc; do
                dep_h=$(find "$REPO/wheels" -name "$inc" -not -path "*/test/*" \
                        -not -path "*/_draft/*" 2>/dev/null | head -1)
                [[ -n "$dep_h" ]] && dep_incs+=("-I$(dirname "$dep_h")")
            done < <(grep -oE '^#include "[^"]+\.h"' "$header" | sed 's/^#include "//;s/"$//')
        fi

        LC_ALL=C gcc -std=c99 -fsyntax-only -Wall \
            -Wno-implicit-function-declaration \
            -Wno-unused-function \
            ${header:+-I"$(dirname "$header")"} \
            -I"$REPO/wheels/primitives" \
            -I"$REPO/wheels/drivers" \
            -I"$REPO/wheels/protocols" \
            -I"$REPO/wheels/modules" \
            "${dep_incs[@]}" \
            "$unit_file" > /dev/null 2>"$err_file" || compile_rc=$?

        if [[ $compile_rc -eq 0 ]]; then
            classification="pass"
        else
            classification="$(classify_errors "$err_file" "$wheel_dir" "$wheel_label")"
            # If no ": error:" line at all (only warnings), treat as pass
            if [[ "$classification" == "pass" ]] && [[ $compile_rc -ne 0 ]]; then
                classification="skip"
            fi
        fi

        case "$classification" in
            pass)
                TOTAL_PASS=$((TOTAL_PASS + 1))
                if ! $QUIET; then
                    echo -e "  ${GREEN}PASS${NC}  block $block_num"
                fi
                if $JSON_MODE; then
                    json_append_result "$wheel_label" "$block_num" "PASS" ""
                fi
                ;;
            skip)
                TOTAL_SKIP=$((TOTAL_SKIP + 1))
                if ! $QUIET; then
                    skip_reason="$(grep ': error:' "$err_file" 2>/dev/null | head -1 | \
                        sed 's|.*/unit_[^.]*\.c:[0-9]*:[0-9]*: error: ||' || true)"
                    echo -e "  ${YELLOW}SKIP${NC}  block $block_num (lenient: ${skip_reason:-missing context})"
                fi
                if $JSON_MODE; then
                    detail="$(grep ': error:' "$err_file" 2>/dev/null | head -1 | sed 's/"/\\"/g' || true)"
                    json_append_result "$wheel_label" "$block_num" "SKIP" "${detail}"
                fi
                ;;
            pending)
                TOTAL_SKIP=$((TOTAL_SKIP + 1))
                TOTAL_PENDING=$((TOTAL_PENDING + 1))
                pending_seen+=" $wheel_label "
                if ! $QUIET; then
                    echo -e "  ${YELLOW}PEND${NC}  block $block_num (stale API, README fix pending)"
                fi
                if $JSON_MODE; then
                    detail="$(hard_error_lines "$err_file" "$wheel_dir" | head -1 | sed 's/"/\\"/g' || true)"
                    json_append_result "$wheel_label" "$block_num" "SKIP" "${detail}"
                fi
                ;;
            fail)
                TOTAL_FAIL=$((TOTAL_FAIL + 1))
                wheel_failed=true
                err_lines="$(hard_error_lines "$err_file" "$wheel_dir" | head -5 || true)"
                if $QUIET; then
                    echo -e "${RED}FAIL${NC}  ${wheel_label} block ${block_num}"
                    echo "$err_lines" | while IFS= read -r eline; do
                        # Strip the temp path prefix for readability
                        echo "       $(echo "$eline" | sed "s|$WORK_DIR/[^:]*:||")"
                    done
                else
                    echo -e "  ${RED}FAIL${NC}  block $block_num"
                    echo "$err_lines" | while IFS= read -r eline; do
                        echo "         $(echo "$eline" | sed "s|$WORK_DIR/[^:]*:||")"
                    done
                fi
                if $JSON_MODE; then
                    detail="$(hard_error_lines "$err_file" "$wheel_dir" | head -1 | sed 's/"/\\"/g' || true)"
                    json_append_result "$wheel_label" "$block_num" "FAIL" "${detail}"
                fi
                ;;
        esac
    done

    if $wheel_failed; then
        WHEELS_FAILED=$((WHEELS_FAILED + 1))
    fi
done

if [[ -n "$WHEEL_FILTER" && $WHEELS_CHECKED -eq 0 ]]; then
    echo "ERROR: no wheel named '$WHEEL_FILTER' with a README: nothing checked" >&2
    exit 2
fi

if [[ -z "$WHEEL_FILTER" ]] && ! $JSON_MODE; then
    for entry in "${PENDING_README_FIXES[@]}"; do
        if [[ "$pending_seen" != *" $entry "* ]]; then
            echo "NOTE: $entry is clean: delete it from PENDING_README_FIXES"
        fi
    done
fi

# -----------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------
if ! $QUIET; then
    echo ""
    echo -e "${BOLD}=== Summary ===${NC}"
    echo "  Wheels checked:          $WHEELS_CHECKED"
    echo "  Wheels with C examples:  $WHEELS_WITH_BLOCKS"
    echo "  Total C blocks:          $TOTAL_BLOCKS"
    echo -e "  ${GREEN}PASS${NC}:                    $TOTAL_PASS"
    echo -e "  ${YELLOW}SKIP${NC} (lenient):          $TOTAL_SKIP ($TOTAL_PENDING pending README fixes)"
    echo -e "  ${RED}FAIL${NC} (hard):             $TOTAL_FAIL"
    echo "  Wheels with failures:    $WHEELS_FAILED"
    echo ""
    if [[ $TOTAL_FAIL -eq 0 ]]; then
        echo -e "${GREEN}All examples pass or have only lenient issues.${NC}"
    else
        echo -e "${RED}${TOTAL_FAIL} example(s) have hard errors — see details above.${NC}"
    fi
else
    # Compact summary even in quiet mode
    echo ""
    if [[ $TOTAL_FAIL -eq 0 ]]; then
        echo -e "${GREEN}check-readme-examples: ${TOTAL_PASS} PASS, ${TOTAL_SKIP} SKIP (${TOTAL_PENDING} pending README fixes), 0 FAIL${NC}"
    else
        echo -e "${RED}check-readme-examples: ${TOTAL_PASS} PASS, ${TOTAL_SKIP} SKIP (${TOTAL_PENDING} pending README fixes), ${TOTAL_FAIL} FAIL (${WHEELS_FAILED} wheels)${NC}"
    fi
fi

if $JSON_MODE; then
    printf '{\n'
    printf '  "wheels_checked": %d,\n'       "$WHEELS_CHECKED"
    printf '  "wheels_with_blocks": %d,\n'   "$WHEELS_WITH_BLOCKS"
    printf '  "total_blocks": %d,\n'         "$TOTAL_BLOCKS"
    printf '  "pass": %d,\n'                 "$TOTAL_PASS"
    printf '  "skip": %d,\n'                 "$TOTAL_SKIP"
    printf '  "fail": %d,\n'                 "$TOTAL_FAIL"
    printf '  "wheels_failed": %d,\n'        "$WHEELS_FAILED"
    printf '  "results": [%s]\n'             "$json_results"
    printf '}\n'
fi

exit $((TOTAL_FAIL > 0 ? 1 : 0))
