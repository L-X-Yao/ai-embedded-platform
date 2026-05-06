/*
 * prop_soft_i2c.c — property-based tests for soft_i2c
 *
 * Build: add to test/CMakeLists.txt or compile standalone:
 *   gcc -std=c99 -I.. -I../../../../tooling/test prop_soft_i2c.c ../soft_i2c.c -o prop_soft_i2c
 */

#include "test_harness.h"
#include "property_test.h"
#include "../soft_i2c.h"
#include <string.h>

/* ---------- mock GPIO with bit trace ---------- */

static uint8_t g_sda_bits[4096];
static int g_sda_bit_idx;
static int g_sda_bit_len;
static void m_scl_set(void *ctx, bool h) { (void)ctx; (void)h; }
static void m_sda_set(void *ctx, bool h) { (void)ctx; (void)h; }
static void m_sda_dir_out(void *ctx) { (void)ctx; }
static void m_sda_dir_in(void *ctx) { (void)ctx; }
static bool m_sda_get(void *ctx) {
    (void)ctx;
    if (g_sda_bit_idx < g_sda_bit_len)
        return g_sda_bits[g_sda_bit_idx++] != 0;
    return true;
}
static void m_delay(void *ctx) { (void)ctx; }

static const soft_i2c_gpio_t g_gpio = {
    .scl_set = m_scl_set, .sda_set = m_sda_set,
    .sda_dir_out = m_sda_dir_out, .sda_dir_in = m_sda_dir_in,
    .sda_get = m_sda_get, .delay = m_delay, .ctx = NULL,
};

static void mock_reset_prop(void) {
    g_sda_bit_idx = 0;
    g_sda_bit_len = 0;
    memset(g_sda_bits, 1, sizeof(g_sda_bits));
}

/* Property: init + write never crashes regardless of ACK/NACK pattern */
PROPERTY(write_never_crashes, 10000) {
    soft_i2c_t bus;
    mock_reset_prop();
    soft_i2c_init(&bus, &g_gpio);

    uint8_t data[32];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 32);
    ARBI_FILL(seed, data, len);
    uint8_t addr = ARBI_UINT8(seed) & 0x7F;

    /* Random ACK/NACK pattern */
    int total_bits = 1 + len; /* addr ACK + data ACKs */
    for (int i = 0; i < total_bits && i < (int)sizeof(g_sda_bits); i++)
        g_sda_bits[i] = ARBI_UINT8(seed) & 1;
    g_sda_bit_len = total_bits;

    soft_i2c_err_t err = soft_i2c_write(&bus, addr, data, len);
    PROP_ASSERT(err == SOFT_I2C_OK || err == SOFT_I2C_ERR_NACK);
}

/* Property: init + read never crashes regardless of response pattern */
PROPERTY(read_never_crashes, 10000) {
    soft_i2c_t bus;
    mock_reset_prop();
    soft_i2c_init(&bus, &g_gpio);

    uint8_t buf[32];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 1, 32);
    uint8_t addr = ARBI_UINT8(seed) & 0x7F;

    /* ACK addr + random data bits */
    g_sda_bits[0] = 0; /* ACK addr */
    for (int i = 1; i < 1 + len * 8 && i < (int)sizeof(g_sda_bits); i++)
        g_sda_bits[i] = ARBI_UINT8(seed) & 1;
    g_sda_bit_len = 1 + len * 8;

    soft_i2c_err_t err = soft_i2c_read(&bus, addr, buf, len);
    PROP_ASSERT(err == SOFT_I2C_OK || err == SOFT_I2C_ERR_NACK);
}

/* Property: write_reg always returns OK or NACK, nothing else */
PROPERTY(write_reg_bounded_errors, 10000) {
    soft_i2c_t bus;
    mock_reset_prop();
    soft_i2c_init(&bus, &g_gpio);

    uint8_t addr = ARBI_UINT8(seed) & 0x7F;
    uint8_t reg = ARBI_UINT8(seed);
    uint8_t val = ARBI_UINT8(seed);

    for (int i = 0; i < 3; i++)
        g_sda_bits[i] = ARBI_UINT8(seed) & 1;
    g_sda_bit_len = 3;

    soft_i2c_err_t err = soft_i2c_write_reg(&bus, addr, reg, val);
    PROP_ASSERT(err == SOFT_I2C_OK || err == SOFT_I2C_ERR_NACK);
}

/* Property: bus_recover always returns OK */
PROPERTY(bus_recover_always_ok, 5000) {
    soft_i2c_t bus;
    mock_reset_prop();
    soft_i2c_init(&bus, &g_gpio);

    for (int i = 0; i < 18; i++)
        g_sda_bits[i] = ARBI_UINT8(seed) & 1;
    g_sda_bit_len = 18;

    PROP_ASSERT_EQ(SOFT_I2C_OK, soft_i2c_bus_recover(&bus));
}

TEST_MAIN("soft_i2c properties")
    RUN(write_never_crashes);
    RUN(read_never_crashes);
    RUN(write_reg_bounded_errors);
    RUN(bus_recover_always_ok);
TEST_MAIN_END()
