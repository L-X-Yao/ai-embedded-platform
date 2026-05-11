#include "test_harness.h"
#include "hw_model_ms5611.h"
#include "ms5611.h"

static const ms5611_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static ms5611_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(ms5611_init(&dev, &hw_i2c), MS5611_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    ms5611_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("ms5611_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
