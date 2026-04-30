#!/bin/bash
# session_bootstrap.sh — dump project context for AI session start.
#
# One command replaces 5-8 tool calls. Run at conversation start or after
# context compression to quickly orient.
#
# Usage:
#   session_bootstrap.sh           # full dump
#   session_bootstrap.sh --short   # git + dirty only (for mid-session refresh)

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
CI_YAML="$REPO/.github/workflows/ci.yml"

BOLD='\033[1m'
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

SHORT=false
[[ "${1:-}" == "--short" ]] && SHORT=true

# --- Git state ----------------------------------------------------------------

echo -e "${BOLD}=== Git ===${NC}"
branch=$(git -C "$REPO" branch --show-current 2>/dev/null || echo "detached")
echo "  Branch: $branch"

ahead=$(git -C "$REPO" rev-list --count @{u}..HEAD 2>/dev/null || echo "?")
behind=$(git -C "$REPO" rev-list --count HEAD..@{u} 2>/dev/null || echo "?")
if [[ "$ahead" != "0" || "$behind" != "0" ]]; then
    echo -e "  Remote: ${YELLOW}+${ahead}/-${behind}${NC} vs upstream"
else
    echo -e "  Remote: ${GREEN}up to date${NC}"
fi

dirty=$(git -C "$REPO" status --porcelain 2>/dev/null | head -20)
if [[ -n "$dirty" ]]; then
    count=$(echo "$dirty" | wc -l)
    echo -e "  Dirty files: ${YELLOW}${count}${NC}"
    echo "$dirty" | sed 's/^/    /'
else
    echo -e "  Working tree: ${GREEN}clean${NC}"
fi

echo ""
echo -e "${BOLD}Recent commits (5):${NC}"
git -C "$REPO" log --oneline -5 | sed 's/^/  /'

$SHORT && { echo ""; echo "(use without --short for full wheel/project/audit dump)"; exit 0; }

# --- Wheel inventory ----------------------------------------------------------

echo ""
echo -e "${BOLD}=== Wheels ===${NC}"
printf "  ${CYAN}%-20s %-12s %-7s %-4s %-8s${NC}\n" "NAME" "LAYER" "ASSERTS" "CI" "CONSUMER"

for wheel_dir in "$REPO"/wheels/*/*/ ; do
    [[ ! -d "$wheel_dir" ]] && continue
    name=$(basename "$wheel_dir")
    layer_dir=$(dirname "$wheel_dir")
    layer=$(basename "$layer_dir")

    # Skip special dirs
    [[ "$name" == "_draft" || "$name" == "__pycache__" ]] && continue
    [[ "$layer" == "_draft" ]] && layer="draft"

    # Count assertions across all test files (ASSERT / TEST macros)
    test_count="0"
    tc=0
    for tf in "$wheel_dir"/test/test_*.c "$wheel_dir"/test_*.c; do
        if [[ -f "$tf" ]]; then
            n=$(grep -cE 'ASSERT\(|TEST\(|CHECK\(' "$tf" 2>/dev/null || true)
            tc=$((tc + n))
        fi
    done
    test_count="$tc"

    # CI job? (use actual dir layer for path matching, not display label)
    ci="--"
    ci_layer=$(basename "$layer_dir")
    if grep -q "wheels/${ci_layer}/${name}" "$CI_YAML" 2>/dev/null; then
        ci="yes"
    fi

    # Consumer? (project CMakeLists.txt or #include in .c files)
    consumer="--"
    if grep -rqE "wheels/${ci_layer}/${name}|wheels/[a-z_]*/${name}" "$REPO"/projects/*/CMakeLists.txt "$REPO"/projects/*/vsf_build/CMakeLists.txt 2>/dev/null; then
        consumer="yes"
    elif grep -rq "#include.*[\"<]${name}\.h[\">]" "$REPO"/projects/ 2>/dev/null; then
        consumer="yes"
    fi

    # Color coding
    ci_color="$RED"
    [[ "$ci" == "yes" ]] && ci_color="$GREEN"
    [[ "$layer" == "draft" ]] && ci_color="$YELLOW"

    cons_color="$RED"
    [[ "$consumer" == "yes" ]] && cons_color="$GREEN"
    [[ "$layer" == "primitives" || "$layer" == "draft" ]] && cons_color="$YELLOW"

    printf "  %-20s %-12s %-6s ${ci_color}%-4s${NC} ${cons_color}%-8s${NC}\n" \
        "$name" "$layer" "$test_count" "$ci" "$consumer"
done

# --- Project summary ----------------------------------------------------------

echo ""
echo -e "${BOLD}=== Projects ===${NC}"

for proj_claude in "$REPO"/projects/*/CLAUDE.md; do
    [[ ! -f "$proj_claude" ]] && continue
    proj_dir=$(dirname "$proj_claude")
    proj_name=$(basename "$proj_dir")

    # Count source files
    src_count=$(find "$proj_dir/src" -name '*.c' 2>/dev/null | wc -l || echo 0)
    # Count known issues (lines starting with - in 已知问题 section)
    issues=$(grep -c '^\- ' "$proj_dir/docs/PRE_LAUNCH_TODO.md" 2>/dev/null || echo "?")

    echo -e "  ${CYAN}${proj_name}${NC}: ${src_count} .c files, ${issues} pre-launch items"
done

# --- Audit quick check --------------------------------------------------------

echo ""
echo -e "${BOLD}=== Wheel Audit ===${NC}"
audit_exit=0
audit_out=$(timeout 15 python3 "$REPO/tooling/scripts/audit_wheels.py" 2>&1) || audit_exit=$?
if [[ $audit_exit -eq 0 ]]; then
    echo -e "  ${GREEN}All checks passed${NC}"
elif [[ $audit_exit -eq 124 ]]; then
    echo -e "  ${YELLOW}Timed out (15s)${NC} — run manually: python3 tooling/scripts/audit_wheels.py"
else
    echo -e "  ${RED}FAILURES:${NC}"
    echo "$audit_out" | sed 's/^/    /'
fi

echo ""
echo -e "${BOLD}=== CI ===${NC}"
yaml_jobs=$(grep -cE '^    name: ' "$CI_YAML" 2>/dev/null || echo 0)
matrix_entries=$(grep -cE '^\s+- \{ name:' "$CI_YAML" 2>/dev/null || echo 0)
if [[ "$matrix_entries" -gt 0 ]]; then
    job_count=$((yaml_jobs + matrix_entries - 1))
else
    job_count=$yaml_jobs
fi
echo "  Jobs in ci.yml: $job_count ($yaml_jobs definitions, $matrix_entries matrix entries)"

last_run=$(git -C "$REPO" log --oneline -1 --format="%h %s" -- .github/workflows/ 2>/dev/null || echo "n/a")
echo "  Last CI change: $last_run"

# --- Audit trigger detection --------------------------------------------------

echo ""
echo -e "${BOLD}=== Audit Triggers ===${NC}"
last_audit_commit=$(git -C "$REPO" log --oneline --grep="audit history" -1 --format=%H 2>/dev/null || echo "")
if [ -n "$last_audit_commit" ]; then
    wheel_c_changed=$(git -C "$REPO" diff --name-only "$last_audit_commit" HEAD -- wheels/ 2>/dev/null | grep "\.c$" | wc -l | xargs)
    new_wheels=$(git -C "$REPO" diff --name-only "$last_audit_commit" HEAD -- wheels/ 2>/dev/null | grep "README.md" | wc -l | xargs)
    days_since=$(( ($(date +%s) - $(git -C "$REPO" log -1 --format=%ct "$last_audit_commit" 2>/dev/null || echo "$(date +%s)")) / 86400 ))
    triggers=""
    if [ "$wheel_c_changed" -gt 10 ] 2>/dev/null; then
        triggers="${triggers}\n  ${YELLOW}▶ $wheel_c_changed wheel .c files changed since last audit${NC}"
    fi
    if [ "$new_wheels" -gt 0 ] 2>/dev/null; then
        triggers="${triggers}\n  ${YELLOW}▶ $new_wheels new wheel(s) added since last audit${NC}"
    fi
    if [ "$days_since" -gt 7 ] 2>/dev/null; then
        triggers="${triggers}\n  ${YELLOW}▶ $days_since days since last audit${NC}"
    fi
    if [ -n "$triggers" ]; then
        echo -e "$triggers"
    else
        echo -e "  ${GREEN}No triggers (last audit: $days_since days ago, $wheel_c_changed files changed)${NC}"
    fi
else
    echo -e "  ${YELLOW}No audit history commit found${NC}"
fi

# --- Session state (if exists) ------------------------------------------------

if [ -f "$REPO/docs/session-state.md" ]; then
    echo ""
    echo -e "${BOLD}=== Session State ===${NC}"
    grep -E "^\- " "$REPO/docs/session-state.md" 2>/dev/null | head -8 | sed 's/^/  /'
fi

# --- Recent playbook entries --------------------------------------------------

PLAYBOOK="$REPO/docs/debugging-playbook.md"
echo ""
echo -e "${BOLD}=== Recent Playbook Entries ===${NC}"
if [[ -f "$PLAYBOOK" ]]; then
    # Show section headers (## lines) as a quick index
    grep -n '^## ' "$PLAYBOOK" | tail -10 | sed 's/^/  /'
    echo -e "  ${CYAN}Full: docs/debugging-playbook.md${NC}"
else
    echo "  (not found)"
fi
