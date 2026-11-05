#!/usr/bin/env bash
# Run coverage-guided LibFuzzer harnesses for protocol parsers and primitives
#
# Requires: clang with libFuzzer support (clang >= 6.0)
# Usage:
#   bash tooling/test/run_libfuzzer.sh              # all protocols, 10s each
#   bash tooling/test/run_libfuzzer.sh --time 30     # 30s per protocol
#   bash tooling/test/run_libfuzzer.sh mqtt_packet   # single protocol
#   bash tooling/test/run_libfuzzer.sh --coverage    # line coverage of saved corpus
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="/tmp/libfuzzer_build"
CORPUS_DIR="/tmp/libfuzzer_corpus"
ARTIFACT_DIR="/tmp/libfuzzer_artifacts"
MAX_TIME=10
SINGLE=""
MODE="fuzz"

# ── Parse arguments ──────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --time) MAX_TIME="$2"; shift 2 ;;
        --coverage) MODE="coverage"; shift ;;
        *)      SINGLE="$1"; shift ;;
    esac
done

# ── Check for clang ──────────────────────────────────────────────────
if [ "$MODE" = "fuzz" ]; then
    if ! command -v clang >/dev/null 2>&1; then
        echo "ERROR: clang not found in PATH. LibFuzzer requires clang."
        echo "Install: sudo apt-get install clang  (Ubuntu/Debian)"
        exit 1
    fi

    CLANG_VER=$(clang --version | head -1)
    echo "Using: $CLANG_VER"
fi

# ── Colors ───────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0

mkdir -p "$BUILD_DIR" "$CORPUS_DIR" "$ARTIFACT_DIR"

# ── Harness definitions ─────────────────────────────────────────────
# Each entry: name|wheel_dir|harness_src|wheel_src|extra_srcs|extra_incs
# Use relative paths from REPO_ROOT

declare -a HARNESSES=(
    "at_command|wheels/protocols/at_command|test/fuzz_libfuzzer_at_command.c|at_command.c||"
    "base64|wheels/primitives/base64|test/fuzz_libfuzzer_base64.c|base64.c||"
    "bencode|wheels/protocols/bencode|test/fuzz_libfuzzer_bencode.c|bencode.c||"
    "bitstream|wheels/primitives/bitstream|test/fuzz_libfuzzer_bitstream.c|bitstream.c||"
    "canbus|wheels/protocols/canbus|test/fuzz_libfuzzer_canbus.c|canbus.c||"
    "cbor_lite|wheels/protocols/cbor_lite|test/fuzz_libfuzzer_cbor_lite.c|cbor_lite.c||"
    "cli_parser|wheels/primitives/cli_parser|test/fuzz_libfuzzer_cli_parser.c|cli_parser.c||"
    "coap_packet|wheels/protocols/coap_packet|test/fuzz_libfuzzer_coap_packet.c|coap_packet.c||"
    "cobs|wheels/primitives/cobs|test/fuzz_libfuzzer_cobs.c|cobs.c||"
    "crsf|wheels/protocols/crsf|test/fuzz_libfuzzer_crsf.c|crsf.c||"
    "dhcp_packet|wheels/protocols/dhcp_packet|test/fuzz_libfuzzer_dhcp_packet.c|dhcp_packet.c||"
    "dmx512|wheels/protocols/dmx512|test/fuzz_libfuzzer_dmx512.c|dmx512.c||"
    "dns_packet|wheels/protocols/dns_packet|test/fuzz_libfuzzer_dns_packet.c|dns_packet.c||"
    "hdlc|wheels/protocols/hdlc|test/fuzz_libfuzzer_hdlc.c|hdlc.c||"
    "heatshrink|wheels/primitives/heatshrink|test/fuzz_libfuzzer_heatshrink.c|heatshrink.c||"
    "hex_codec|wheels/primitives/hex_codec|test/fuzz_libfuzzer_hex_codec.c|hex_codec.c||"
    "http_parser|wheels/protocols/http_parser|test/fuzz_libfuzzer_http_parser.c|http_parser.c||"
    "ibus|wheels/protocols/ibus|test/fuzz_libfuzzer_ibus.c|ibus.c||"
    "intel_hex|wheels/protocols/intel_hex|test/fuzz_libfuzzer_intel_hex.c|intel_hex.c||"
    "ir_nec|wheels/protocols/ir_nec|test/fuzz_libfuzzer_ir_nec.c|ir_nec.c||"
    "isotp|wheels/protocols/isotp|test/fuzz_libfuzzer_isotp.c|isotp.c||"
    "json_parser|wheels/primitives/json_parser|test/fuzz_libfuzzer_json_parser.c|json_parser.c||"
    "json_rpc|wheels/protocols/json_rpc|test/fuzz_libfuzzer_json_rpc.c|json_rpc.c||"
    "lin_bus|wheels/protocols/lin_bus|test/fuzz_libfuzzer_lin_bus.c|lin_bus.c||"
    "lorawan_packet|wheels/protocols/lorawan_packet|test/fuzz_libfuzzer_lorawan_packet.c|lorawan_packet.c||"
    "mavlink|wheels/protocols/mavlink|test/fuzz_libfuzzer_mavlink.c|mavlink.c||"
    "midi_packet|wheels/protocols/midi_packet|test/fuzz_libfuzzer_midi_packet.c|midi_packet.c||"
    "modbus_ascii|wheels/protocols/modbus_ascii|test/fuzz_libfuzzer_modbus_ascii.c|modbus_ascii.c||"
    "modbus_rtu|wheels/protocols/modbus_rtu|test/fuzz_libfuzzer_modbus_rtu.c|modbus_rtu.c|wheels/primitives/crc16/crc16.c|-Iwheels/primitives/crc16"
    "modbus_tcp|wheels/protocols/modbus_tcp|test/fuzz_libfuzzer_modbus_tcp.c|modbus_tcp.c||"
    "mqtt5_packet|wheels/protocols/mqtt5_packet|test/fuzz_libfuzzer_mqtt5_packet.c|mqtt5_packet.c||"
    "mqtt_packet|wheels/protocols/mqtt_packet|test/fuzz_libfuzzer_mqtt_packet.c|mqtt_packet.c||"
    "msgpack|wheels/protocols/msgpack|test/fuzz_libfuzzer_msgpack.c|msgpack.c||"
    "ncp5623_parser|wheels/protocols/ncp5623_parser|test/fuzz_libfuzzer_ncp5623_parser.c|ncp5623_parser.c||"
    "nmea_parser|wheels/protocols/nmea_parser|test/fuzz_libfuzzer_nmea_parser.c|nmea_parser.c||"
    "ota_ab|wheels/protocols/ota_ab|test/fuzz_libfuzzer_ota_ab.c|ota_ab.c||"
    "ppp_frame|wheels/protocols/ppp_frame|test/fuzz_libfuzzer_ppp_frame.c|ppp_frame.c||"
    "protobuf_lite|wheels/protocols/protobuf_lite|test/fuzz_libfuzzer_protobuf_lite.c|protobuf_lite.c||"
    "rle_codec|wheels/primitives/rle_codec|test/fuzz_libfuzzer_rle_codec.c|rle_codec.c||"
    "s_record|wheels/protocols/s_record|test/fuzz_libfuzzer_s_record.c|s_record.c||"
    "sbus|wheels/protocols/sbus|test/fuzz_libfuzzer_sbus.c|sbus.c||"
    "slip|wheels/primitives/slip|test/fuzz_libfuzzer_slip.c|slip.c||"
    "sntp_client|wheels/protocols/sntp_client|test/fuzz_libfuzzer_sntp_client.c|sntp_client.c||"
    "syslog_packet|wheels/protocols/syslog_packet|test/fuzz_libfuzzer_syslog_packet.c|syslog_packet.c||"
    "tftp|wheels/protocols/tftp|test/fuzz_libfuzzer_tftp.c|tftp.c||"
    "tlv_codec|wheels/protocols/tlv_codec|test/fuzz_libfuzzer_tlv_codec.c|tlv_codec.c||"
    "ubx_parser|wheels/protocols/ubx_parser|test/fuzz_libfuzzer_ubx_parser.c|ubx_parser.c||"
    "uds|wheels/protocols/uds|test/fuzz_libfuzzer_uds.c|uds.c||"
    "utf8|wheels/primitives/utf8|test/fuzz_libfuzzer_utf8.c|utf8.c||"
    "varint|wheels/primitives/varint|test/fuzz_libfuzzer_varint.c|varint.c||"
    "websocket_frame|wheels/protocols/websocket_frame|test/fuzz_libfuzzer_websocket_frame.c|websocket_frame.c||"
    "xmodem|wheels/protocols/xmodem|test/fuzz_libfuzzer_xmodem.c|xmodem.c|wheels/protocols/crc_any/crc_any.c|-Iwheels/protocols/crc_any"
    "ymodem|wheels/protocols/ymodem|test/fuzz_libfuzzer_ymodem.c|ymodem.c|wheels/protocols/crc_any/crc_any.c|-Iwheels/protocols/crc_any"
)

# Fill TARGET_ARGS with the include paths and sources of one harness entry.
target_args() {
    local entry="$1"
    local name wheel_dir harness_src wheel_src extra_srcs extra_incs
    IFS='|' read -r name wheel_dir harness_src wheel_src extra_srcs extra_incs <<< "$entry"

    local full_wheel_dir="$REPO_ROOT/$wheel_dir"
    TARGET_ARGS=("-I$full_wheel_dir" "-I$REPO_ROOT/tooling/harness")

    # Extra include paths (e.g., crc16 for modbus_rtu)
    if [ -n "$extra_incs" ]; then
        for inc in $extra_incs; do
            TARGET_ARGS+=("-I$REPO_ROOT/${inc#-I}")
        done
    fi

    TARGET_ARGS+=("$full_wheel_dir/$harness_src" "$full_wheel_dir/$wheel_src")

    # Extra source files (e.g., crc16.c for modbus_rtu)
    if [ -n "$extra_srcs" ]; then
        for src in $extra_srcs; do
            TARGET_ARGS+=("$REPO_ROOT/$src")
        done
    fi
}

run_libfuzzer() {
    local entry="$1"
    IFS='|' read -r name wheel_dir harness_src wheel_src extra_srcs extra_incs <<< "$entry"

    local full_wheel_dir="$REPO_ROOT/$wheel_dir"
    local full_harness="$full_wheel_dir/$harness_src"
    local full_src="$full_wheel_dir/$wheel_src"
    local out="$BUILD_DIR/fuzz_$name"
    local corpus="$CORPUS_DIR/$name"
    local artifacts="$ARTIFACT_DIR/$name"

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

    # Create corpus and crash-artifact directories
    mkdir -p "$corpus" "$artifacts"

    # ── Compile ──────────────────────────────────────────────────────
    local -a cmd=(clang -std=c99 -O1 -g
        -DCONTRACT_ENABLED
        -fsanitize=fuzzer,address,undefined
        -fno-sanitize-recover=all)

    target_args "$entry"
    cmd+=("${TARGET_ARGS[@]}" -o "$out" -lm)

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
        -timeout=10 \
        -artifact_prefix="$artifacts/" \
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
        echo "  Crash inputs saved in: $artifacts"
        # Show last few lines of the fuzzer output for context
        tail -20 "$BUILD_DIR/fuzz_${name}.log" | sed 's/^/  /'
        ((FAIL++)) || true
    fi
}

# Replay the saved corpus through a gcc --coverage build and report line
# coverage of the component source. gcc rather than clang because this
# toolchain has no llvm-cov to read clang's profile data.
run_coverage() {
    local entry="$1"
    IFS='|' read -r name wheel_dir harness_src wheel_src extra_srcs extra_incs <<< "$entry"

    local dir="$BUILD_DIR/cov_$name"
    local corpus="$CORPUS_DIR/$name"
    local stem="${wheel_src##*/}"
    stem="${stem%.c}"

    rm -rf "$dir"
    mkdir -p "$dir" "$corpus"

    target_args "$entry"
    if ! gcc -std=c99 -O0 -g --coverage "${TARGET_ARGS[@]}" \
        "$REPO_ROOT/tooling/test/fuzz_replay_main.c" \
        -o "$dir/replay" -lm 2>"$dir/compile.log"; then
        echo -e "${RED}FAIL${NC}: $name (compile error)"
        cat "$dir/compile.log"
        ((FAIL++)) || true
        return
    fi

    "$dir/replay" "$corpus" > "$dir/replay.log" 2>&1
    local inputs
    inputs=$(ls "$corpus" | wc -l)

    # gcov prints a File/Lines pair per source; keep the component's own.
    local lines
    lines=$(cd "$dir" && gcov "replay-$stem.gcda" 2>/dev/null \
        | grep -A1 "^File .*/$stem\.c'" | grep '^Lines executed' \
        | sed 's/Lines executed://' || true)

    printf '%-18s %-20s corpus %s\n' "$name" "${lines:-no data}" "$inputs"
    ((PASS++)) || true
}

# ── Main ─────────────────────────────────────────────────────────────

echo "=== LibFuzzer Coverage-Guided Protocol Fuzzing ==="
echo "Build dir:  $BUILD_DIR"
echo "Corpus dir: $CORPUS_DIR"
echo "Crash dir:  $ARTIFACT_DIR"
echo "Time/proto: ${MAX_TIME}s"
echo ""

for entry in "${HARNESSES[@]}"; do
    IFS='|' read -r name _ <<< "$entry"

    # If single protocol requested, skip others
    if [ -n "$SINGLE" ] && [ "$name" != "$SINGLE" ]; then
        continue
    fi

    if [ "$MODE" = "coverage" ]; then
        run_coverage "$entry"
    else
        run_libfuzzer "$entry"
    fi
done

echo ""
echo "=== Summary ==="
echo -e "${GREEN}PASS${NC}: $PASS  ${RED}FAIL${NC}: $FAIL  ${YELLOW}SKIP${NC}: $SKIP"
echo "Corpus saved in: $CORPUS_DIR"
echo ""

if [ "$FAIL" -gt 0 ]; then
    echo "FAILED — crashes found! Examine crash-* files in $ARTIFACT_DIR."
    exit 1
fi
echo "ALL LIBFUZZER RUNS PASSED — no crashes found"
