#include "test_harness.h"
#include "hw_model_adxl345.h"
#include "adxl345.h"

static const adxl345_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static adxl345_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(adxl345_init(&dev, &hw_i2c), ADXL345_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    adxl345_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("adxl345_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
