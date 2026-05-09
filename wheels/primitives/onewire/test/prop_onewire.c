/*
 * prop_onewire.c -- property-based tests for onewire
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness
 *        prop_onewire.c ../onewire.c -o prop_onewire
 */

#include "test_harness.h"
#include "property_test.h"
#include "../onewire.h"
#include <string.h>

/* ========== minimal mock GPIO for init/write/CRC ========== */

static void stub_pin_low(void *ctx)     { (void)ctx; }
static void stub_pin_release(void *ctx)  { (void)ctx; }
static bool stub_pin_read(void *ctx)     { (void)ctx; return true; }
static void stub_delay_us(void *ctx, uint16_t us) { (void)ctx; (void)us; }

static const onewire_gpio_t g_stub_gpio = {
    .pin_low     = stub_pin_low,
    .pin_release = stub_pin_release,
    .pin_read    = stub_pin_read,
    .delay_us    = stub_delay_us,
    .ctx         = NULL,
};

/* --- Property 1: CRC-8 self-check: crc(data || crc) == 0 --- */

PROPERTY(crc8_self_check, 10000) {
    uint8_t data[32];
    size_t len = ARBI_SIZE(seed, 30) + 1;
    ARBI_FILL(seed, data, len);

    uint8_t crc = onewire_crc8(data, (uint16_t)len);

    uint8_t full[33];
    memcpy(full, data, len);
    full[len] = crc;

    uint8_t check = onewire_crc8(full, (uint16_t)(len + 1));
    PROP_ASSERT_EQ(check, 0);
}

/* --- Property 2: CRC-8 is deterministic --- */

PROPERTY(crc8_deterministic, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 63) + 1;
    ARBI_FILL(seed, data, len);

    uint8_t crc1 = onewire_crc8(data, (uint16_t)len);
    uint8_t crc2 = onewire_crc8(data, (uint16_t)len);
    PROP_ASSERT_EQ(crc1, crc2);
}

/* --- Property 3: flipping any bit changes the CRC --- */

PROPERTY(crc8_bit_flip_changes_crc, 10000) {
    uint8_t data[16];
    size_t len = ARBI_SIZE(seed, 14) + 2;
    ARBI_FILL(seed, data, len);

    uint8_t crc_orig = onewire_crc8(data, (uint16_t)len);

    /* flip one random bit */
    uint8_t modified[16];
    memcpy(modified, data, len);
    size_t byte_idx = ARBI_SIZE(seed, len - 1);
    uint8_t bit_idx = ARBI_UINT8(seed) % 8;
    modified[byte_idx] ^= (uint8_t)(1u << bit_idx);

    uint8_t crc_mod = onewire_crc8(modified, (uint16_t)len);
    PROP_ASSERT(crc_orig != crc_mod);
}

/* --- Property 4: double init does not crash --- */

PROPERTY(double_init_safe, 5000) {
    (void)seed;
    onewire_t bus;
    onewire_err_t rc1 = onewire_init(&bus, &g_stub_gpio);
    PROP_ASSERT_EQ(rc1, ONEWIRE_OK);

    onewire_err_t rc2 = onewire_init(&bus, &g_stub_gpio);
    PROP_ASSERT_EQ(rc2, ONEWIRE_OK);
}

/* --- Property 5: write_byte never crashes with any byte value --- */

PROPERTY(write_byte_no_crash, 10000) {
    onewire_t bus;
    onewire_init(&bus, &g_stub_gpio);

    uint8_t byte = ARBI_UINT8(seed);
    onewire_err_t rc = onewire_write_byte(&bus, byte);
    PROP_ASSERT_EQ(rc, ONEWIRE_OK);
}

TEST_MAIN("onewire properties") {
    RUN(crc8_self_check);
    RUN(crc8_deterministic);
    RUN(crc8_bit_flip_changes_crc);
    RUN(double_init_safe);
    RUN(write_byte_no_crash);
}
TEST_MAIN_END()
