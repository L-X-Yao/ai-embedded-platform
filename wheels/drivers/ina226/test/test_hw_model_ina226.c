#include "test_harness.h"
#include "hw_model_ina226.h"
#include "ina226.h"

static const ina226_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static ina226_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(ina226_init(&dev, &hw_i2c), INA226_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("ina226_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
