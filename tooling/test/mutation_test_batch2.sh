#!/usr/bin/env bash
# mutation_test_batch2.sh — mutation testing for 50 wheels (batch 2)
# Run from WSL: bash tooling/test/mutation_test_batch2.sh
set -euo pipefail

REPO="/mnt/d/temp/project/ai-embedded-platform"
HARNESS="$REPO/tooling/test"
TMPDIR="/tmp/mutation_batch2"
mkdir -p "$TMPDIR"

# Results file
RESULTS="$TMPDIR/results.csv"
echo "wheel,layer,total_mutations,killed,survived,skipped_compile,kill_rate" > "$RESULTS"

# Define all 50 wheels: layer directory name
declare -a WHEELS=(
  # L0 primitives (17)
  "primitives:checksum"
  "primitives:crc8"
  "primitives:crc32"
  "primitives:endian"
  "primitives:hash_fnv"
  "primitives:mahony_ahrs"
  "primitives:mag_cal"
  "primitives:moving_window"
  "primitives:cli_parser"
  "primitives:log_dedup"
  "primitives:log_ringbuf"
  "primitives:log_file"
  "primitives:log_filter"
  "primitives:log_rate"
  "primitives:log_color"
  "primitives:log_rtt"
  "primitives:log_uart"
  # L1 drivers (18)
  "drivers:bme280"
  "drivers:bme680"
  "drivers:sht30"
  "drivers:lis3dh"
  "drivers:ads1115"
  "drivers:tmp102"
  "drivers:ina219"
  "drivers:w25qxx"
  "drivers:ds3231"
  "drivers:servo"
  "drivers:stepper"
  "drivers:gpio_button"
  "drivers:rotary_encoder"
  "drivers:ws2812"
  "drivers:pca9685"
  "drivers:drv8833"
  "drivers:pcf8574"
  "drivers:mcp23017"
  # L2 protocols (10)
  "protocols:modbus_rtu"
  "protocols:mqtt5_packet"
  "protocols:lorawan_packet"
  "protocols:nmea_parser"
  "protocols:websocket_frame"
  "protocols:at_command"
  "protocols:bencode"
  "protocols:protobuf_lite"
  "protocols:syslog_packet"
  "protocols:dns_packet"
  # L3 modules (5)
  "modules:cli_shell"
  "modules:data_logger"
  "modules:sensor_hub"
  "modules:error_handler"
  "modules:config_store"
)

# Mutation operators: each is a perl substitution pattern
# We apply each to the source .c file, one at a time per match site
declare -a MUT_NAMES=(
  "LT_to_LE"
  "LE_to_LT"
  "GT_to_GE"
  "GE_to_GT"
  "EQ_to_NE"
  "NE_to_EQ"
  "PLUS_to_MINUS"
  "MINUS_to_PLUS"
  "RET0_to_RET1"
  "RETn1_to_RET0"
  "AND_to_OR"
  "OR_to_AND"
  "PREINC_to_PREDEC"
  "PREDEC_to_PREINC"
  "POSTINC_to_POSTDEC"
  "POSTDEC_to_POSTINC"
  "LSHIFT_to_RSHIFT"
  "RSHIFT_to_LSHIFT"
  "BITOR_to_BITAND"
  "BITAND_to_BITOR"
)

# Generate mutations for a source file and test them
# Arguments: $1=layer, $2=name
test_wheel() {
  local layer="$1"
  local name="$2"
  local wheel_dir="$REPO/wheels/$layer/$name"
  local src="$wheel_dir/${name}.c"
  local test_src="$wheel_dir/test/test_${name}.c"
  local hdr_dir="$wheel_dir"
  local work="$TMPDIR/$name"
  mkdir -p "$work"

  # Determine extra include paths and source files
  local extra_srcs=""
  local extra_includes="-I$hdr_dir -I$HARNESS"

  # modbus_rtu depends on crc16
  if [ "$name" = "modbus_rtu" ]; then
    extra_srcs="$REPO/wheels/primitives/crc16/crc16.c"
    extra_includes="$extra_includes -I$REPO/wheels/primitives/crc16"
  fi

  # Try baseline compile
  local base_bin="$work/base"
  if ! gcc -std=c99 -w -O0 $extra_includes "$test_src" "$src" $extra_srcs -o "$base_bin" -lm 2>"$work/compile_err.txt"; then
    # Try with gnu99 (some use typeof etc)
    if ! gcc -std=gnu99 -w -O0 $extra_includes "$test_src" "$src" $extra_srcs -o "$base_bin" -lm 2>"$work/compile_err.txt"; then
      echo "$name,$layer,0,0,0,0,0.0" >> "$RESULTS"
      echo "  SKIP $layer/$name — baseline compile failed"
      return
    fi
  fi

  # Verify baseline passes
  if ! timeout 10 "$base_bin" >/dev/null 2>&1; then
    echo "$name,$layer,0,0,0,0,0.0" >> "$RESULTS"
    echo "  SKIP $layer/$name — baseline test failed"
    return
  fi

  # Determine which std worked
  local std_flag="-std=c99"
  if ! gcc -std=c99 -w -O0 $extra_includes "$test_src" "$src" $extra_srcs -o /dev/null -lm 2>/dev/null; then
    std_flag="-std=gnu99"
  fi

  local total=0
  local killed=0
  local survived=0
  local skip_compile=0

  # Read source file
  local orig_src
  orig_src=$(cat "$src")
  local line_count
  line_count=$(echo "$orig_src" | wc -l)

  # For each line in the source file, try mutations
  for (( lineno=1; lineno<=line_count; lineno++ )); do
    local line
    line=$(echo "$orig_src" | sed -n "${lineno}p")

    # Skip comments, includes, blank lines
    if echo "$line" | grep -qE '^\s*(//|/\*|\*|#include|#define|#ifndef|#endif|#ifdef|#else|#pragma|$)'; then
      continue
    fi

    # Try each mutation operator on this line
    local mutated_line=""
    local mut_applied=0

    # 1. < to <= (but not <<, <=, ->)
    if echo "$line" | grep -qP '(?<!<)(?<!-)< (?!=)' 2>/dev/null || echo "$line" | grep -qE '[^<-]<[^<=]' 2>/dev/null; then
      mutated_line=$(echo "$line" | perl -pe 's/(?<!<)(?<!-)(?<![<])< (?!=)/<=/' 2>/dev/null || echo "$line" | sed 's/< /<= /' 2>/dev/null)
      if [ "$mutated_line" != "$line" ]; then
        mut_applied=1
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 2. <= to <
    if echo "$line" | grep -qF '<='; then
      mutated_line=$(echo "$line" | sed 's/<=/< /')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 3. > to >= (but not >>, >=, ->)
    if echo "$line" | grep -qP '(?<!>)(?<!-)> (?!=)' 2>/dev/null || echo "$line" | grep -qE '[^>-]>[^>=]' 2>/dev/null; then
      mutated_line=$(echo "$line" | perl -pe 's/(?<!>)(?<!-)> (?!=)/>= /' 2>/dev/null || echo "$line" | sed 's/> />= /')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 4. >= to >
    if echo "$line" | grep -qF '>='; then
      mutated_line=$(echo "$line" | sed 's/>=/> /')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 5. == to !=
    if echo "$line" | grep -qF '=='; then
      mutated_line=$(echo "$line" | sed 's/==/!=/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 6. != to ==
    if echo "$line" | grep -qF '!='; then
      mutated_line=$(echo "$line" | sed 's/!=/==/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 7. + to - (avoid ++ and +=, pointers)
    if echo "$line" | grep -qP '(?<!\+)\+ (?!\+|=)' 2>/dev/null; then
      mutated_line=$(echo "$line" | perl -pe 's/(?<!\+)\+ (?!\+|=)/- /')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 8. - to + (avoid --, -=, ->)
    if echo "$line" | grep -qP '(?<!-)- (?!-|=|>)' 2>/dev/null; then
      mutated_line=$(echo "$line" | perl -pe 's/(?<!-)- (?!-|=|>)/+ /')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 9. return 0 to return 1
    if echo "$line" | grep -qE 'return\s+0\s*;'; then
      mutated_line=$(echo "$line" | sed 's/return 0/return 1/' | sed 's/return  *0/return 1/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 10. return -1 to return 0
    if echo "$line" | grep -qE 'return\s+-?1\s*;'; then
      mutated_line=$(echo "$line" | sed 's/return -1/return 0/' | sed 's/return 1/return 0/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 11. && to ||
    if echo "$line" | grep -qF '&&'; then
      mutated_line=$(echo "$line" | sed 's/&&/||/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 12. || to &&
    if echo "$line" | grep -qF '||'; then
      mutated_line=$(echo "$line" | sed 's/||/\&\&/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 13. ++ to --
    if echo "$line" | grep -qF '++'; then
      mutated_line=$(echo "$line" | sed 's/++/--/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 14. -- to ++
    if echo "$line" | grep -qP '(?<!--)--(?!>)' 2>/dev/null; then
      mutated_line=$(echo "$line" | perl -pe 's/(?<!--)--(?!>)/++/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 15. << to >>
    if echo "$line" | grep -qF '<<'; then
      mutated_line=$(echo "$line" | sed 's/<</>>/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 16. >> to <<
    if echo "$line" | grep -qF '>>'; then
      mutated_line=$(echo "$line" | sed 's/>>/<</')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 17. | to & (bitwise, not || or |=)
    if echo "$line" | grep -qP '(?<!\|)\|(?!\||=)' 2>/dev/null; then
      mutated_line=$(echo "$line" | perl -pe 's/(?<!\|)\|(?!\||=)/\&/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

    # 18. & to | (bitwise, not && or &=)
    if echo "$line" | grep -qP '(?<!\&)\&(?!\&|=)' 2>/dev/null; then
      mutated_line=$(echo "$line" | perl -pe 's/(?<!\&)\&(?!\&|=)/|/')
      if [ "$mutated_line" != "$line" ]; then
        echo "$orig_src" | sed "${lineno}s|.*|${mutated_line}|" > "$work/mut.c"
        total=$((total + 1))
        if gcc $std_flag -w -O0 $extra_includes "$test_src" "$work/mut.c" $extra_srcs -o "$work/mut_bin" -lm 2>/dev/null; then
          if timeout 10 "$work/mut_bin" >/dev/null 2>&1; then
            survived=$((survived + 1))
          else
            killed=$((killed + 1))
          fi
        else
          skip_compile=$((skip_compile + 1))
          total=$((total - 1))
        fi
      fi
    fi

  done

  # Calculate kill rate
  local rate="0.0"
  if [ "$total" -gt 0 ]; then
    rate=$(awk "BEGIN { printf \"%.1f\", ($killed / $total) * 100 }")
  fi

  echo "$name,$layer,$total,$killed,$survived,$skip_compile,$rate" >> "$RESULTS"
  echo "  DONE $layer/$name: $total mutations, $killed killed, $survived survived ($rate%)"
}

echo "========================================"
echo " Mutation Testing — Batch 2 (50 wheels)"
echo "========================================"
echo ""

count=0
for entry in "${WHEELS[@]}"; do
  layer="${entry%%:*}"
  name="${entry##*:}"
  count=$((count + 1))
  echo "[$count/50] Testing $layer/$name..."
  test_wheel "$layer" "$name"
done

echo ""
echo "========================================"
echo " RESULTS SUMMARY"
echo "========================================"
echo ""
column -t -s',' "$RESULTS"
echo ""
echo "Results saved to: $RESULTS"
