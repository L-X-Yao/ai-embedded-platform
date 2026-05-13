#include "test_harness.h"
#include "hw_model_bmm150.h"
#include "bmm150.h"

static const bmm150_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static bmm150_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(bmm150_init(&dev, &hw_i2c), BMM150_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    bmm150_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("bmm150_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
