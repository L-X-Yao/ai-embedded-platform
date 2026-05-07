#include "test_harness.h"
#include "hw_model_test_dummy2.h"
#include "test_dummy2.h"

static const test_dummy2_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static test_dummy2_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(test_dummy2_init(&dev, &hw_i2c), TEST_DUMMY2_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    test_dummy2_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("test_dummy2_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
