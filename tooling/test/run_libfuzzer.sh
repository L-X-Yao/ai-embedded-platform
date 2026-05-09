#!/usr/bin/env bash
# Run coverage-guided LibFuzzer harnesses for protocol parsers
#
# Requires: clang with libFuzzer support (clang >= 6.0)
# Usage:
#   bash tooling/test/run_libfuzzer.sh              # all protocols, 10s each
#   bash tooling/test/run_libfuzzer.sh --time 30     # 30s per protocol
#   bash tooling/test/run_libfuzzer.sh mqtt_packet   # single protocol
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="/tmp/libfuzzer_build"
CORPUS_DIR="/tmp/libfuzzer_corpus"
MAX_TIME=10
SINGLE=""

# ── Parse arguments ──────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --time) MAX_TIME="$2"; shift 2 ;;
        *)      SINGLE="$1"; shift ;;
    esac
done

# ── Check for clang ──────────────────────────────────────────────────
if ! command -v clang >/dev/null 2>&1; then
    echo "ERROR: clang not found in PATH. LibFuzzer requires clang."
    echo "Install: sudo apt-get install clang  (Ubuntu/Debian)"
    exit 1
fi

CLANG_VER=$(clang --version | head -1)
echo "Using: $CLANG_VER"

# ── Colors ───────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0

mkdir -p "$BUILD_DIR" "$CORPUS_DIR"

# ── Harness definitions ─────────────────────────────────────────────
# Each entry: name|wheel_dir|harness_src|wheel_src|extra_srcs|extra_incs
# Use relative paths from REPO_ROOT

declare -a HARNESSES=(
    "at_command|wheels/protocols/at_command|test/fuzz_libfuzzer_at_command.c|at_command.c||"
    "bencode|wheels/protocols/bencode|test/fuzz_libfuzzer_bencode.c|bencode.c||"
    "canbus|wheels/protocols/canbus|test/fuzz_libfuzzer_canbus.c|canbus.c||"
    "cbor_lite|wheels/protocols/cbor_lite|test/fuzz_libfuzzer_cbor_lite.c|cbor_lite.c||"
    "coap_packet|wheels/protocols/coap_packet|test/fuzz_libfuzzer_coap_packet.c|coap_packet.c||"
    "dhcp_packet|wheels/protocols/dhcp_packet|test/fuzz_libfuzzer_dhcp_packet.c|dhcp_packet.c||"
    "dns_packet|wheels/protocols/dns_packet|test/fuzz_libfuzzer_dns_packet.c|dns_packet.c||"
    "http_parser|wheels/protocols/http_parser|test/fuzz_libfuzzer_http_parser.c|http_parser.c||"
    "json_parser|wheels/primitives/json_parser|test/fuzz_libfuzzer_json_parser.c|json_parser.c||"
    "lorawan_packet|wheels/protocols/lorawan_packet|test/fuzz_libfuzzer_lorawan_packet.c|lorawan_packet.c||"
    "midi_packet|wheels/protocols/midi_packet|test/fuzz_libfuzzer_midi_packet.c|midi_packet.c||"
    "modbus_rtu|wheels/protocols/modbus_rtu|test/fuzz_libfuzzer_modbus_rtu.c|modbus_rtu.c|wheels/primitives/crc16/crc16.c|-Iwheels/primitives/crc16"
    "modbus_tcp|wheels/protocols/modbus_tcp|test/fuzz_libfuzzer_modbus_tcp.c|modbus_tcp.c||"
    "mqtt_packet|wheels/protocols/mqtt_packet|test/fuzz_libfuzzer_mqtt_packet.c|mqtt_packet.c||"
    "mqtt5_packet|wheels/protocols/mqtt5_packet|test/fuzz_libfuzzer_mqtt5_packet.c|mqtt5_packet.c||"
    "msgpack|wheels/protocols/msgpack|test/fuzz_libfuzzer_msgpack.c|msgpack.c||"
    "ncp5623_parser|wheels/protocols/ncp5623_parser|test/fuzz_libfuzzer_ncp5623_parser.c|ncp5623_parser.c||"
    "nmea_parser|wheels/protocols/nmea_parser|test/fuzz_libfuzzer_nmea_parser.c|nmea_parser.c||"
    "protobuf_lite|wheels/protocols/protobuf_lite|test/fuzz_libfuzzer_protobuf_lite.c|protobuf_lite.c||"
    "syslog_packet|wheels/protocols/syslog_packet|test/fuzz_libfuzzer_syslog_packet.c|syslog_packet.c||"
    "tftp|wheels/protocols/tftp|test/fuzz_libfuzzer_tftp.c|tftp.c||"
    "tlv_codec|wheels/protocols/tlv_codec|test/fuzz_libfuzzer_tlv_codec.c|tlv_codec.c||"
    "websocket_frame|wheels/protocols/websocket_frame|test/fuzz_libfuzzer_websocket_frame.c|websocket_frame.c||"
    "xmodem|wheels/protocols/xmodem|test/fuzz_libfuzzer_xmodem.c|xmodem.c||"
    "ymodem|wheels/protocols/ymodem|test/fuzz_libfuzzer_ymodem.c|ymodem.c||"
)

run_libfuzzer() {
    local entry="$1"
    IFS='|' read -r name wheel_dir harness_src wheel_src extra_srcs extra_incs <<< "$entry"

    local full_wheel_dir="$REPO_ROOT/$wheel_dir"
    local full_harness="$full_wheel_dir/$harness_src"
    local full_src="$full_wheel_dir/$wheel_src"
    local out="$BUILD_DIR/fuzz_$name"
    local corpus="$CORPUS_DIR/$name"

    if [ ! -f "$full_harness" ]; then
        echo -e "${YELLOW}SKIP${NC}: $name (no LibFuzzer harness)"
        ((SKIP++)) || true
        return
    fi
    if [ ! -f "$full_src" ]; then
        echo -e "${YELLOW}SKIP${NC}: $name (no source)"
        ((SKIP++)) || true
        return
    fi

    # Create corpus directory
    mkdir -p "$corpus"

    # ── Compile ──────────────────────────────────────────────────────
    local -a cmd=(clang -std=c99 -O1 -g
        -fsanitize=fuzzer,address,undefined
        -fno-sanitize-recover=all
        "-I$full_wheel_dir"
        "-I$REPO_ROOT/tooling/harness")

    # Extra include paths (e.g., crc16 for modbus_rtu)
    if [ -n "$extra_incs" ]; then
        for inc in $extra_incs; do
            cmd+=("-I$REPO_ROOT/${inc#-I}")
        done
    fi

    cmd+=("$full_harness" "$full_src")

    # Extra source files (e.g., crc16.c for modbus_rtu)
    if [ -n "$extra_srcs" ]; then
        for src in $extra_srcs; do
            cmd+=("$REPO_ROOT/$src")
        done
    fi

    cmd+=(-o "$out" -lm)

    if ! "${cmd[@]}" 2>"$BUILD_DIR/compile_${name}.log"; then
        echo -e "${RED}FAIL${NC}: $name (compile error)"
        cat "$BUILD_DIR/compile_${name}.log"
        ((FAIL++)) || true
        return
    fi

    # ── Run LibFuzzer ────────────────────────────────────────────────
    echo -ne "${CYAN}FUZZ${NC}: $name (${MAX_TIME}s)... "

    if "$out" "$corpus" \
        -max_total_time="$MAX_TIME" \
        -print_final_stats=1 \
        > "$BUILD_DIR/fuzz_${name}.log" 2>&1; then

        # Extract stats from LibFuzzer output
        local cov=$(grep "cov:" "$BUILD_DIR/fuzz_${name}.log" | tail -1 | sed 's/.*cov: \([0-9]*\).*/\1/' || echo "?")
        local execs=$(grep "stat::number_of_executed_units:" "$BUILD_DIR/fuzz_${name}.log" | sed 's/.*: //' || echo "?")
        local corpus_size=$(ls "$corpus" 2>/dev/null | wc -l)

        echo -e "${GREEN}PASS${NC} (cov: $cov, execs: $execs, corpus: $corpus_size)"
        ((PASS++)) || true
    else
        local exit_code=$?
        echo -e "${RED}FAIL${NC} (exit $exit_code — crash found!)"
        echo "  Log: $BUILD_DIR/fuzz_${name}.log"
        echo "  Crash inputs saved in: $corpus"
        # Show last few lines of the fuzzer output for context
        tail -20 "$BUILD_DIR/fuzz_${name}.log" | sed 's/^/  /'
        ((FAIL++)) || true
    fi
}

# ── Main ─────────────────────────────────────────────────────────────

echo "=== LibFuzzer Coverage-Guided Protocol Fuzzing ==="
echo "Build dir:  $BUILD_DIR"
echo "Corpus dir: $CORPUS_DIR"
echo "Time/proto: ${MAX_TIME}s"
echo ""

for entry in "${HARNESSES[@]}"; do
    IFS='|' read -r name _ <<< "$entry"

    # If single protocol requested, skip others
    if [ -n "$SINGLE" ] && [ "$name" != "$SINGLE" ]; then
        continue
    fi

    run_libfuzzer "$entry"
done

echo ""
echo "=== Summary ==="
echo -e "${GREEN}PASS${NC}: $PASS  ${RED}FAIL${NC}: $FAIL  ${YELLOW}SKIP${NC}: $SKIP"
echo "Corpus saved in: $CORPUS_DIR"
echo ""

if [ "$FAIL" -gt 0 ]; then
    echo "FAILED — crashes found! Examine crash-* files in corpus directories."
    exit 1
fi
echo "ALL LIBFUZZER RUNS PASSED — no crashes found"
