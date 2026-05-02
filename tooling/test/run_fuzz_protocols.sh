#!/usr/bin/env bash
# Run all L2 protocol fuzz harnesses
# Usage: bash tooling/test/run_fuzz_protocols.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROTO_DIR="$REPO_ROOT/wheels/protocols"
BUILD_DIR="/tmp/fuzz_protocols_build"
PASS=0
FAIL=0
SKIP=0
TIMEOUT_SEC=30

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

RED='\033[0;31m'
GRN='\033[0;32m'
YEL='\033[0;33m'
NC='\033[0m'

declare -A EXTRA_SRCS
declare -A EXTRA_INCS

# modbus_rtu depends on crc16
EXTRA_SRCS[modbus_rtu]="$PROTO_DIR/../primitives/crc16/crc16.c"
EXTRA_INCS[modbus_rtu]="-I$PROTO_DIR/../primitives/crc16"

run_fuzz() {
    local name="$1"
    local dir="$PROTO_DIR/$name"
    local fuzz_src="$dir/test/fuzz_${name}.c"
    local wheel_src="$dir/${name}.c"
    local out="$BUILD_DIR/fuzz_${name}"

    if [ ! -f "$fuzz_src" ]; then
        echo -e "${YEL}SKIP${NC}: $name (no fuzz harness)"
        ((SKIP++)) || true
        return
    fi
    if [ ! -f "$wheel_src" ]; then
        echo -e "${YEL}SKIP${NC}: $name (no source file)"
        ((SKIP++)) || true
        return
    fi

    local extra_src="${EXTRA_SRCS[$name]:-}"
    local extra_inc="${EXTRA_INCS[$name]:-}"

    # Compile with sanitizers
    local cmd="gcc -std=c99 -O1 -g -Wall -Wextra"
    cmd+=" -fsanitize=address,undefined -fno-sanitize-recover=all"
    cmd+=" -I$dir $extra_inc"
    cmd+=" $fuzz_src $wheel_src $extra_src"
    cmd+=" -o $out -lm"

    if ! eval $cmd 2>"$BUILD_DIR/compile_${name}.log"; then
        echo -e "${RED}FAIL${NC}: $name (compile error)"
        cat "$BUILD_DIR/compile_${name}.log"
        ((FAIL++)) || true
        return
    fi

    # Run with timeout
    if timeout "$TIMEOUT_SEC" "$out" > "$BUILD_DIR/run_${name}.log" 2>&1; then
        echo -e "${GRN}PASS${NC}: $name — $(tail -1 "$BUILD_DIR/run_${name}.log")"
        ((PASS++)) || true
    else
        local exit_code=$?
        if [ "$exit_code" -eq 124 ]; then
            echo -e "${RED}FAIL${NC}: $name (TIMEOUT after ${TIMEOUT_SEC}s — possible infinite loop)"
        else
            echo -e "${RED}FAIL${NC}: $name (exit code $exit_code — crash or sanitizer error)"
        fi
        cat "$BUILD_DIR/run_${name}.log"
        ((FAIL++)) || true
    fi
}

echo "=== Protocol Wheel Fuzz Testing ==="
echo ""

WHEELS=(
    at_command
    bencode
    canbus
    cbor_lite
    coap_packet
    dns_packet
    http_parser
    lorawan_packet
    midi_packet
    modbus_rtu
    modbus_tcp
    mqtt_packet
    mqtt5_packet
    msgpack
    ncp5623_parser
    nmea_parser
    protobuf_lite
    syslog_packet
    tlv_codec
    websocket_frame
    xmodem
    ymodem
)

for w in "${WHEELS[@]}"; do
    run_fuzz "$w"
done

echo ""
echo "=== Summary ==="
echo -e "${GRN}PASS${NC}: $PASS  ${RED}FAIL${NC}: $FAIL  ${YEL}SKIP${NC}: $SKIP"
echo ""

if [ "$FAIL" -gt 0 ]; then
    echo "FAILED — see output above for details"
    exit 1
fi
echo "ALL FUZZ TESTS PASSED"
