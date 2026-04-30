#!/usr/bin/env python3
"""Second pass: remove leftover ASSERT_TRUE macro defs from migrated files."""
import re
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


def cleanup_file(filepath):
    full = os.path.join(BASE, filepath)
    with open(full, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    # Remove #define ASSERT_TRUE(cond, msg) do { ... } while(0) with backslash continuations
    # This is a multi-line macro using backslash continuations
    content = re.sub(
        r'#define\s+ASSERT_TRUE\(cond,\s*msg\)\s+do\s*\{\s*\\\n(?:.*\\\n)*.*\}\s*while\s*\(0\)\s*\n',
        '', content
    )

    # Remove #define ASSERT_TRUE(cond) do { ... } while (0) (cli_parser single-arg variant)
    content = re.sub(
        r'#define\s+ASSERT_TRUE\(cond\)\s+do\s*\{\s*\\\n(?:.*\\\n)*.*\}\s*while\s*\(0\)\s*\n',
        '', content
    )

    # Remove leftover #define ASSERT_EQ and #define ASSERT_NEAR one-liners that reference ASSERT_TRUE
    content = re.sub(r'^#define\s+ASSERT_EQ\(a,\s*b,\s*msg\)\s+ASSERT_TRUE\(.*\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^#define\s+ASSERT_NEQ\(a,\s*b,\s*msg\)\s+ASSERT_TRUE\(.*\n', '', content, flags=re.MULTILINE)
    content = re.sub(r'^#define\s+ASSERT_NEAR\(a,\s*b,\s*tol,\s*msg\)\s+ASSERT_TRUE\(.*\n', '', content, flags=re.MULTILINE)

    # Also clean up the oled_ssd1306 special ASSERT_EQ that casts to int
    content = re.sub(r'^#define\s+ASSERT_EQ\(a,\s*b,\s*msg\)\s+ASSERT_TRUE\(.*\n', '', content, flags=re.MULTILINE)

    # Remove any remaining empty comment blocks like /* ---------- test helpers ---------- */ followed by blank lines
    # but only if they're now orphaned (nothing between them and the next real content)

    # Remove excessive blank lines
    content = re.sub(r'\n{3,}', '\n\n', content)

    if content != original:
        with open(full, 'w', encoding='utf-8', newline='\n') as f:
            f.write(content)
        return True
    return False


if __name__ == '__main__':
    changed = 0
    for filepath in FILES:
        try:
            if cleanup_file(filepath):
                print("FIXED: " + filepath)
                changed += 1
            else:
                print("OK:    " + filepath)
        except Exception as e:
            print("FAIL:  " + filepath + ": " + str(e))
            import traceback
            traceback.print_exc()

    print("\n" + str(changed) + " files needed cleanup out of " + str(len(FILES)))
