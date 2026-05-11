#include "test_harness.h"
#include "mock_i2c.h"
#include "ms5611.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* TODO: set default register values (chip ID, etc.) */
    /* mock_regs[0xXX] = 0xYY; */
}

static const ms5611_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static ms5611_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(ms5611_init(&dev, &mock_i2c), MS5611_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(ms5611_init(&dev, &mock_i2c), MS5611_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(ms5611_init(NULL, &mock_i2c), MS5611_ERR_PARAM);
    ASSERT_EQ(ms5611_init(&dev, NULL), MS5611_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers — init must reject it */
    ms5611_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ms5611_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ms5611_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)MS5611_ERR_I2C);
    ASSERT_EQ(-2, (int)MS5611_ERR_ID);
    ASSERT_EQ(-3, (int)MS5611_ERR_PARAM);
}

/* TODO: add more tests */

TEST_MAIN("ms5611")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
TEST_MAIN_END()
