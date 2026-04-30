#!/usr/bin/env python3
"""Migrate Family A test files to use test_harness.h"""
import re
import sys
import os

FILES = [
    "wheels/drivers/w25qxx/test/test_w25qxx.c",
    "wheels/drivers/qmi8658a/test/test_qmi8658a.c",
    "wheels/drivers/qmc5883p/test/test_qmc5883p.c",
    "wheels/drivers/xgzp6816d/test/test_xgzp6816d.c",
    "wheels/drivers/gpio_button/test/test_gpio_button.c",
    "wheels/drivers/gpio_led/test/test_gpio_led.c",
    "wheels/drivers/ws2812/test/test_ws2812.c",
    "wheels/drivers/bmp280/test/test_bmp280.c",
    "wheels/drivers/sht30/test/test_sht30.c",
    "wheels/drivers/mpu6050/test/test_mpu6050.c",
    "wheels/drivers/oled_ssd1306/test/test_oled_ssd1306.c",
    "wheels/primitives/fal/test/test_fal.c",
    "wheels/primitives/kvs/test/test_kvs.c",
    "wheels/primitives/cli_parser/test/test_cli_parser.c",
    "wheels/primitives/mahony_ahrs/test/test_mahony_ahrs.c",
    "wheels/primitives/mag_cal/test/test_mag_cal.c",
    "wheels/primitives/pid_controller/test/test_pid_controller.c",
    "wheels/primitives/moving_average/test/test_moving_average.c",
    "wheels/primitives/state_machine/test/test_state_machine.c",
    "wheels/primitives/soft_timer/test/test_soft_timer.c",
    "wheels/primitives/byte_buffer/test/test_byte_buffer.c",
    "wheels/primitives/event_queue/test/test_event_queue.c",
    "wheels/protocols/at_command/test/test_at_command.c",
    "wheels/protocols/modbus_rtu/test/test_modbus_rtu.c",
    "wheels/protocols/ncp5623_parser/test/test_ncp5623_parser.c",
    "wheels/protocols/nmea_parser/test/test_nmea_parser.c",
    "wheels/protocols/ota_ab/test/test_ota_ab.c",
    "wheels/protocols/mqtt_packet/test/test_mqtt_packet.c",
    "wheels/primitives/crc16/test/test_crc16.c",
]

BASE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def get_wheel_name(filepath):
    """Extract wheel name from path for TEST_MAIN"""
    parts = filepath.replace("\\", "/").split("/")
    for i, p in enumerate(parts):
        if p == "test":
            return parts[i-1]
    return "unknown"


def find_matching_paren(text, start):
    """Given text starting AFTER an open paren, find the matching close paren position."""
    depth = 1
    j = start
    while j < len(text) and depth > 0:
        if text[j] == '(':
            depth += 1
        elif text[j] == ')':
            depth -= 1
        elif text[j] == '"':
            j += 1
            while j < len(text) and text[j] != '"':
                if text[j] == '\\':
                    j += 1
                j += 1
        j += 1
    return j  # position AFTER the closing paren


def strip_last_string_from_call(text, macro_name, new_macro=None):
    """Convert MACRO(args..., "msg") -> NEW_MACRO(args...)"""
    if new_macro is None:
        new_macro = macro_name
    result = []
    i = 0
    search = macro_name + '('
    slen = len(search)
    while i < len(text):
        pos = text.find(search, i)
        if pos == -1:
            result.append(text[i:])
            break
        # Make sure this isn't part of a longer identifier
        if pos > 0 and (text[pos-1].isalnum() or text[pos-1] == '_'):
            result.append(text[i:pos + slen])
            i = pos + slen
            continue
        result.append(text[i:pos])
        paren_start = pos + slen
        j = find_matching_paren(text, paren_start)
        inner = text[paren_start:j-1]

        # Find last quoted string
        last_quote = inner.rfind('"')
        if last_quote != -1:
            open_quote = inner.rfind('"', 0, last_quote)
            if open_quote != -1 and open_quote < last_quote:
                comma_pos = inner.rfind(',', 0, open_quote)
                if comma_pos != -1:
                    args = inner[:comma_pos].rstrip()
                    result.append(new_macro + '(' + args + ')')
                else:
                    # only one arg which is the string itself - keep
                    result.append(new_macro + '(' + inner + ')')
            else:
                result.append(new_macro + '(' + inner + ')')
        else:
            result.append(new_macro + '(' + inner + ')')
        i = j
    return ''.join(result)


def convert_assert_neq(text):
    """ASSERT_NEQ(a, b, "msg") -> ASSERT_TRUE((a) != (b))"""
    result = []
    i = 0
    search = 'ASSERT_NEQ('
    slen = len(search)
    while i < len(text):
        pos = text.find(search, i)
        if pos == -1:
            result.append(text[i:])
            break
        if pos > 0 and (text[pos-1].isalnum() or text[pos-1] == '_'):
            result.append(text[i:pos + slen])
            i = pos + slen
            continue
        result.append(text[i:pos])
        paren_start = pos + slen
        j = find_matching_paren(text, paren_start)
        inner = text[paren_start:j-1]

        last_quote = inner.rfind('"')
        if last_quote != -1:
            open_quote = inner.rfind('"', 0, last_quote)
            comma_pos = inner.rfind(',', 0, open_quote)
            if comma_pos != -1:
                args = inner[:comma_pos].rstrip()
                # split args into a,b at first top-level comma
                d = 0
                first_comma = -1
                for k, c in enumerate(args):
                    if c == '(':
                        d += 1
                    elif c == ')':
                        d -= 1
                    elif c == ',' and d == 0:
                        first_comma = k
                        break
                if first_comma != -1:
                    a = args[:first_comma].strip()
                    b = args[first_comma+1:].strip()
                    result.append('ASSERT_TRUE((' + a + ') != (' + b + '))')
                else:
                    result.append('ASSERT_NEQ(' + inner + ')')
            else:
                result.append('ASSERT_NEQ(' + inner + ')')
        else:
            result.append('ASSERT_NEQ(' + inner + ')')
        i = j
    return ''.join(result)


def convert_assert_to_true(text):
    """ASSERT(cond, "msg") -> ASSERT_TRUE(cond) and ASSERT(cond) -> ASSERT_TRUE(cond)"""
    result = []
    i = 0
    search = 'ASSERT('
    slen = len(search)
    while i < len(text):
        pos = text.find(search, i)
        if pos == -1:
            result.append(text[i:])
            break
        # Check it is exactly ASSERT( and not ASSERT_xxx(
        if pos > 0 and (text[pos-1].isalnum() or text[pos-1] == '_'):
            result.append(text[i:pos + slen])
            i = pos + slen
            continue
        # Check the char after ASSERT is ( not _
        after_assert = text[pos+6:pos+7] if pos+6 < len(text) else ''
        if after_assert == '_':
            result.append(text[i:pos + slen])
            i = pos + slen
            continue

        result.append(text[i:pos])
        paren_start = pos + slen
        j = find_matching_paren(text, paren_start)
        inner = text[paren_start:j-1]

        last_quote = inner.rfind('"')
        if last_quote != -1:
            open_quote = inner.rfind('"', 0, last_quote)
            if open_quote != -1 and open_quote < last_quote:
                comma_pos = inner.rfind(',', 0, open_quote)
                if comma_pos != -1:
                    cond = inner[:comma_pos].rstrip()
                    result.append('ASSERT_TRUE(' + cond + ')')
                else:
                    result.append('ASSERT_TRUE(' + inner + ')')
            else:
                result.append('ASSERT_TRUE(' + inner + ')')
        else:
            # 1-arg ASSERT(cond)
            result.append('ASSERT_TRUE(' + inner + ')')
        i = j
    return ''.join(result)


def migrate_file(filepath):
    full = os.path.join(BASE, filepath)
    with open(full, 'r', encoding='utf-8') as f:
        content = f.read()

    wheel_name = get_wheel_name(filepath)

    # --- Step 1: Remove #include <stdio.h> ---
    content = re.sub(r'^#include\s+<stdio\.h>\s*\n', '', content, flags=re.MULTILINE)

    # --- Step 2: Add #include "test_harness.h" at top ---
    content = '#include "test_harness.h"\n' + content

    # --- Step 3: Change #include "../xxx.h" to #include "xxx.h" ---
    content = re.sub(r'#include\s+"\.\./([\w./]+)"', r'#include "\1"', content)

    # --- Step 4: Remove inline macro definitions ---
    # Remove counter variables
    content = re.sub(r'^\s*static\s+int\s+tests_run\s*=\s*0\s*,\s*tests_fail\s*=\s*0\s*;\s*\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*static\s+int\s+tests_run\s*,\s*tests_fail\s*;\s*\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*static\s+int\s+g_tests\s*=\s*0\s*;\s*\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*static\s+int\s+g_failures\s*=\s*0\s*;\s*\n', '', content, flags=re.MULTILINE)

    # Remove ASSERT(cond, msg) multi-line macro
    content = re.sub(
        r'#define\s+ASSERT\(cond,\s*msg\)\s+do\s*\{[^}]*\}\s*while\s*\(0\)\s*\n',
        '', content, flags=re.DOTALL
    )
    # Remove ASSERT(cond) single-arg macro (cli_parser)
    content = re.sub(
        r'#define\s+ASSERT\(cond\)\s+do\s*\{[^}]*\}\s*while\s*\(0\)\s*\n',
        '', content, flags=re.DOTALL
    )
    # Remove crc16 TEST(name, cond) macro
    content = re.sub(
        r'#define\s+TEST\(name,\s*cond\)\s+do\s*\{[^}]*\}\s*while\s*\(0\)\s*\n',
        '', content, flags=re.DOTALL
    )

    # Remove one-liner macro definitions
    content = re.sub(r'^#define\s+ASSERT_EQ\([^)]*\)\s+.*\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^#define\s+ASSERT_NEQ\([^)]*\)\s+.*\n', '', content, flags=re.MULTILINE)
    # ASSERT_NEAR can be one-liner or wrapped
    content = re.sub(r'^#define\s+ASSERT_NEAR\([^)]*\)\s+.*\n', '', content, flags=re.MULTILINE)
    # Multi-line ASSERT_NEAR with backslash continuation
    content = re.sub(r'#define\s+ASSERT_NEAR\([^)]*\)\s*\\\n\s*ASSERT\([^)]*\)\s*\n', '', content)

    # --- Step 5: Convert assert calls ---
    content = strip_last_string_from_call(content, 'ASSERT_EQ')
    content = convert_assert_neq(content)
    content = strip_last_string_from_call(content, 'ASSERT_NEAR')
    content = convert_assert_to_true(content)

    # --- Step 6: Convert test functions ---
    # static void test_xxx(void) -> TEST(test_xxx)
    content = re.sub(
        r'^static\s+void\s+(test_\w+)\s*\(void\)\s*$',
        r'TEST(\1)',
        content,
        flags=re.MULTILINE
    )

    # For crc16: static int test_xxx(void) -> TEST(test_xxx)
    content = re.sub(
        r'^static\s+int\s+(test_\w+)\s*\(void\)\s*$',
        r'TEST(\1)',
        content,
        flags=re.MULTILINE
    )

    # --- crc16-specific: TEST("name", cond) -> ASSERT_TRUE(cond) ---
    # After the main ASSERT conversion, the crc16 file still has TEST("name", cond) calls
    # because the macro was removed but the calls remain
    # These look like: TEST("empty buffer returns 0xFFFF", crc16_modbus(NULL, 0) == 0xFFFFu);
    content = re.sub(
        r'    TEST\("([^"]*)",\s*',
        '    ASSERT_TRUE(',
        content
    )
    # Remove leftover "return 0;" from crc16 test functions (they returned int before)
    # Only remove standalone ones at function level (4-space indent)
    content = re.sub(r'^    return 0;\n', '', content, flags=re.MULTILINE)

    # --- Step 7: Convert main() ---
    main_match = re.search(r'^(static\s+)?int\s+main\s*\(void\)\s*\n?\{?\s*$', content, re.MULTILINE)
    if not main_match:
        main_match = re.search(r'^int\s+main\s*\(void\)\s*$', content, re.MULTILINE)
    if main_match:
        main_start = main_match.start()
        brace_start = content.find('{', main_start)
        if brace_start != -1:
            j = find_matching_paren_brace(content, brace_start + 1)
            main_body = content[brace_start+1:j-1]

            test_calls = re.findall(r'\b(test_\w+)\s*\(\s*\)\s*;', main_body)

            run_lines = ''.join('    RUN(' + name + ');\n' for name in test_calls)
            new_main = 'TEST_MAIN("' + wheel_name + '")\n' + run_lines + 'TEST_MAIN_END()\n'

            content = content[:main_start] + new_main + content[j:]

    # --- Cleanup ---
    # Remove printf("test_xxx\n"); or printf("%s\n", __func__);
    content = re.sub(r'^\s*printf\("%s\\n",\s*__func__\);\s*\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^\s*printf\("test_\w+\\n"\);\s*\n', '', content, flags=re.MULTILINE)
    # Remove printf("=== crc16 wheel test suite ===\n"); style
    content = re.sub(r'^\s*printf\("===.*===\\n"\);\s*\n', '', content, flags=re.MULTILINE)

    # Remove tests_run = tests_fail = 0;
    content = re.sub(r'^\s*tests_run\s*=\s*tests_fail\s*=\s*0\s*;\s*\n', '', content, flags=re.MULTILINE)

    # Remove excessive blank lines
    content = re.sub(r'\n{3,}', '\n\n', content)

    with open(full, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)

    return filepath


def find_matching_paren_brace(text, start):
    """Find matching } for an open {, starting after the open brace."""
    depth = 1
    j = start
    while j < len(text) and depth > 0:
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
        elif text[j] == '"':
            j += 1
            while j < len(text) and text[j] != '"':
                if text[j] == '\\':
                    j += 1
                j += 1
        elif text[j] == "'":
            j += 1
            while j < len(text) and text[j] != "'":
                if text[j] == '\\':
                    j += 1
                j += 1
        j += 1
    return j


if __name__ == '__main__':
    for filepath in FILES:
        try:
            migrate_file(filepath)
            print("OK: " + filepath)
        except Exception as e:
            print("FAIL: " + filepath + ": " + str(e))
            import traceback
            traceback.print_exc()

    print("\nDone: " + str(len(FILES)) + " files processed")
