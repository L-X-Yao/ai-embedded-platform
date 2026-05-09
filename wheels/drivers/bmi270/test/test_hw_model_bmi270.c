#include "test_harness.h"
#include "hw_model_bmi270.h"
#include "bmi270.h"

static const bmi270_spi_t hw_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static bmi270_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID and succeeds */
    ASSERT_EQ(bmi270_init(&dev, &hw_spi), BMI270_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    bmi270_init(&dev, &hw_spi);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("bmi270_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
