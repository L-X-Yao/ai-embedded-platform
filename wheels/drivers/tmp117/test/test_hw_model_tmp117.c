#include "test_harness.h"
#include "hw_model_tmp117.h"
#include "tmp117.h"

static const tmp117_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static tmp117_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(tmp117_init(&dev, &hw_i2c), TMP117_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("tmp117_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
