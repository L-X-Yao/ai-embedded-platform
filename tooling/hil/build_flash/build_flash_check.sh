#!/bin/bash
# Generic build → flash → serial check pipeline driven by a project conf file.
# Replaces project-specific orchestration scripts (e.g. projects/flybox/scripts/build_flash.sh).
#
# Usage:
#   build_flash_check.sh --conf <path> [build|flash|check|all] [SECONDS]
#
#   build       — run BUILD_CMD then optional COPY_CMD + size check
#   flash       — openocd write_image + verify_image at FLASH_ADDR
#   check [sec] — capture serial via debug/gd32f4xx/serial_capture.ps1, verdict
#                  by FAIL_PATTERNS / PASS_PATTERNS in conf
#   all         — build + flash + check (default)
#
# Conf file is sourced as bash. See example.conf for full schema.
# Required keys: BIN_PATH, FLASH_ADDR, SERIAL_PORT (for check),
#                FAIL_PATTERNS (array), PASS_PATTERNS (array), MIN_PASSED.
#
# Reuses tooling/debug/cortex_m/gd32f4xx/serial_capture.ps1 for serial capture
# (override SERIAL_CAPTURE_SCRIPT if you ship your own).

set -euo pipefail

# Escape a string for embedding in a JSON string literal.
json_escape() {
    local s="$1"
    s="${s//\\/\\\\}"    # backslash first
    s="${s//\"/\\\"}"    # double-quote
    s="${s//$'\n'/\\n}"  # newline
    s="${s//$'\t'/\\t}"  # tab
    printf '%s' "$s"
}

usage() {
    cat <<EOF
Usage: $0 --conf <path> [build|flash|check [sec]|all]

  --conf PATH         Project conf file (sourced as bash)
  build               Build + optional copy + size check
  flash               OpenOCD write_image + verify_image
  check [SECONDS]     Reset + capture serial + verdict (default: \$CHECK_DURATION)
  all                 build + flash + check (default action)

See $(dirname "$0")/example.conf for conf schema and full key list.
EOF
    exit 1
}

CONF=""
ACTION="all"
DURATION_OVERRIDE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --conf)
            CONF="${2:-}"
            [[ -z "$CONF" ]] && usage
            shift 2
            ;;
        build|flash|check|all)
            ACTION="$1"
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            if [[ "$ACTION" == "check" && "$1" =~ ^[0-9]+$ ]]; then
                DURATION_OVERRIDE="$1"
                shift
            else
                echo "[ERR] unknown arg: $1" >&2
                usage
            fi
            ;;
    esac
done

[[ -z "$CONF" ]] && { echo "[ERR] --conf is required" >&2; usage; }
[[ -f "$CONF" ]] || { echo "[ERR] conf not found: $CONF" >&2; exit 1; }

# Defaults (overridable in conf)
OCD_INTERFACE="interface/cmsis-dap.cfg"
OCD_TARGET="target/stm32f4x.cfg"
OCD_SPEED="1000"
CHECK_DURATION="30"
SERIAL_PORT=""
SERIAL_LOG="/tmp/build_flash_check_serial.log"
MIN_PASSED=""
SRAM_LIMIT=""
CCM_OFFSET="0"
SRAM_RESERVE="0"
BUILD_CMD=""
COPY_CMD=""
ELF_PATH=""
FAIL_PATTERNS=()
PASS_PATTERNS=()
PASS_PATTERNS_INFO=()

# Anchor for relative paths in conf (e.g. SERIAL_CAPTURE_SCRIPT defaults)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONF_DIR="$(cd "$(dirname "$CONF")" && pwd)"
SERIAL_CAPTURE_SCRIPT="$SCRIPT_DIR/../../debug/gd32f4xx/serial_capture.ps1"

# Validate CONF is within the repo to prevent sourcing arbitrary files
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CONF_REAL="$(realpath "$CONF")"
case "$CONF_REAL" in
    "$REPO_ROOT"/*)
        ;;
    *)
        echo "[ERR] conf file must be within repo: $CONF_REAL" >&2
        exit 1
        ;;
esac

# shellcheck disable=SC1090
. "$CONF"

# Required keys
[[ -z "${BIN_PATH:-}" ]]    && { echo "[ERR] BIN_PATH not set in conf" >&2; exit 1; }
[[ -z "${FLASH_ADDR:-}" ]]  && { echo "[ERR] FLASH_ADDR not set in conf" >&2; exit 1; }

[[ -n "$DURATION_OVERRIDE" ]] && CHECK_DURATION="$DURATION_OVERRIDE"

do_build() {
    if [[ -z "$BUILD_CMD" ]]; then
        echo "[SKIP build] no BUILD_CMD in conf"
        return 0
    fi
    echo "=== BUILD ==="
    eval "$BUILD_CMD"
    if [[ -n "$COPY_CMD" ]]; then
        eval "$COPY_CMD"
    fi

    # Optional size check (needs ELF_PATH or BIN_PATH→.elf)
    if [[ -n "$SRAM_LIMIT" ]]; then
        local elf="${ELF_PATH:-${BIN_PATH%.bin}.elf}"
        if [[ -f "$elf" ]]; then
            echo "=== SIZE ==="
            local size_line text data bss main_bss sram_used sram_free
            size_line=$(arm-none-eabi-size "$elf" | tail -1)
            text=$(echo "$size_line" | awk '{print $1}')
            data=$(echo "$size_line" | awk '{print $2}')
            bss=$(echo  "$size_line" | awk '{print $3}')
            main_bss=$((bss - CCM_OFFSET))
            [[ $main_bss -lt 0 ]] && main_bss=$bss
            sram_used=$((data + main_bss))
            sram_free=$((SRAM_LIMIT - sram_used - SRAM_RESERVE))
            echo "  text=${text} data=${data} bss=${bss} (main ~${main_bss} + CCM ${CCM_OFFSET})"
            echo "  Main SRAM: used=${sram_used} free=~${sram_free} (post-reserve ${SRAM_RESERVE})"
            if [[ $sram_free -lt 20480 ]]; then
                echo "  [WARN] SRAM free < 20KB — thread stacks may not fit"
            fi
        else
            echo "[SKIP size] ELF not found: $elf"
        fi
    fi
}

do_flash() {
    echo "=== FLASH ==="
    [[ -f "$BIN_PATH" ]] || { echo "[ERR] BIN not found: $BIN_PATH" >&2; exit 1; }
    local output
    output=$(openocd \
        -f "$OCD_INTERFACE" \
        -c "adapter speed $OCD_SPEED" \
        -f "$OCD_TARGET" \
        -c "cortex_m reset_config sysresetreq" \
        -c "init" \
        -c "reset halt" \
        -c "stm32f4x unlock 0" \
        -c "reset halt" \
        -c "flash write_image erase $BIN_PATH $FLASH_ADDR bin" \
        -c "verify_image $BIN_PATH $FLASH_ADDR bin" \
        -c "reset run" \
        -c "shutdown" 2>&1) || true    # the verified check below decides
    if echo "$output" | grep -q "verified"; then
        echo "$output" | grep "verified"
        echo "[OK] Flash + verify done"
    else
        echo "[FAIL] Verify failed:"
        echo "$output" | tail -15
        return 1
    fi
}

do_check() {
    [[ -z "$SERIAL_PORT" ]] && { echo "[ERR] SERIAL_PORT not set in conf" >&2; exit 1; }
    # Without these any log would pass, an empty one included.
    if [[ ${#FAIL_PATTERNS[@]} -eq 0 || ${#PASS_PATTERNS[@]} -eq 0 || -z "$MIN_PASSED" ]]; then
        echo "[ERR] check needs FAIL_PATTERNS, PASS_PATTERNS and MIN_PASSED in conf" >&2
        exit 1
    fi
    [[ -f "$SERIAL_CAPTURE_SCRIPT" ]] || {
        echo "[ERR] capture script not found: $SERIAL_CAPTURE_SCRIPT" >&2; exit 1; }
    echo "=== SERIAL CHECK (${CHECK_DURATION}s) ==="
    powershell -NoProfile -ExecutionPolicy Bypass -File "$SERIAL_CAPTURE_SCRIPT" \
        -Port "$SERIAL_PORT" -WaitSec "$CHECK_DURATION" -LogFile "$SERIAL_LOG" \
        -OcdInterface "$OCD_INTERFACE" -OcdTarget "$OCD_TARGET" -OcdSpeed "$OCD_SPEED"
    do_verdict
}

do_verdict() {
    [[ -f "$SERIAL_LOG" ]] || { echo "[ERR] no serial log: $SERIAL_LOG" >&2; return 1; }

    echo "--- last 50 lines of $SERIAL_LOG ---"
    tail -50 "$SERIAL_LOG"
    echo "--- verdict ---"

    local log
    log=$(cat "$SERIAL_LOG")
    local ok=1

    if [[ ! -s "$SERIAL_LOG" ]]; then
        echo "[FAIL] serial log is empty"
        ok=0
    fi

    if [[ ${#FAIL_PATTERNS[@]} -gt 0 ]]; then
        for pat in "${FAIL_PATTERNS[@]}"; do
            if echo "$log" | grep -qiE "$pat"; then
                echo "[FAIL] matched fail pattern: $pat"
                ok=0
            fi
        done
    fi

    local passed=0
    if [[ ${#PASS_PATTERNS[@]} -gt 0 ]]; then
        for pat in "${PASS_PATTERNS[@]}"; do
            if echo "$log" | grep -qE "$pat"; then
                echo "[OK] $pat"
                passed=$((passed + 1))
            else
                echo "[--] $pat"
            fi
        done
        echo "  Passed: $passed/${#PASS_PATTERNS[@]} (min required: $MIN_PASSED)"
        if [[ $passed -lt $MIN_PASSED ]]; then
            ok=0
        fi
    fi

    if [[ ${#PASS_PATTERNS_INFO[@]} -gt 0 ]]; then
        for pat in "${PASS_PATTERNS_INFO[@]}"; do
            if echo "$log" | grep -qE "$pat"; then
                echo "[INFO] $pat"
            fi
        done
    fi

    # Structured JSON verdict — faster for AI analysis than grepping raw log
    local json_file="${SERIAL_LOG%.log}_verdict.json"
    local wdg_count mqtt_connect_count rdy_count online_count total_lines
    total_lines=$(wc -l < "$SERIAL_LOG" 2>/dev/null || echo 0)
    wdg_count=$(echo "$log" | grep -ciE '\[WDG\]' || true)
    mqtt_connect_count=$(echo "$log" | grep -ciE 'MQTT Session Established|CONNACK' || true)
    rdy_count=$(echo "$log" | grep -ciE 'Air RDY|RDY detected' || true)
    online_count=$(echo "$log" | grep -ciE '"online"' || true)

    local fault_count hardfault_lines fault_lines_json
    fault_count=$(echo "$log" | grep -ciE 'HardFault|BusFault|MemManage|UsageFault' || true)
    hardfault_lines=$(echo "$log" | grep -iE 'HardFault|BusFault|MemManage|UsageFault' | head -3 || true)

    # Build fault_lines JSON array with proper escaping before the heredoc
    if [[ -z "$hardfault_lines" ]]; then
        fault_lines_json="[]"
    else
        fault_lines_json="["
        first=1
        while IFS= read -r fl; do
            [[ $first -eq 0 ]] && fault_lines_json+=","
            fault_lines_json+="\"$(json_escape "$fl")\""
            first=0
        done <<< "$hardfault_lines"
        fault_lines_json+="]"
    fi

    cat > "$json_file" << JSONEOF
{
  "verdict": "$([ $ok -eq 1 ] && echo PASS || echo FAIL)",
  "duration_sec": $CHECK_DURATION,
  "total_lines": $total_lines,
  "pass_matched": $passed,
  "pass_required": $MIN_PASSED,
  "pass_total": ${#PASS_PATTERNS[@]},
  "counters": {
    "watchdog_reset": $wdg_count,
    "hardfault": $fault_count,
    "mqtt_connect": $mqtt_connect_count,
    "air_rdy": $rdy_count,
    "online_msg": $online_count
  },
  "fault_lines": $fault_lines_json
}
JSONEOF
    echo ""
    echo "--- verdict JSON: $json_file ---"
    cat "$json_file"

    if [[ $ok -eq 1 ]]; then
        return 0
    else
        return 1
    fi
}

case "$ACTION" in
    build) do_build ;;
    flash) do_flash ;;
    check) do_check ;;
    all)
        # One statement each: in 'do_build && do_flash' set -e is off inside
        # do_build, so a failed BUILD_CMD would flash the old binary.
        do_build
        do_flash
        do_check
        ;;
    *)     usage ;;
esac
