#!/bin/bash
# _arm_audit_common.sh — shared wheel discovery + include resolution for ARM audit scripts.
#
# Sourced by arm_size_audit.sh and arm_stack_audit.sh to avoid duplicating
# the wheel-enumeration loop and CMakeLists.txt dependency parsing.
#
# Exports:
#   Variables:  ARM_AUDIT_REPO, ARM_AUDIT_TOOLING_HARNESS, ARM_AUDIT_CC
#   Functions:  arm_audit_check_toolchain()
#               discover_wheel_sources()
#               resolve_includes()

set -euo pipefail

# ── Common variables ──────────────────────────────────────────────────

ARM_AUDIT_REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARM_AUDIT_TOOLING_HARNESS="$ARM_AUDIT_REPO/tooling/harness"
ARM_AUDIT_CC="arm-none-eabi-gcc"

# ── arm_audit_check_toolchain ─────────────────────────────────────────
# Verify arm-none-eabi-gcc is on PATH.  Exits 1 with install hints if missing.

arm_audit_check_toolchain() {
    if command -v "$ARM_AUDIT_CC" &>/dev/null; then
        return 0
    fi
    echo "ERROR: $ARM_AUDIT_CC not found in PATH." >&2
    echo "" >&2
    echo "Install the ARM embedded toolchain:" >&2
    echo "  Ubuntu/Debian:  sudo apt install gcc-arm-none-eabi" >&2
    echo "  macOS (brew):   brew install --cask gcc-arm-embedded" >&2
    echo "  Windows:        https://developer.arm.com/downloads/-/gnu-rm" >&2
    echo "  Manual:         https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads" >&2
    exit 1
}

# ── discover_wheel_sources ────────────────────────────────────────────
# Populate caller's "sources" array with .c files for one wheel.
#
# Arguments:
#   $1  — wheel directory (absolute path)
#   $2  — "skip_tests" to exclude test_*.c / prop_*.c / verify_*.c,
#          or "all" to include every .c file
#
# On return the caller's `sources` array is populated (via nameref).
# Returns 0 if at least one source was found, 1 otherwise.

discover_wheel_sources() {
    local wheel_dir="$1"
    local mode="${2:-all}"
    # Nameref: caller must declare `sources=()` before calling.
    local -n _out_sources=sources

    _out_sources=()

    # Vendor sources (vendor/src/*.c)
    if [[ -d "$wheel_dir/vendor/src" ]]; then
        while IFS= read -r f; do
            if [[ "$mode" == "skip_tests" ]]; then
                local base
                base=$(basename "$f")
                [[ "$base" == test_* || "$base" == prop_* || "$base" == verify_* ]] && continue
            fi
            _out_sources+=("$f")
        done < <(find "$wheel_dir/vendor/src" -name '*.c' 2>/dev/null)
    fi

    # Root-level sources (*.c at wheel top)
    while IFS= read -r f; do
        if [[ "$mode" == "skip_tests" ]]; then
            local base
            base=$(basename "$f")
            [[ "$base" == test_* || "$base" == prop_* || "$base" == verify_* ]] && continue
        fi
        _out_sources+=("$f")
    done < <(find "$wheel_dir" -maxdepth 1 -name '*.c' 2>/dev/null)

    [[ ${#_out_sources[@]} -gt 0 ]]
}

# ── resolve_includes ──────────────────────────────────────────────────
# Build the -I flag string for compiling a wheel.
#
# Arguments:
#   $1  — wheel directory (absolute path)
#   $@  — extra include dirs to append (optional, e.g. a stubs dir)
#
# Prints the combined -I flags to stdout (caller captures via $()).

resolve_includes() {
    local wheel_dir="$1"
    shift
    local includes="-I$wheel_dir -I$ARM_AUDIT_TOOLING_HARNESS"

    # Extra dirs passed by caller (e.g. config stubs)
    local extra
    for extra in "$@"; do
        [[ -n "$extra" ]] && includes="$includes -I$extra"
    done

    # Vendor headers
    if [[ -d "$wheel_dir/vendor/inc" ]]; then
        includes="$includes -I$wheel_dir/vendor/inc"
    fi

    # Cross-wheel dependencies: auto-discover from CMakeLists.txt or test/CMakeLists.txt
    local cmake_file=""
    if [[ -f "$wheel_dir/CMakeLists.txt" ]]; then
        cmake_file="$wheel_dir/CMakeLists.txt"
    elif [[ -f "$wheel_dir/test/CMakeLists.txt" ]]; then
        cmake_file="$wheel_dir/test/CMakeLists.txt"
    fi
    if [[ -n "$cmake_file" ]]; then
        while IFS= read -r dep_name; do
            [[ -z "$dep_name" ]] && continue
            local dep_dir
            dep_dir=$(find "$ARM_AUDIT_REPO/wheels" -maxdepth 3 -type d -name "$dep_name" 2>/dev/null | head -1)
            [[ -n "$dep_dir" ]] && includes="$includes -I$dep_dir"
        done < <(grep -oE '(primitives|drivers|protocols|modules)/[a-z_0-9]+' "$cmake_file" 2>/dev/null \
            | sed 's|.*/||' | sort -u || true)
    fi

    # Vendor sub-deps: kvs needs fal headers
    if [[ -d "$wheel_dir/vendor/inc" ]] && grep -rq '#include.*fal\.h' "$wheel_dir/vendor" 2>/dev/null; then
        includes="$includes -I$ARM_AUDIT_REPO/wheels/primitives/fal"
    fi

    echo "$includes"
}
