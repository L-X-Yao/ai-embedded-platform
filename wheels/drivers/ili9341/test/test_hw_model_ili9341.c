#include "test_harness.h"
#include "hw_model_ili9341.h"
#include "ili9341.h"

static const ili9341_spi_t hw_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static ili9341_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID and succeeds */
    ASSERT_EQ(ili9341_init(&dev, &hw_spi), ILI9341_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    ili9341_init(&dev, &hw_spi);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("ili9341_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
