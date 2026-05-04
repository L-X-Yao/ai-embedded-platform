/*
 * prop_device_info.c — property-based tests for device_info
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_device_info.c ../device_info.c -o prop_device_info
 */

#include "test_harness.h"
#include "property_test.h"
#include "../device_info.h"
#include <string.h>

/* Property: random set/tick/format sequences never crash */
PROPERTY(random_ops_no_crash, 10000) {
    device_info_t info;
    device_info_init(&info);

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 7;
        if (op == 0) {
            device_info_set_firmware_version(&info, "1.2.3");
        } else if (op == 1) {
            device_info_set_hardware_version(&info, "rev-B");
        } else if (op == 2) {
            device_info_set_serial_number(&info, "SN12345");
        } else if (op == 3) {
            device_info_tick(&info);
        } else if (op == 4) {
            device_info_set_boot_count(&info, ARBI_UINT32(seed));
        } else if (op == 5) {
            device_info_set_reset_reason(&info, ARBI_UINT32(seed));
        } else {
            char buf[512];
            device_info_format(&info, buf, sizeof(buf));
        }
    }

    (void)device_info_get_uptime(&info);
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    device_info_t d1, d2;
    device_info_init(&d1);
    device_info_init(&d2);
    device_info_init(&d2);

    PROP_ASSERT_EQ(device_info_get_uptime(&d1), device_info_get_uptime(&d2));
}

/* Property: tick increments uptime by 1 each call */
PROPERTY(tick_increments_uptime, 10000) {
    device_info_t info;
    device_info_init(&info);

    uint32_t before = device_info_get_uptime(&info);
    uint8_t ticks = (uint8_t)ARBI_RANGE(seed, 1, 100);
    for (uint8_t i = 0; i < ticks; i++)
        device_info_tick(&info);

    uint32_t after = device_info_get_uptime(&info);
    PROP_ASSERT_EQ(after, before + ticks);
}

/* Property: format is deterministic */
PROPERTY(format_determinism, 5000) {
    device_info_t info;
    device_info_init(&info);
    device_info_set_firmware_version(&info, "1.0.0");
    device_info_set_device_name(&info, "test");

    char buf1[512], buf2[512];
    int len1 = device_info_format(&info, buf1, sizeof(buf1));
    int len2 = device_info_format(&info, buf2, sizeof(buf2));
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

TEST_MAIN("device_info properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(tick_increments_uptime);
    RUN(format_determinism);
}
TEST_MAIN_END()
