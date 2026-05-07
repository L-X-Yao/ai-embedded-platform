#include "test_harness.h"
#include "mock_i2c.h"
#include "test_dummy2.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* TODO: set default register values (chip ID, etc.) */
    /* mock_regs[0xXX] = 0xYY; */
}

static const test_dummy2_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static test_dummy2_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(test_dummy2_init(&dev, &mock_i2c), TEST_DUMMY2_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(test_dummy2_init(&dev, &mock_i2c), TEST_DUMMY2_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(test_dummy2_init(NULL, &mock_i2c), TEST_DUMMY2_ERR_PARAM);
    ASSERT_EQ(test_dummy2_init(&dev, NULL), TEST_DUMMY2_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    test_dummy2_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* Primary read function on zero-init struct must not crash (no UB) */
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)TEST_DUMMY2_ERR_I2C);
    ASSERT_EQ(-2, (int)TEST_DUMMY2_ERR_ID);
    ASSERT_EQ(-3, (int)TEST_DUMMY2_ERR_PARAM);
}

/* TODO: add more tests */

TEST_MAIN("test_dummy2")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
TEST_MAIN_END()
