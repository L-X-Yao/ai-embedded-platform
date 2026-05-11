#include "test_harness.h"
#include "hw_model_bno055.h"
#include "bno055.h"

static const bno055_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static bno055_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(bno055_init(&dev, &hw_i2c), BNO055_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    bno055_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("bno055_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
