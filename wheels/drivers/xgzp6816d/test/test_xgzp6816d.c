#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../xgzp6816d.h"

/* ---------- mock I2C transport ---------- */

#define MOCK_BUF_MAX  16

static int      mock_write_called;
static int      mock_read_called;
static uint8_t  mock_last_addr;
static uint8_t  mock_last_write[MOCK_BUF_MAX];
static uint16_t mock_last_write_len;
static uint8_t  mock_read_response[MOCK_BUF_MAX];
static uint16_t mock_read_response_len;
static int      mock_write_should_fail;
static int      mock_read_should_fail;

static int tests_run, tests_fail;

static void mock_reset(void)
{
    mock_write_called = 0;
    mock_read_called = 0;
    mock_last_addr = 0;
    memset(mock_last_write, 0, sizeof(mock_last_write));
    mock_last_write_len = 0;
    memset(mock_read_response, 0, sizeof(mock_read_response));
    mock_read_response_len = 0;
    mock_write_should_fail = 0;
    mock_read_should_fail = 0;
}

static bool mock_write(void *ctx, uint8_t dev_addr, const uint8_t *buf, uint16_t len)
{
    (void)ctx;
    mock_write_called++;
    mock_last_addr = dev_addr;
    if (len > MOCK_BUF_MAX) return false;
    if (buf && len > 0) memcpy(mock_last_write, buf, len);
    mock_last_write_len = len;
    return mock_write_should_fail ? false : true;
}

static bool mock_read(void *ctx, uint8_t dev_addr, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    mock_read_called++;
    mock_last_addr = dev_addr;
    if (mock_read_should_fail) return false;
    if (len > mock_read_response_len) return false;
    memcpy(buf, mock_read_response, len);
    return true;
}

static const xgzp6816d_i2c_t mock_i2c = {
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL,
};

/* ---------- helpers ---------- */

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)

#define ASSERT_NEAR(a, b, tol, msg) \
    ASSERT(fabsf((float)(a) - (float)(b)) < (float)(tol), msg)

static void mock_set_pt_response(uint32_t p_raw, uint16_t t_raw)
{
    mock_read_response[0] = 0x00;                          /* status */
    mock_read_response[1] = (uint8_t)((p_raw >> 16) & 0xFF);
    mock_read_response[2] = (uint8_t)((p_raw >>  8) & 0xFF);
    mock_read_response[3] = (uint8_t)( p_raw        & 0xFF);
    mock_read_response[4] = (uint8_t)((t_raw >>  8) & 0xFF);
    mock_read_response[5] = (uint8_t)( t_raw        & 0xFF);
    mock_read_response_len = 6;
}

/* ---------- tests ---------- */

static void test_probe_ok(void)
{
    printf("test_probe_ok\n");
    mock_reset();
    xgzp6816d_err_t e = xgzp6816d_probe(&mock_i2c);
    ASSERT_EQ(e, XGZP6816D_OK, "probe OK");
    ASSERT_EQ(mock_write_called, 1, "one write call");
    ASSERT_EQ(mock_last_addr, XGZP6816D_DEV_ADDR, "addr matches");
    ASSERT_EQ(mock_last_write_len, 0, "zero-length write");
}

static void test_probe_no_ack(void)
{
    printf("test_probe_no_ack\n");
    mock_reset();
    mock_write_should_fail = 1;
    xgzp6816d_err_t e = xgzp6816d_probe(&mock_i2c);
    ASSERT_EQ(e, XGZP6816D_ERR_NO_ACK, "no-ack detected");
}

static void test_trigger_ok(void)
{
    printf("test_trigger_ok\n");
    mock_reset();
    xgzp6816d_err_t e = xgzp6816d_trigger(&mock_i2c);
    ASSERT_EQ(e, XGZP6816D_OK, "trigger OK");
    ASSERT_EQ(mock_write_called, 1, "one write call");
    ASSERT_EQ(mock_last_addr, XGZP6816D_DEV_ADDR, "addr matches");
    ASSERT_EQ(mock_last_write_len, 1, "one byte written");
    ASSERT_EQ(mock_last_write[0], 0xAC, "MEASURE_PT cmd");
}

static void test_trigger_i2c_err(void)
{
    printf("test_trigger_i2c_err\n");
    mock_reset();
    mock_write_should_fail = 1;
    xgzp6816d_err_t e = xgzp6816d_trigger(&mock_i2c);
    ASSERT_EQ(e, XGZP6816D_ERR_I2C, "trigger surfaces i2c error");
}

static void test_read_midrange(void)
{
    printf("test_read_midrange\n");
    mock_reset();
    /* p_raw = 8388608 (mid 24-bit) → ~70 kPa = 70000 Pa */
    /* t_raw = 22417 → ~25 °C */
    mock_set_pt_response(8388608, 22417);
    xgzp6816d_data_t out = {0};
    xgzp6816d_err_t e = xgzp6816d_read(&mock_i2c, &out);
    ASSERT_EQ(e, XGZP6816D_OK, "read OK");
    ASSERT_NEAR(out.pressure_pa,    70000.0f, 1.0f, "pressure ~70 kPa");
    ASSERT_NEAR(out.temperature_c,    25.0f, 0.05f, "temp ~25 °C");
}

static void test_read_low_rail(void)
{
    printf("test_read_low_rail\n");
    mock_reset();
    mock_set_pt_response(1677722, 0);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 30000.0f, 1.0f, "low rail = 30 kPa");
}

static void test_read_below_low_clamps(void)
{
    printf("test_read_below_low_clamps\n");
    mock_reset();
    mock_set_pt_response(1000000, 0);   /* below low rail */
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 30000.0f, 1.0f, "clamped to 30 kPa");
}

static void test_read_high_rail(void)
{
    printf("test_read_high_rail\n");
    mock_reset();
    mock_set_pt_response(15099494, 0);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.pressure_pa, 110000.0f, 1.0f, "high rail = 110 kPa");
}

static void test_read_above_high_clamps(void)
{
    printf("test_read_above_high_clamps\n");
    mock_reset();
    mock_set_pt_response(16000000, 0);   /* above high rail */
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    /* This is the NEW behavior — pre-wheel flybox driver only clamped low,
     * letting saturated readings produce 119 kPa garbage. */
    ASSERT_NEAR(out.pressure_pa, 110000.0f, 1.0f, "clamped to 110 kPa");
}

static void test_read_temp_min(void)
{
    printf("test_read_temp_min\n");
    mock_reset();
    mock_set_pt_response(8388608, 0);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    ASSERT_NEAR(out.temperature_c, -40.0f, 0.001f, "t_raw=0 → -40 °C");
}

static void test_read_temp_max(void)
{
    printf("test_read_temp_max\n");
    mock_reset();
    mock_set_pt_response(8388608, 65535);
    xgzp6816d_data_t out = {0};
    xgzp6816d_read(&mock_i2c, &out);
    /* (65535 / 65536) * 190 - 40 ≈ 149.997 */
    ASSERT_NEAR(out.temperature_c, 150.0f, 0.01f, "t_raw=65535 → ~150 °C");
}

static void test_read_i2c_err(void)
{
    printf("test_read_i2c_err\n");
    mock_reset();
    mock_read_should_fail = 1;
    xgzp6816d_data_t out = {0};
    xgzp6816d_err_t e = xgzp6816d_read(&mock_i2c, &out);
    ASSERT_EQ(e, XGZP6816D_ERR_I2C, "read surfaces i2c error");
}

static void test_full_cycle(void)
{
    printf("test_full_cycle\n");
    mock_reset();
    /* probe → trigger → (caller waits) → read */
    ASSERT_EQ(xgzp6816d_probe(&mock_i2c), XGZP6816D_OK, "probe");
    ASSERT_EQ(xgzp6816d_trigger(&mock_i2c), XGZP6816D_OK, "trigger");
    mock_set_pt_response(8388608, 22417);
    xgzp6816d_data_t out = {0};
    ASSERT_EQ(xgzp6816d_read(&mock_i2c, &out), XGZP6816D_OK, "read");
    ASSERT(out.pressure_pa > 0.0f, "pressure positive");
    ASSERT(out.temperature_c > -40.0f && out.temperature_c < 150.0f, "temp in range");
}

/* ---------- main ---------- */

int main(void)
{
    test_probe_ok();
    test_probe_no_ack();
    test_trigger_ok();
    test_trigger_i2c_err();
    test_read_midrange();
    test_read_low_rail();
    test_read_below_low_clamps();
    test_read_high_rail();
    test_read_above_high_clamps();
    test_read_temp_min();
    test_read_temp_max();
    test_read_i2c_err();
    test_full_cycle();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
