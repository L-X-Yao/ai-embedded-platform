#!/bin/bash
# sync_vsf.sh — synchronize vsf/ with upstream vsfteam/vsf.
#
# Usage:
#   sync_vsf.sh                      # sync to upstream master HEAD
#   sync_vsf.sh --ref v1.2.3         # sync to specific tag/branch/commit
#   sync_vsf.sh --dry-run             # show what would change, don't write
#   sync_vsf.sh --status              # show current vs upstream versions
#
# Workflow:
#   1. Fetches upstream into a local cache (tooling/.vsf-upstream/)
#   2. Reverts any applied patches (tooling/ci/patches/apply.sh --revert)
#   3. Replaces vsf/ contents with upstream at target ref
#   4. Re-applies patches (reports conflicts if any)
#   5. Prints diff summary for user review
#
# Does NOT auto-commit. User reviews the diff and commits manually.
#
# Exit 0 on success, 1 on error, 2 on patch conflict (vsf/ updated but
# patches need manual resolution).

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
VSF_DIR="$REPO/vsf"
CACHE_DIR="$REPO/tooling/.vsf-upstream"
PATCH_SCRIPT="$REPO/tooling/ci/patches/apply.sh"
UPSTREAM_URL="https://github.com/vsfteam/vsf.git"
VERSION_FILE="$REPO/tooling/.vsf-version"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

TARGET_REF="master"
DRY_RUN=false
STATUS_ONLY=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ref)    [[ $# -ge 2 ]] || { echo "ERROR: --ref requires a value"; exit 1; }; TARGET_REF="$2"; shift 2 ;;
        --dry-run) DRY_RUN=true; shift ;;
        --status) STATUS_ONLY=true; shift ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

# --- Helpers -----------------------------------------------------------------

log_info()  { echo -e "${CYAN}[info]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[ok]${NC}    $*"; }
log_warn()  { echo -e "${YELLOW}[warn]${NC}  $*"; }
log_err()   { echo -e "${RED}[err]${NC}   $*"; }

read_current_version() {
    if [[ -f "$VERSION_FILE" ]]; then
        cat "$VERSION_FILE"
    else
        echo "(unknown — no .vsf-version file)"
    fi
}

# --- Cache management --------------------------------------------------------

ensure_cache() {
    local git_rc=0
    if [[ -d "$CACHE_DIR/.git" ]]; then
        log_info "Fetching upstream into cache..."
        git_out=$(git -C "$CACHE_DIR" fetch origin --prune --tags 2>&1) || git_rc=$?
        echo "$git_out" | sed 's/^/  /'
    else
        log_info "Cloning upstream into cache (first time, may take a while)..."
        git_out=$(git clone "$UPSTREAM_URL" "$CACHE_DIR" 2>&1) || git_rc=$?
        echo "$git_out" | sed 's/^/  /'
    fi
    if [[ $git_rc -ne 0 ]]; then
        log_err "Git operation failed (exit $git_rc)"
        exit 1
    fi
}

resolve_ref() {
    local ref="$1"
    # Try as-is first (branch, tag, commit hash)
    if git -C "$CACHE_DIR" rev-parse --verify "origin/$ref" >/dev/null 2>&1; then
        git -C "$CACHE_DIR" rev-parse "origin/$ref"
    elif git -C "$CACHE_DIR" rev-parse --verify "$ref" >/dev/null 2>&1; then
        git -C "$CACHE_DIR" rev-parse "$ref"
    else
        log_err "Cannot resolve ref '$ref' in upstream"
        exit 1
    fi
}

# --- Status mode -------------------------------------------------------------

if $STATUS_ONLY; then
    echo -e "${BOLD}=== VSF upstream sync status ===${NC}"
    echo ""
    echo -e "  ${BOLD}Current version:${NC} $(read_current_version)"
    if [[ -d "$CACHE_DIR/.git" ]]; then
        git -C "$CACHE_DIR" fetch origin --prune --tags >/dev/null 2>&1 || true
        upstream_head=$(git -C "$CACHE_DIR" rev-parse origin/master 2>/dev/null || echo "unknown")
        upstream_date=$(git -C "$CACHE_DIR" log -1 --format="%ci" origin/master 2>/dev/null || echo "unknown")
        echo -e "  ${BOLD}Upstream HEAD:${NC}   ${upstream_head:0:12} ($upstream_date)"

        current_hash=$(read_current_version | grep -oE '[a-f0-9]{7,40}' | head -1 || true)
        if [[ -n "$current_hash" ]] && [[ "$upstream_head" != "$current_hash"* ]]; then
            behind=$(git -C "$CACHE_DIR" rev-list --count "$current_hash..origin/master" 2>/dev/null || echo "?")
            echo -e "  ${YELLOW}Behind upstream by ~$behind commits${NC}"
        elif [[ -n "$current_hash" ]]; then
            echo -e "  ${GREEN}Up to date${NC}"
        fi
    else
        echo -e "  ${YELLOW}No local cache yet. Run sync_vsf.sh once to populate.${NC}"
    fi

    shopt -s nullglob
    _patches=("$REPO/tooling/ci/patches/"[0-9]*-*.patch)
    shopt -u nullglob
    patch_count=${#_patches[@]}
    echo -e "  ${BOLD}Local patches:${NC}   $patch_count"
    exit 0
fi

# --- Pre-flight checks -------------------------------------------------------

if [[ ! -d "$VSF_DIR" ]]; then
    log_err "vsf/ directory not found at $VSF_DIR"
    exit 1
fi

# Check for uncommitted changes in vsf/
if ! git -C "$REPO" diff --quiet -- vsf/; then
    log_err "vsf/ has uncommitted changes. Commit or stash first."
    git -C "$REPO" diff --stat -- vsf/ | head -10
    exit 1
fi

# Check for untracked files in vsf/ (would be silently deleted during sync)
untracked_vsf=$(git -C "$REPO" ls-files --others --exclude-standard -- vsf/ 2>/dev/null || true)
if [[ -n "$untracked_vsf" ]]; then
    log_err "vsf/ has untracked files that would be lost during sync:"
    echo "$untracked_vsf" | head -5 | sed 's/^/  /'
    log_err "Commit or remove them first."
    exit 1
fi

# --- Main flow ---------------------------------------------------------------

echo -e "${BOLD}=== VSF upstream sync ===${NC}"
echo ""

# 1. Ensure cache
ensure_cache
echo ""

# 2. Resolve target
target_hash=$(resolve_ref "$TARGET_REF")
target_short="${target_hash:0:12}"
target_date=$(git -C "$CACHE_DIR" log -1 --format="%ci" "$target_hash")
target_subject=$(git -C "$CACHE_DIR" log -1 --format="%s" "$target_hash")

log_info "Target: $target_short ($target_date)"
log_info "        $target_subject"
echo ""

current_version=$(read_current_version)
log_info "Current: $current_version"
echo ""

if $DRY_RUN; then
    echo -e "${BOLD}--- Dry run: comparing current vsf/ with upstream $target_short ---${NC}"
    echo ""

    # Checkout target in cache
    git -C "$CACHE_DIR" checkout --quiet "$target_hash" 2>/dev/null || true

    # Compare using rsync dry-run (shows what would change)
    # Use diff on file lists as a proxy
    local_files=$(cd "$VSF_DIR" && find . -type f ! -path './.git/*' | sort)
    upstream_files=$(cd "$CACHE_DIR" && find . -type f ! -path './.git/*' ! -path './.git' | sort)

    added=$(comm -13 <(echo "$local_files") <(echo "$upstream_files") | wc -l)
    removed=$(comm -23 <(echo "$local_files") <(echo "$upstream_files") | wc -l)
    common=$(comm -12 <(echo "$local_files") <(echo "$upstream_files") | wc -l)

    echo -e "  Files in upstream only (would add):    ${GREEN}+$added${NC}"
    echo -e "  Files in local only (would remove):    ${RED}-$removed${NC}"
    echo -e "  Common files (may have changes):       $common"
    echo ""
    echo -e "  Run without --dry-run to apply."
    exit 0
fi

# 3. Revert patches (if any applied)
if [[ -x "$PATCH_SCRIPT" ]]; then
    shopt -s nullglob
    _patches=("$REPO/tooling/ci/patches/"[0-9]*-*.patch)
    shopt -u nullglob
    patch_count=${#_patches[@]}
    if [[ "$patch_count" -gt 0 ]]; then
        log_info "Reverting $patch_count patches before sync..."
        revert_rc=0
        revert_out=$(bash "$PATCH_SCRIPT" --revert 2>&1) || revert_rc=$?
        echo "$revert_out" | sed 's/^/  /'
        if [[ $revert_rc -ne 0 ]]; then
            log_err "Patch revert failed (exit $revert_rc) — aborting sync"
            exit 1
        fi
    fi
fi

# 4. Checkout target in cache
log_info "Checking out upstream $target_short..."
git -C "$CACHE_DIR" checkout --quiet "$target_hash" 2>/dev/null || \
    git -C "$CACHE_DIR" checkout --quiet --detach "$target_hash"

# 5. Sync vsf/ contents
log_info "Syncing vsf/ contents..."

# Sanity check: VSF_DIR must end with /vsf
[[ "$VSF_DIR" == *"/vsf" ]] || { log_err "VSF_DIR sanity check failed: $VSF_DIR"; exit 1; }

# Delete everything in vsf/ except .git (there shouldn't be one, but safe)
find "$VSF_DIR" -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} +

# Copy upstream contents (exclude .git)
rsync -a --exclude='.git' "$CACHE_DIR/" "$VSF_DIR/"

log_ok "vsf/ updated to upstream $target_short"

# 6. Record version
echo "$target_short ($target_date) $target_subject" > "$VERSION_FILE"
log_ok "Version recorded in tooling/.vsf-version"

# 7. Show diff summary
echo ""
echo -e "${BOLD}--- Changes in vsf/ ---${NC}"
git -C "$REPO" diff --stat -- vsf/ | tail -5
echo ""

added=$(git -C "$REPO" diff --numstat -- vsf/ | awk '{s+=$1} END {print s+0}')
removed=$(git -C "$REPO" diff --numstat -- vsf/ | awk '{s+=$2} END {print s+0}')
new_files=$(git -C "$REPO" ls-files --others --exclude-standard -- vsf/ | wc -l)
deleted_files=$(git -C "$REPO" diff --name-only --diff-filter=D -- vsf/ | wc -l)

echo -e "  Lines: ${GREEN}+$added${NC} / ${RED}-$removed${NC}"
echo -e "  New files: $new_files"
echo -e "  Deleted files: $deleted_files"

# 8. Re-apply patches
patch_conflict=false
if [[ -x "$PATCH_SCRIPT" ]]; then
    shopt -s nullglob
    _patches=("$REPO/tooling/ci/patches/"[0-9]*-*.patch)
    shopt -u nullglob
    patch_count=${#_patches[@]}
    if [[ "$patch_count" -gt 0 ]]; then
        echo ""
        log_info "Re-applying $patch_count patches..."
        apply_rc=0
        apply_out=$(bash "$PATCH_SCRIPT" 2>&1) || apply_rc=$?
        echo "$apply_out" | sed 's/^/  /'
        if [[ $apply_rc -eq 0 ]]; then
            log_ok "All patches applied cleanly"
        else
            log_warn "Some patches failed — manual resolution needed"
            log_warn "Check tooling/ci/patches/*.rej for conflicts"
            patch_conflict=true
        fi
    fi
fi

# 9. Final instructions
echo ""
echo -e "${BOLD}=== Next steps ===${NC}"
echo "  1. Review changes:  git diff --stat -- vsf/"
echo "  2. Run wheel tests: make test-wheels"
step=3
if $patch_conflict; then
    echo "  $step. Fix patch conflicts in tooling/ci/patches/"
    step=$((step + 1))
fi
echo "  $step. Commit:          git add vsf/ tooling/.vsf-version && git commit"
echo ""
echo -e "  Suggested commit message:"
echo -e "    ${CYAN}wheel: sync vsf upstream to $target_short${NC}"
echo ""

if $patch_conflict; then
    exit 2
fi
exit 0
