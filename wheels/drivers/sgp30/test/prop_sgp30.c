/*
 * prop_sgp30.c -- property-based tests for sgp30
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_sgp30.c ../sgp30.c -o prop_sgp30
 */

#include "test_harness.h"
#include "property_test.h"
#include <string.h>
#include "../sgp30.h"

/* --- mock I2C (raw write/read, not register-based) --- */

static int     g_fail_next;
static int     g_op_count;
static int     g_fail_at;
static uint8_t g_read_data[32];

static void mock_reset(void)
{
    g_fail_next = 0;
    g_op_count  = 0;
    g_fail_at   = -1;
    memset(g_read_data, 0, sizeof(g_read_data));
}

static bool mock_should_fail(void)
{
    int n = g_op_count++;
    if (g_fail_next) { g_fail_next = 0; return true; }
    if (g_fail_at >= 0 && n == g_fail_at) return true;
    return false;
}

static bool mock_write(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr; (void)data; (void)len;
    if (mock_should_fail()) return false;
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len && i < (uint16_t)sizeof(g_read_data); i++)
        data[i] = g_read_data[i];
    return true;
}

static void mock_delay(void *ctx, uint16_t ms)
{
    (void)ctx; (void)ms;
}

static const sgp30_i2c_t mock_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

static void plant_iaq_response(uint16_t eco2, uint16_t tvoc)
{
    uint8_t w0[2] = { (uint8_t)(eco2 >> 8), (uint8_t)(eco2 & 0xFF) };
    uint8_t w1[2] = { (uint8_t)(tvoc >> 8), (uint8_t)(tvoc & 0xFF) };
    g_read_data[0] = w0[0];
    g_read_data[1] = w0[1];
    g_read_data[2] = sgp30_crc8(w0, 2);
    g_read_data[3] = w1[0];
    g_read_data[4] = w1[1];
    g_read_data[5] = sgp30_crc8(w1, 2);
}

/* --- crc8_deterministic: crc8 of any 2-byte input is deterministic --- */
PROPERTY(crc8_deterministic, 10000) {
    uint8_t data[2];
    data[0] = ARBI_UINT8(seed);
    data[1] = ARBI_UINT8(seed);
    uint8_t c1 = sgp30_crc8(data, 2);
    uint8_t c2 = sgp30_crc8(data, 2);
    PROP_ASSERT_EQ(c1, c2);
}

/* --- init_no_crash: never crashes regardless of I2C state --- */
PROPERTY(init_no_crash, 10000) {
    mock_reset();
    /* Randomly decide if I2C should fail */
    if (ARBI_UINT8(seed) & 1) g_fail_at = 0;
    sgp30_t dev;
    (void)sgp30_init(&dev, &mock_i2c);
    PROP_ASSERT(1);
}

/* --- double_init_safe: repeated init is safe --- */
PROPERTY(double_init_safe, 5000) {
    mock_reset();
    sgp30_t dev;
    sgp30_err_t e1 = sgp30_init(&dev, &mock_i2c);
    sgp30_err_t e2 = sgp30_init(&dev, &mock_i2c);
    PROP_ASSERT_EQ(e1, e2);
}

/* --- measure_with_valid_crc_succeeds: properly CRC'd data always OK --- */
PROPERTY(measure_with_valid_crc, 10000) {
    mock_reset();
    sgp30_t dev;
    sgp30_init(&dev, &mock_i2c);
    g_op_count = 0;

    /* Generate random eCO2 and TVOC values */
    uint16_t eco2 = ARBI_UINT16(seed);
    uint16_t tvoc = ARBI_UINT16(seed);
    plant_iaq_response(eco2, tvoc);

    sgp30_data_t data;
    sgp30_err_t err = sgp30_measure_iaq(&dev, &data);
    PROP_ASSERT(err == SGP30_OK);
    PROP_ASSERT_EQ(data.eco2_ppm, eco2);
    PROP_ASSERT_EQ(data.tvoc_ppb, tvoc);
}

/* --- measure_with_bad_crc_fails: corrupted CRC always detected --- */
PROPERTY(measure_with_bad_crc, 10000) {
    mock_reset();
    sgp30_t dev;
    sgp30_init(&dev, &mock_i2c);
    g_op_count = 0;

    uint16_t eco2 = ARBI_UINT16(seed);
    uint16_t tvoc = ARBI_UINT16(seed);
    plant_iaq_response(eco2, tvoc);

    /* Corrupt one of the CRC bytes */
    int which = ARBI_UINT8(seed) & 1;
    uint8_t corrupt = ARBI_UINT8(seed);
    if (which == 0) {
        /* Only corrupt if different from correct CRC */
        if (corrupt != g_read_data[2]) g_read_data[2] = corrupt;
        else g_read_data[2] = (uint8_t)(corrupt + 1);
    } else {
        if (corrupt != g_read_data[5]) g_read_data[5] = corrupt;
        else g_read_data[5] = (uint8_t)(corrupt + 1);
    }

    sgp30_data_t data;
    sgp30_err_t err = sgp30_measure_iaq(&dev, &data);
    PROP_ASSERT(err == SGP30_ERR_CRC);
}

TEST_MAIN("sgp30 properties") {
    RUN(crc8_deterministic);
    RUN(init_no_crash);
    RUN(double_init_safe);
    RUN(measure_with_valid_crc);
    RUN(measure_with_bad_crc);
}
TEST_MAIN_END()
