#include "test_harness.h"
#include "device_info.h"
#include <string.h>

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    device_info_t info;
    device_info_err_t err = device_info_init(&info);
    ASSERT_EQ(err, DEVICE_INFO_OK);
    ASSERT_EQ(info.uptime_seconds, 0);
    ASSERT_EQ(info.boot_count, 0);
    ASSERT_EQ(info.reset_reason, 0);
    ASSERT_EQ(info.firmware_version[0], '\0');
    ASSERT_EQ(info.device_name[0], '\0');
}

TEST(test_init_null)
{
    device_info_err_t err = device_info_init(NULL);
    ASSERT_EQ(err, DEVICE_INFO_ERR_PARAMS);
}

TEST(test_set_firmware_version)
{
    device_info_t info;
    device_info_init(&info);
    device_info_err_t err = device_info_set_firmware_version(&info, "1.2.3");
    ASSERT_EQ(err, DEVICE_INFO_OK);
    ASSERT_TRUE(strcmp(info.firmware_version, "1.2.3") == 0);
}

TEST(test_set_hardware_version)
{
    device_info_t info;
    device_info_init(&info);
    device_info_err_t err = device_info_set_hardware_version(&info, "rev-B");
    ASSERT_EQ(err, DEVICE_INFO_OK);
    ASSERT_TRUE(strcmp(info.hardware_version, "rev-B") == 0);
}

TEST(test_set_serial_number)
{
    device_info_t info;
    device_info_init(&info);
    device_info_err_t err = device_info_set_serial_number(&info, "SN-001234");
    ASSERT_EQ(err, DEVICE_INFO_OK);
    ASSERT_TRUE(strcmp(info.serial_number, "SN-001234") == 0);
}

TEST(test_set_build_date)
{
    device_info_t info;
    device_info_init(&info);
    device_info_err_t err = device_info_set_build_date(&info, "2026-05-01");
    ASSERT_EQ(err, DEVICE_INFO_OK);
    ASSERT_TRUE(strcmp(info.build_date, "2026-05-01") == 0);
}

TEST(test_set_device_name)
{
    device_info_t info;
    device_info_init(&info);
    device_info_err_t err = device_info_set_device_name(&info, "FlyBox-v2");
    ASSERT_EQ(err, DEVICE_INFO_OK);
    ASSERT_TRUE(strcmp(info.device_name, "FlyBox-v2") == 0);
}

TEST(test_set_all_fields)
{
    device_info_t info;
    device_info_init(&info);
    device_info_set_firmware_version(&info, "1.0.0");
    device_info_set_hardware_version(&info, "rev-A");
    device_info_set_serial_number(&info, "ABC123");
    device_info_set_build_date(&info, "2026-01-15");
    device_info_set_device_name(&info, "TestDev");
    device_info_set_boot_count(&info, 42);
    device_info_set_reset_reason(&info, 3);

    ASSERT_TRUE(strcmp(info.firmware_version, "1.0.0") == 0);
    ASSERT_TRUE(strcmp(info.hardware_version, "rev-A") == 0);
    ASSERT_TRUE(strcmp(info.serial_number, "ABC123") == 0);
    ASSERT_TRUE(strcmp(info.build_date, "2026-01-15") == 0);
    ASSERT_TRUE(strcmp(info.device_name, "TestDev") == 0);
    ASSERT_EQ(info.boot_count, 42);
    ASSERT_EQ(info.reset_reason, 3);
}

TEST(test_uptime_after_ticks)
{
    device_info_t info;
    device_info_init(&info);
    ASSERT_EQ(device_info_get_uptime(&info), 0);

    device_info_tick(&info);
    device_info_tick(&info);
    device_info_tick(&info);
    ASSERT_EQ(device_info_get_uptime(&info), 3);
}

TEST(test_multiple_ticks)
{
    device_info_t info;
    device_info_init(&info);

    for (int i = 0; i < 100; i++) {
        device_info_tick(&info);
    }
    ASSERT_EQ(device_info_get_uptime(&info), 100);
}

TEST(test_string_overflow)
{
    device_info_t info;
    device_info_init(&info);

    /* Create a string longer than DEVICE_INFO_STR_SIZE (32) */
    char long_str[64];
    memset(long_str, 'A', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';

    device_info_err_t err = device_info_set_firmware_version(&info, long_str);
    ASSERT_EQ(err, DEVICE_INFO_ERR_OVERFLOW);

    /* Should be truncated but NUL-terminated */
    ASSERT_EQ(strlen(info.firmware_version), DEVICE_INFO_STR_SIZE - 1);
    ASSERT_EQ(info.firmware_version[DEVICE_INFO_STR_SIZE - 1], '\0');
}

TEST(test_null_params_setters)
{
    device_info_t info;
    device_info_init(&info);

    ASSERT_EQ(device_info_set_firmware_version(NULL, "v"), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_firmware_version(&info, NULL), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_hardware_version(NULL, "v"), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_hardware_version(&info, NULL), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_serial_number(NULL, "s"), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_serial_number(&info, NULL), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_build_date(NULL, "d"), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_build_date(&info, NULL), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_device_name(NULL, "n"), DEVICE_INFO_ERR_PARAMS);
    ASSERT_EQ(device_info_set_device_name(&info, NULL), DEVICE_INFO_ERR_PARAMS);
}

TEST(test_boot_count)
{
    device_info_t info;
    device_info_init(&info);
    ASSERT_EQ(info.boot_count, 0);

    device_info_set_boot_count(&info, 99);
    ASSERT_EQ(info.boot_count, 99);

    device_info_set_boot_count(&info, 0);
    ASSERT_EQ(info.boot_count, 0);
}

TEST(test_reset_reason)
{
    device_info_t info;
    device_info_init(&info);
    ASSERT_EQ(info.reset_reason, 0);

    device_info_set_reset_reason(&info, 0x04);
    ASSERT_EQ(info.reset_reason, 0x04);
}

TEST(test_format_output)
{
    device_info_t info;
    device_info_init(&info);
    device_info_set_firmware_version(&info, "1.0.0");
    device_info_set_hardware_version(&info, "rev-A");
    device_info_set_serial_number(&info, "SN001");
    device_info_set_build_date(&info, "2026-05-01");
    device_info_set_device_name(&info, "TestDev");
    device_info_set_boot_count(&info, 5);
    device_info_set_reset_reason(&info, 2);
    device_info_tick(&info);
    device_info_tick(&info);
    device_info_tick(&info);

    char buf[512];
    int n = device_info_format(&info, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    ASSERT_TRUE(strstr(buf, "device_name=TestDev\n") != NULL);
    ASSERT_TRUE(strstr(buf, "firmware_version=1.0.0\n") != NULL);
    ASSERT_TRUE(strstr(buf, "hardware_version=rev-A\n") != NULL);
    ASSERT_TRUE(strstr(buf, "serial_number=SN001\n") != NULL);
    ASSERT_TRUE(strstr(buf, "build_date=2026-05-01\n") != NULL);
    ASSERT_TRUE(strstr(buf, "uptime_seconds=3\n") != NULL);
    ASSERT_TRUE(strstr(buf, "boot_count=5\n") != NULL);
    ASSERT_TRUE(strstr(buf, "reset_reason=2\n") != NULL);
}

TEST(test_format_buffer_too_small)
{
    device_info_t info;
    device_info_init(&info);
    device_info_set_device_name(&info, "Dev");
    device_info_set_firmware_version(&info, "1.0");

    char buf[16];  /* way too small for full output */
    int n = device_info_format(&info, buf, sizeof(buf));
    ASSERT_EQ(n, DEVICE_INFO_ERR_OVERFLOW);

    /* buf should still be NUL-terminated */
    ASSERT_TRUE(strlen(buf) < sizeof(buf));
}

TEST(test_format_null_params)
{
    device_info_t info;
    device_info_init(&info);
    char buf[256];

    ASSERT_EQ(device_info_format(NULL, buf, sizeof(buf)), -1);
    ASSERT_EQ(device_info_format(&info, NULL, sizeof(buf)), -1);
    ASSERT_EQ(device_info_format(&info, buf, 0), -1);
}

TEST(test_empty_fields_format)
{
    device_info_t info;
    device_info_init(&info);

    char buf[512];
    int n = device_info_format(&info, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    /* Empty strings should produce empty values after = */
    ASSERT_TRUE(strstr(buf, "device_name=\n") != NULL);
    ASSERT_TRUE(strstr(buf, "firmware_version=\n") != NULL);
    ASSERT_TRUE(strstr(buf, "uptime_seconds=0\n") != NULL);
    ASSERT_TRUE(strstr(buf, "boot_count=0\n") != NULL);
}

TEST(test_set_same_field_twice)
{
    device_info_t info;
    device_info_init(&info);

    device_info_set_firmware_version(&info, "1.0.0");
    ASSERT_TRUE(strcmp(info.firmware_version, "1.0.0") == 0);

    device_info_set_firmware_version(&info, "2.0.0");
    ASSERT_TRUE(strcmp(info.firmware_version, "2.0.0") == 0);
}

TEST(test_tick_null_safe)
{
    /* Should not crash */
    device_info_tick(NULL);
    ASSERT_TRUE(1);
}

TEST(test_get_uptime_null)
{
    ASSERT_EQ(device_info_get_uptime(NULL), 0);
}

TEST(test_set_boot_count_null_safe)
{
    /* Should not crash */
    device_info_set_boot_count(NULL, 10);
    ASSERT_TRUE(1);
}

TEST(test_set_reset_reason_null_safe)
{
    /* Should not crash */
    device_info_set_reset_reason(NULL, 5);
    ASSERT_TRUE(1);
}

TEST(test_exact_fit_string)
{
    device_info_t info;
    device_info_init(&info);

    /* Create a string that is exactly DEVICE_INFO_STR_SIZE-1 chars */
    char exact[DEVICE_INFO_STR_SIZE];
    memset(exact, 'B', DEVICE_INFO_STR_SIZE - 1);
    exact[DEVICE_INFO_STR_SIZE - 1] = '\0';

    device_info_err_t err = device_info_set_device_name(&info, exact);
    ASSERT_EQ(err, DEVICE_INFO_OK);
    ASSERT_EQ(strlen(info.device_name), DEVICE_INFO_STR_SIZE - 1);
}

TEST(test_format_returns_byte_count)
{
    device_info_t info;
    device_info_init(&info);
    device_info_set_device_name(&info, "X");

    char buf[512];
    int n = device_info_format(&info, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ((size_t)n, strlen(buf));
}

/* ================================================================ */

TEST_MAIN("device_info")
    RUN(test_init_valid);
    RUN(test_init_null);
    RUN(test_set_firmware_version);
    RUN(test_set_hardware_version);
    RUN(test_set_serial_number);
    RUN(test_set_build_date);
    RUN(test_set_device_name);
    RUN(test_set_all_fields);
    RUN(test_uptime_after_ticks);
    RUN(test_multiple_ticks);
    RUN(test_string_overflow);
    RUN(test_null_params_setters);
    RUN(test_boot_count);
    RUN(test_reset_reason);
    RUN(test_format_output);
    RUN(test_format_buffer_too_small);
    RUN(test_format_null_params);
    RUN(test_empty_fields_format);
    RUN(test_set_same_field_twice);
    RUN(test_tick_null_safe);
    RUN(test_get_uptime_null);
    RUN(test_set_boot_count_null_safe);
    RUN(test_set_reset_reason_null_safe);
    RUN(test_exact_fit_string);
    RUN(test_format_returns_byte_count);
TEST_MAIN_END()
