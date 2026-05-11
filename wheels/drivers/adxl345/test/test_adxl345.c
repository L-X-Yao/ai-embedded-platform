#include "test_harness.h"
#include "mock_i2c.h"
#include "adxl345.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* TODO: set default register values (chip ID, etc.) */
    /* mock_regs[0xXX] = 0xYY; */
}

static const adxl345_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static adxl345_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(adxl345_init(&dev, &mock_i2c), ADXL345_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(adxl345_init(&dev, &mock_i2c), ADXL345_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(adxl345_init(NULL, &mock_i2c), ADXL345_ERR_PARAM);
    ASSERT_EQ(adxl345_init(&dev, NULL), ADXL345_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers — init must reject it */
    adxl345_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    adxl345_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(adxl345_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)ADXL345_ERR_I2C);
    ASSERT_EQ(-2, (int)ADXL345_ERR_ID);
    ASSERT_EQ(-3, (int)ADXL345_ERR_PARAM);
}

/* TODO: add more tests */

TEST_MAIN("adxl345")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
TEST_MAIN_END()
