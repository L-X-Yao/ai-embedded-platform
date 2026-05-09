#include "test_harness.h"
#include <string.h>
#include "ili9341.h"

/* ---------- mock SPI ---------- */

static int     mock_fail_at;
static uint8_t mock_cs_state;

static uint8_t xfer_log[256];
static int     xfer_log_idx;

static void mock_reset(void)
{
    mock_fail_at   = -1;
    mock_cs_state  = 1;
    xfer_log_idx   = 0;
    memset(xfer_log, 0, sizeof(xfer_log));
}

static void mock_cs_low(void *ctx)  { (void)ctx; mock_cs_state = 0; }
static void mock_cs_high(void *ctx) { (void)ctx; mock_cs_state = 1; }

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (xfer_log_idx < (int)sizeof(xfer_log))
        xfer_log[xfer_log_idx++] = tx;
    return 0xFF; /* override for device-specific responses */
}

static const ili9341_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ili9341_t dev;
    ili9341_err_t e = ili9341_init(&dev, &mock_spi);
    ASSERT_EQ(e, ILI9341_OK);
}

TEST(test_null_safety)
{
    ili9341_t dev;
    ASSERT_EQ(ili9341_init(NULL, &mock_spi), ILI9341_ERR_PARAM);
    ASSERT_EQ(ili9341_init(&dev, NULL), ILI9341_ERR_PARAM);
}

TEST(test_init_spi_fail)
{
    /* TODO: implement once init reads chip ID via SPI.
     * Set mock to return wrong ID, verify ERR_ID returned. */
    (void)mock_fail_at;
    ASSERT_TRUE(ILI9341_ERR_SPI < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)ILI9341_ERR_PARAM);
    ASSERT_EQ(-2, (int)ILI9341_ERR_SPI);
    ASSERT_EQ(-3, (int)ILI9341_ERR_ID);
    ASSERT_EQ(-4, (int)ILI9341_ERR_TIMEOUT);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers — init must reject it */
    ili9341_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    ili9341_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(ili9341_init(&zero_dev, &zero_hal) < 0);
}

/* TODO: add more tests */

TEST_MAIN("ili9341")
    RUN(test_init_ok);
    RUN(test_null_safety);
    RUN(test_init_spi_fail);
    RUN(test_error_codes);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
