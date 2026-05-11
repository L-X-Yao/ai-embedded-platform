#include "test_harness.h"
#include "hw_model_sx1262.h"
#include "sx1262.h"

static const sx1262_spi_t hw_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static sx1262_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID and succeeds */
    ASSERT_EQ(sx1262_init(&dev, &hw_spi), SX1262_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    sx1262_init(&dev, &hw_spi);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("sx1262_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
