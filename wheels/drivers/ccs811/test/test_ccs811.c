#include "test_harness.h"
#include "mock_i2c.h"
#include "ccs811.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* TODO: set default register values (chip ID, etc.) */
    /* mock_regs[0xXX] = 0xYY; */
}

static const ccs811_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static ccs811_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(ccs811_init(&dev, &mock_i2c), CCS811_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(ccs811_init(&dev, &mock_i2c), CCS811_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(ccs811_init(NULL, &mock_i2c), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_init(&dev, NULL), CCS811_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers — init must reject it */
    ccs811_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ccs811_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ccs811_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)CCS811_ERR_I2C);
    ASSERT_EQ(-2, (int)CCS811_ERR_ID);
    ASSERT_EQ(-3, (int)CCS811_ERR_PARAM);
}

/* TODO: add more tests */

TEST_MAIN("ccs811")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
TEST_MAIN_END()
