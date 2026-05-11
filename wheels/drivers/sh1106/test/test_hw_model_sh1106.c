#include "test_harness.h"
#include "hw_model_sh1106.h"
#include "sh1106.h"

static const sh1106_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static sh1106_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(sh1106_init(&dev, &hw_i2c), SH1106_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    sh1106_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("sh1106_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
