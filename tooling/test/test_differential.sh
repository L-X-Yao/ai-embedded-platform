#!/bin/bash
# test_differential.sh — compare wheel output against reference implementations
#
# Usage:
#   test_differential.sh              # run all differential tests
#   test_differential.sh base64       # run only base64 tests
#   test_differential.sh --rounds 50  # override random test count (default 20)
#   test_differential.sh --seed 42    # reproducible random data
#   test_differential.sh --verbose    # show each test vector
#
# For each testable wheel:
#   1. Generates random input data
#   2. Runs our wheel's diff driver (compile a small C driver)
#   3. Runs the reference implementation (system CLI / python)
#   4. Compares output
#   5. Reports match/mismatch
#
# Reference implementations:
#   base64    — `base64` CLI (coreutils)
#   crc16     — python3 crcmod (CRC16-MODBUS)
#   hex_codec — python3 binascii.hexlify
#
# Exit: 0 = all match, 1 = mismatch found, 2 = build/setup failure

set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_diff"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

ROUNDS=20
SEED=""
VERBOSE=false
declare -a FILTER=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --rounds)  shift; ROUNDS="$1"; shift ;;
        --seed)    shift; SEED="$1"; shift ;;
        --verbose) VERBOSE=true; shift ;;
        -h|--help)
            sed -n '2,/^$/{ s/^# //; s/^#//; p }' "$0"
            exit 0
            ;;
        *)         FILTER+=("$1"); shift ;;
    esac
done

# The lengths come from $RANDOM, so --seed has to pin it as well as the data.
if [[ -n "$SEED" ]]; then
    RANDOM=$SEED
fi

pass_count=0
fail_count=0
skip_count=0
build_fail_count=0

# ---- helpers ---------------------------------------------------------------

log_pass() { printf "${GREEN}  PASS${NC} %s\n" "$1"; (( ++pass_count )) || true; }
log_fail() { printf "${RED}  FAIL${NC} %s\n" "$1"; (( ++fail_count )) || true; }
log_skip() { printf "${YELLOW}  SKIP${NC} %s\n" "$1"; (( ++skip_count )) || true; }
log_build_fail() { printf "${RED}  BUILD_FAIL${NC} %s\n" "$1"; (( ++build_fail_count )) || true; }
log_info() { printf "${CYAN}  INFO${NC} %s\n" "$1"; }

should_run() {
    local name="$1"
    if [[ ${#FILTER[@]} -eq 0 ]]; then return 0; fi
    for f in "${FILTER[@]}"; do
        if [[ "$name" == "$f" ]]; then return 0; fi
    done
    return 1
}

# Generate random binary data of a given length to a file.
# Uses /dev/urandom, seeded through shuf if --seed given.
gen_random() {
    local outfile="$1" length="$2"
    if [[ -n "$SEED" ]]; then
        # Reproducible: use python with seed
        python3 -c "
import random, sys
random.seed(${SEED} + ${round_idx:-0})
sys.stdout.buffer.write(bytes(random.getrandbits(8) for _ in range(${length})))
" > "$outfile"
    else
        dd if=/dev/urandom bs=1 count="$length" of="$outfile" 2>/dev/null
    fi
}

# ---- build drivers ---------------------------------------------------------

mkdir -p "$BUILD_DIR"

build_driver() {
    local name="$1" src="$2" wheel_src="$3" wheel_inc="$4"
    local out="${BUILD_DIR}/${name}"

    if [[ -f "$out" && "$out" -nt "$src" && "$out" -nt "$wheel_src" ]]; then
        return 0  # already built and up-to-date
    fi

    gcc -O2 -I "$wheel_inc" -I "$SCRIPT_DIR" -I "${REPO}/tooling/harness" \
        "$src" $wheel_src \
        -o "$out" 2>&1
}

# ---- base64 ----------------------------------------------------------------

test_base64() {
    if ! should_run base64; then return; fi

    printf "${BOLD}base64${NC} (encode + decode, %d rounds):\n" "$ROUNDS"

    # Check reference tool
    if ! command -v base64 &>/dev/null; then
        log_skip "base64 CLI not found"
        return
    fi

    # Build driver
    if ! build_driver diff_base64 \
            "${SCRIPT_DIR}/diff_base64.c" \
            "${REPO}/wheels/primitives/base64/base64.c" \
            "${REPO}/wheels/primitives/base64"; then
        log_build_fail "diff_base64"
        return
    fi

    local driver="${BUILD_DIR}/diff_base64"
    local tmpdata="${BUILD_DIR}/b64_input.bin"
    local our_enc="${BUILD_DIR}/b64_ours.txt"
    local ref_enc="${BUILD_DIR}/b64_ref.txt"
    local our_dec="${BUILD_DIR}/b64_dec_ours.bin"
    local ref_dec="${BUILD_DIR}/b64_dec_ref.bin"

    # Fixed test vectors first
    local -a fixed_inputs=("" "f" "fo" "foo" "foob" "fooba" "foobar"
                           "Hello, World!" "test")
    local -a fixed_expect=("" "Zg==" "Zm8=" "Zm9v" "Zm9vYg==" "Zm9vYmE=" "Zm9vYmFy"
                           "SGVsbG8sIFdvcmxkIQ==" "dGVzdA==")

    for i in "${!fixed_inputs[@]}"; do
        local input="${fixed_inputs[$i]}"
        local expected="${fixed_expect[$i]}"

        local ours
        ours=$(printf '%s' "$input" | "$driver" 2>/dev/null)
        local ref
        ref=$(printf '%s' "$input" | base64 | tr -d '\n')

        if [[ "$ours" == "$ref" && "$ours" == "$expected" ]]; then
            $VERBOSE && log_info "fixed[${i}]: '${input}' -> '${ours}'"
            log_pass "base64 encode fixed[${i}]"
        else
            log_fail "base64 encode fixed[${i}]: input='${input}' ours='${ours}' ref='${ref}' expected='${expected}'"
        fi
    done

    # Random rounds -- encode
    for round_idx in $(seq 1 "$ROUNDS"); do
        # Random length 0..200
        local len=$(( (RANDOM + round_idx) % 201 ))
        gen_random "$tmpdata" "$len"

        local ours ref
        ours=$("$driver" < "$tmpdata" 2>/dev/null)
        ref=$(base64 < "$tmpdata" | tr -d '\n')

        if [[ "$ours" == "$ref" ]]; then
            $VERBOSE && log_info "random encode #${round_idx}: ${len}B -> match"
            log_pass "base64 encode random #${round_idx} (${len}B)"
        else
            log_fail "base64 encode random #${round_idx} (${len}B): ours='${ours}' ref='${ref}'"
        fi
    done

    # Random rounds -- roundtrip (encode then decode)
    for round_idx in $(seq 1 "$ROUNDS"); do
        local len=$(( (RANDOM + round_idx + 1000) % 201 ))
        gen_random "$tmpdata" "$len"

        # Encode with reference, decode with ours
        local encoded
        encoded=$(base64 < "$tmpdata" | tr -d '\n')
        printf '%s' "$encoded" | "$driver" -d > "$our_dec" 2>/dev/null

        if cmp -s "$tmpdata" "$our_dec"; then
            $VERBOSE && log_info "random roundtrip #${round_idx}: ${len}B -> match"
            log_pass "base64 roundtrip random #${round_idx} (${len}B)"
        else
            log_fail "base64 roundtrip random #${round_idx} (${len}B)"
        fi
    done
}

# ---- crc16 -----------------------------------------------------------------

test_crc16() {
    if ! should_run crc16; then return; fi

    printf "${BOLD}crc16${NC} (CRC16-MODBUS, %d rounds):\n" "$ROUNDS"

    # Check python3
    if ! command -v python3 &>/dev/null; then
        log_skip "python3 not found"
        return
    fi

    # Build driver
    if ! build_driver diff_crc16 \
            "${SCRIPT_DIR}/diff_crc16.c" \
            "${REPO}/wheels/primitives/crc16/crc16.c" \
            "${REPO}/wheels/primitives/crc16"; then
        log_build_fail "diff_crc16"
        return
    fi

    local driver="${BUILD_DIR}/diff_crc16"
    local tmpdata="${BUILD_DIR}/crc_input.bin"

    # Fixed: canonical "123456789" -> 0x4B37
    local ours
    ours=$(printf '123456789' | "$driver" 2>/dev/null)
    if [[ "$ours" == "4b37" ]]; then
        log_pass "crc16 canonical '123456789' -> 0x4B37"
    else
        log_fail "crc16 canonical: expected '4b37', got '${ours}'"
    fi

    # Fixed: empty input -> 0xFFFF (init value)
    ours=$(printf '' | "$driver" 2>/dev/null)
    if [[ "$ours" == "ffff" ]]; then
        log_pass "crc16 empty -> 0xFFFF"
    else
        log_fail "crc16 empty: expected 'ffff', got '${ours}'"
    fi

    # Python reference: bitwise CRC16-MODBUS (no crcmod dependency)
    local py_crc16_ref
    py_crc16_ref='
import sys
data = sys.stdin.buffer.read()
crc = 0xFFFF
for b in data:
    crc ^= b
    for _ in range(8):
        if crc & 1:
            crc = (crc >> 1) ^ 0xA001
        else:
            crc >>= 1
print(f"{crc:04x}", end="")
'

    # Random rounds
    for round_idx in $(seq 1 "$ROUNDS"); do
        local len=$(( (RANDOM + round_idx) % 256 ))
        gen_random "$tmpdata" "$len"

        local ours ref
        ours=$("$driver" < "$tmpdata" 2>/dev/null)
        ref=$(python3 -c "$py_crc16_ref" < "$tmpdata" 2>/dev/null)

        if [[ "$ours" == "$ref" ]]; then
            $VERBOSE && log_info "random #${round_idx}: ${len}B -> ${ours}"
            log_pass "crc16 random #${round_idx} (${len}B)"
        else
            log_fail "crc16 random #${round_idx} (${len}B): ours='${ours}' ref='${ref}'"
        fi
    done
}

# ---- hex_codec --------------------------------------------------------------

test_hex_codec() {
    if ! should_run hex_codec; then return; fi

    printf "${BOLD}hex_codec${NC} (encode + decode, %d rounds):\n" "$ROUNDS"

    # Check python3
    if ! command -v python3 &>/dev/null; then
        log_skip "python3 not found"
        return
    fi

    # Build driver
    if ! build_driver diff_hex_codec \
            "${SCRIPT_DIR}/diff_hex_codec.c" \
            "${REPO}/wheels/primitives/hex_codec/hex_codec.c" \
            "${REPO}/wheels/primitives/hex_codec"; then
        log_build_fail "diff_hex_codec"
        return
    fi

    local driver="${BUILD_DIR}/diff_hex_codec"
    local tmpdata="${BUILD_DIR}/hex_input.bin"

    # Fixed vectors
    local -a fixed_inputs=("" "Hello" "\x00\x01\x02\xff" "deadbeef")
    local -a fixed_expect=("" "48656c6c6f" "000102ff" "6465616462656566")

    for i in "${!fixed_inputs[@]}"; do
        local input="${fixed_inputs[$i]}"
        local expected="${fixed_expect[$i]}"

        local ours
        ours=$(printf '%b' "$input" | "$driver" 2>/dev/null)

        if [[ "$ours" == "$expected" ]]; then
            $VERBOSE && log_info "fixed[${i}]: -> '${ours}'"
            log_pass "hex_codec encode fixed[${i}]"
        else
            log_fail "hex_codec encode fixed[${i}]: expected='${expected}' got='${ours}'"
        fi
    done

    # Python reference for hex encode
    local py_hex_ref='
import sys, binascii
data = sys.stdin.buffer.read()
print(binascii.hexlify(data).decode(), end="")
'

    # Random rounds -- encode
    for round_idx in $(seq 1 "$ROUNDS"); do
        local len=$(( (RANDOM + round_idx) % 201 ))
        gen_random "$tmpdata" "$len"

        local ours ref
        ours=$("$driver" < "$tmpdata" 2>/dev/null)
        ref=$(python3 -c "$py_hex_ref" < "$tmpdata" 2>/dev/null)

        if [[ "$ours" == "$ref" ]]; then
            $VERBOSE && log_info "random encode #${round_idx}: ${len}B -> match"
            log_pass "hex_codec encode random #${round_idx} (${len}B)"
        else
            log_fail "hex_codec encode random #${round_idx} (${len}B): ours='${ours}' ref='${ref}'"
        fi
    done

    # Random rounds -- roundtrip (encode with python, decode with ours)
    for round_idx in $(seq 1 "$ROUNDS"); do
        local len=$(( (RANDOM + round_idx + 500) % 201 ))
        gen_random "$tmpdata" "$len"

        local encoded our_dec="${BUILD_DIR}/hex_dec_ours.bin"
        encoded=$(python3 -c "$py_hex_ref" < "$tmpdata" 2>/dev/null)
        printf '%s' "$encoded" | "$driver" -d > "$our_dec" 2>/dev/null

        if cmp -s "$tmpdata" "$our_dec"; then
            $VERBOSE && log_info "random roundtrip #${round_idx}: ${len}B -> match"
            log_pass "hex_codec roundtrip random #${round_idx} (${len}B)"
        else
            log_fail "hex_codec roundtrip random #${round_idx} (${len}B)"
        fi
    done
}

# ---- main ------------------------------------------------------------------

printf "${BOLD}=== Differential Testing ===${NC}\n"
printf "Rounds: %d" "$ROUNDS"
[[ -n "$SEED" ]] && printf "  Seed: %s" "$SEED"
printf "\n\n"

test_base64
test_crc16
test_hex_codec

# Cleanup temp files (keep binaries for re-runs)
rm -f "${BUILD_DIR}"/*.bin "${BUILD_DIR}"/*.txt

# ---- summary ---------------------------------------------------------------

printf "\n${BOLD}=== Summary ===${NC}\n"
printf "${GREEN}%d passed${NC}, ${RED}%d failed${NC}, ${RED}%d build failures${NC}, ${YELLOW}%d skipped${NC}\n" \
    "$pass_count" "$fail_count" "$build_fail_count" "$skip_count"

if [[ $fail_count -gt 0 ]]; then
    exit 1
elif [[ $build_fail_count -gt 0 ]]; then
    exit 2
elif [[ $pass_count -eq 0 ]]; then
    printf "${YELLOW}No tests ran.${NC}\n"
    exit 2
else
    exit 0
fi
