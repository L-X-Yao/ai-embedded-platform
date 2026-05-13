#include "test_harness.h"
#include "hw_model_max6675.h"
#include "max6675.h"

static const max6675_spi_t hw_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static max6675_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID and succeeds */
    ASSERT_EQ(max6675_init(&dev, &hw_spi), MAX6675_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    max6675_init(&dev, &hw_spi);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("max6675_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
