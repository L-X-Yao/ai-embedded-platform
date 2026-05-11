#include "test_harness.h"
#include "hw_model_hd44780.h"
#include "hd44780.h"

static const hd44780_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static hd44780_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID register and succeeds */
    ASSERT_EQ(hd44780_init(&dev, &hw_i2c), HD44780_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    hd44780_init(&dev, &hw_i2c);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("hd44780_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
