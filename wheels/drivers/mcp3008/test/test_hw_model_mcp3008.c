#include "test_harness.h"
#include "hw_model_mcp3008.h"
#include "mcp3008.h"

static const mcp3008_spi_t hw_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static mcp3008_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    /* TODO: verify init reads correct ID and succeeds */
    ASSERT_EQ(mcp3008_init(&dev, &hw_spi), MCP3008_OK);
}

TEST(test_hw_read_round_trip)
{
    hw_model_reset();
    mcp3008_init(&dev, &hw_spi);
    /* TODO: inject physical values, read back, verify conversion */
}

TEST_MAIN("mcp3008_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_round_trip);
TEST_MAIN_END()
