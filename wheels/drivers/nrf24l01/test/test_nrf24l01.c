#include "test_harness.h"
#include <string.h>
#include "nrf24l01.h"

/* ---------- mock SPI ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;
static uint8_t mock_cs_state;

static uint8_t xfer_log[256];
static int     xfer_log_idx;

static void mock_reset(void)
{
    mock_fail_next = 0;
    mock_op_count  = 0;
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

static const nrf24l01_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    nrf24l01_err_t e = nrf24l01_init(&mock_spi);
    ASSERT_EQ(e, NRF24L01_OK);
    ASSERT_TRUE(xfer_log_idx > 0);
}

TEST(test_null_safety)
{
    ASSERT_EQ(nrf24l01_init(NULL), NRF24L01_ERR_SPI);
}

TEST(test_init_spi_fail)
{
    mock_reset();
    mock_fail_at = 0;
    nrf24l01_err_t e = nrf24l01_init(&mock_spi);
    ASSERT_TRUE(e != NRF24L01_OK);
}

TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
}

/* TODO: add more tests */

TEST_MAIN("nrf24l01")
    RUN(test_init_ok);
    RUN(test_null_safety);
    RUN(test_init_spi_fail);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
