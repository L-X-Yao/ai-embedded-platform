#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../bmp280.h"

/* ---------- mock I2C ---------- */

#define MOCK_REG_COUNT 256
static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;

    mock_regs[0xD0] = 0x58;

    /* Calibration data from Bosch datasheet example (approximate) */
    mock_regs[0x88] = 0x70; mock_regs[0x89] = 0x6B; /* dig_T1 = 27504 */
    mock_regs[0x8A] = 0x43; mock_regs[0x8B] = 0x67; /* dig_T2 = 26435 */
    mock_regs[0x8C] = 0x18; mock_regs[0x8D] = 0xFC; /* dig_T3 = -1000 */

    mock_regs[0x8E] = 0x7D; mock_regs[0x8F] = 0x8E; /* dig_P1 = 36477 */
    mock_regs[0x90] = 0x43; mock_regs[0x91] = 0xD6; /* dig_P2 = -10685 */
    mock_regs[0x92] = 0xD0; mock_regs[0x93] = 0x0B; /* dig_P3 = 3024 */
    mock_regs[0x94] = 0x27; mock_regs[0x95] = 0x0B; /* dig_P4 = 2855 */
    mock_regs[0x96] = 0x8C; mock_regs[0x97] = 0x00; /* dig_P5 = 140 */
    mock_regs[0x98] = 0xF9; mock_regs[0x99] = 0xFF; /* dig_P6 = -7 */
    mock_regs[0x9A] = 0x8C; mock_regs[0x9B] = 0x3C; /* dig_P7 = 15500 */
    mock_regs[0x9C] = 0xF8; mock_regs[0x9D] = 0xC6; /* dig_P8 = -14600 */
    mock_regs[0x9E] = 0x70; mock_regs[0x9F] = 0x17; /* dig_P9 = 6000 */
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    mock_regs[reg] = val;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len; i++)
        buf[i] = ((reg + i) < MOCK_REG_COUNT) ? mock_regs[reg + i] : 0xFF;
    return true;
}

static const bmp280_i2c_t mock_i2c = {
    .write_reg = mock_write_reg,
    .read_regs = mock_read_regs,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((a)-(b)) < (tol), msg)

static bmp280_t dev;
static bmp280_cfg_t default_cfg = {
    .addr = BMP280_ADDR_LOW,
    .osrs_t = BMP280_OVERSAMPLING_1,
    .osrs_p = BMP280_OVERSAMPLING_1,
};

static void setup(void)
{
    mock_reset();
    bmp280_init(&dev, &mock_i2c, &default_cfg);
}

/* ---------- tests ---------- */

static void test_init_ok(void)
{
    printf("%s\n", __func__);
    mock_reset();
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_OK, "init ok");
}

static void test_init_wrong_id(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mock_regs[0xD0] = 0x00;
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_ERR_ID, "wrong chip ID");
}

static void test_init_i2c_fail(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mock_fail_at = 0;
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_ERR_I2C, "I2C fail on ID read");
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    ASSERT_EQ(bmp280_init(NULL, &mock_i2c, &default_cfg), BMP280_ERR_PARAM, "null dev");
    bmp280_t d;
    ASSERT_EQ(bmp280_init(&d, NULL, &default_cfg), BMP280_ERR_PARAM, "null i2c");
    ASSERT_EQ(bmp280_init(&d, &mock_i2c, NULL), BMP280_ERR_PARAM, "null cfg");
    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(NULL, &data), BMP280_ERR_PARAM, "read null dev");
    ASSERT_EQ(bmp280_read(&d, NULL), BMP280_ERR_PARAM, "read null out");
    ASSERT_EQ(bmp280_reset(NULL), BMP280_ERR_PARAM, "reset null");
}

static void plant_adc(uint32_t adc_p, uint32_t adc_t)
{
    mock_regs[0xF7] = (uint8_t)(adc_p >> 12);
    mock_regs[0xF8] = (uint8_t)(adc_p >> 4);
    mock_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    mock_regs[0xFA] = (uint8_t)(adc_t >> 12);
    mock_regs[0xFB] = (uint8_t)(adc_t >> 4);
    mock_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);
}

static void test_read_temperature(void)
{
    printf("%s\n", __func__);
    setup();

    /* adc_t chosen to produce ~25°C with the example calibration data */
    plant_adc(415148, 519888);

    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&dev, &data);
    ASSERT_EQ(e, BMP280_OK, "read ok");
    ASSERT(data.temperature_c > 15.0f && data.temperature_c < 45.0f,
           "temperature in plausible range");
}

static void test_read_pressure(void)
{
    printf("%s\n", __func__);
    setup();

    plant_adc(415148, 519888);

    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&dev, &data);
    ASSERT_EQ(e, BMP280_OK, "read ok");
    ASSERT(data.pressure_pa > 10000.0f && data.pressure_pa < 200000.0f,
           "pressure in plausible range");
}

static void test_read_i2c_fail(void)
{
    printf("%s\n", __func__);
    setup();
    mock_fail_next = 1;
    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&dev, &data);
    ASSERT_EQ(e, BMP280_ERR_I2C, "read fails on I2C error");
}

static void test_reset(void)
{
    printf("%s\n", __func__);
    setup();
    bmp280_err_t e = bmp280_reset(&dev);
    ASSERT_EQ(e, BMP280_OK, "reset ok");
    ASSERT_EQ(mock_regs[0xE0], 0xB6, "reset command written");
}

static void test_reset_i2c_fail(void)
{
    printf("%s\n", __func__);
    setup();
    mock_fail_next = 1;
    bmp280_err_t e = bmp280_reset(&dev);
    ASSERT_EQ(e, BMP280_ERR_I2C, "reset I2C fail");
}

static void test_addr_high(void)
{
    printf("%s\n", __func__);
    mock_reset();
    bmp280_cfg_t cfg = { .addr = BMP280_ADDR_HIGH, .osrs_t = BMP280_OVERSAMPLING_1,
                         .osrs_p = BMP280_OVERSAMPLING_1 };
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BMP280_OK, "init with addr 0x77");
}

static void test_oversampling_16x(void)
{
    printf("%s\n", __func__);
    mock_reset();
    bmp280_cfg_t cfg = { .addr = BMP280_ADDR_LOW,
                         .osrs_t = BMP280_OVERSAMPLING_16,
                         .osrs_p = BMP280_OVERSAMPLING_16 };
    bmp280_t d;
    bmp280_init(&d, &mock_i2c, &cfg);

    plant_adc(415148, 519888);

    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&d, &data);
    ASSERT_EQ(e, BMP280_OK, "read with 16x oversampling");
    uint8_t ctrl = mock_regs[0xF4];
    ASSERT_EQ((ctrl >> 5) & 0x07, BMP280_OVERSAMPLING_16, "osrs_t=16x in ctrl_meas");
    ASSERT_EQ((ctrl >> 2) & 0x07, BMP280_OVERSAMPLING_16, "osrs_p=16x in ctrl_meas");
}

static void test_calib_readback(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(dev.calib.dig_t1, 27504, "dig_T1");
    ASSERT_EQ(dev.calib.dig_t2, 26435, "dig_T2");
    ASSERT_EQ(dev.calib.dig_t3, -1000, "dig_T3");
    ASSERT_EQ(dev.calib.dig_p1, 36477, "dig_P1");
}

static void test_calib_i2c_fail(void)
{
    printf("%s\n", __func__);
    mock_reset();
    mock_fail_at = 1;
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_ERR_I2C, "calib read I2C fail");
}

int main(void)
{
    printf("test_bmp280\n");
    test_init_ok();
    test_init_wrong_id();
    test_init_i2c_fail();
    test_null_safety();
    test_read_temperature();
    test_read_pressure();
    test_read_i2c_fail();
    test_reset();
    test_reset_i2c_fail();
    test_addr_high();
    test_oversampling_16x();
    test_calib_readback();
    test_calib_i2c_fail();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
