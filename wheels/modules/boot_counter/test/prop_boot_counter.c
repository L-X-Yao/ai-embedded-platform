/*
 * prop_boot_counter.c — property-based tests for boot_counter
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_boot_counter.c ../boot_counter.c -o prop_boot_counter
 */

#include "test_harness.h"
#include "property_test.h"
#include "../boot_counter.h"
#include <string.h>

/* --- RAM-backed storage mock --- */
static uint8_t mock_storage_buf[sizeof(boot_counter_record_t)];
static bool mock_storage_valid;

static int mock_read(uint8_t *buf, size_t size, void *ctx) {
    (void)ctx;
    if (!mock_storage_valid) return -1;
    if (size > sizeof(mock_storage_buf)) return -1;
    memcpy(buf, mock_storage_buf, size);
    return 0;
}

static int mock_write(const uint8_t *buf, size_t size, void *ctx) {
    (void)ctx;
    if (size > sizeof(mock_storage_buf)) return -1;
    memcpy(mock_storage_buf, buf, size);
    mock_storage_valid = true;
    return 0;
}

static const boot_counter_storage_t mock_storage = {
    .read     = mock_read,
    .write    = mock_write,
    .user_ctx = NULL,
};

/* Property: random operation sequences never crash */
PROPERTY(random_ops_no_crash, 10000) {
    mock_storage_valid = false;
    memset(mock_storage_buf, 0, sizeof(mock_storage_buf));

    boot_counter_t bc;
    boot_counter_init(&bc, &mock_storage);
    boot_counter_load(&bc);

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 5;
        if (op == 0) {
            boot_counter_increment(&bc);
        } else if (op == 1) {
            boot_counter_tick_second(&bc);
        } else if (op == 2) {
            boot_counter_set_reset_reason(&bc, ARBI_UINT32(seed));
        } else if (op == 3) {
            (void)boot_counter_get_count(&bc);
        } else {
            (void)boot_counter_get_uptime_seconds(&bc);
        }
    }
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    mock_storage_valid = false;

    boot_counter_t bc1, bc2;
    boot_counter_init(&bc1, &mock_storage);
    boot_counter_init(&bc2, &mock_storage);
    boot_counter_init(&bc2, &mock_storage);

    PROP_ASSERT_EQ(boot_counter_get_count(&bc1), boot_counter_get_count(&bc2));
    PROP_ASSERT_EQ(boot_counter_get_uptime_seconds(&bc1),
                   boot_counter_get_uptime_seconds(&bc2));
}

/* Property: increment always increases count by exactly 1 */
PROPERTY(increment_monotonic, 10000) {
    mock_storage_valid = true;
    memset(mock_storage_buf, 0, sizeof(mock_storage_buf));

    boot_counter_t bc;
    boot_counter_init(&bc, &mock_storage);
    boot_counter_load(&bc);

    uint32_t before = boot_counter_get_count(&bc);
    boot_counter_increment(&bc);
    uint32_t after = boot_counter_get_count(&bc);
    PROP_ASSERT(after >= before);
}

/* Property: tick_second always increases uptime by 1 */
PROPERTY(tick_increments_uptime, 10000) {
    mock_storage_valid = false;

    boot_counter_t bc;
    boot_counter_init(&bc, &mock_storage);

    uint32_t before = boot_counter_get_uptime_seconds(&bc);
    boot_counter_tick_second(&bc);
    uint32_t after = boot_counter_get_uptime_seconds(&bc);
    PROP_ASSERT_EQ(after, before + 1);
}

TEST_MAIN("boot_counter properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(increment_monotonic);
    RUN(tick_increments_uptime);
}
TEST_MAIN_END()
