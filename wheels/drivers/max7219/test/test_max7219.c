#include "test_harness.h"
#include "max7219.h"
#include <string.h>

/* Mock SPI */
typedef struct {
    uint8_t reg;
    uint8_t val;
} spi_write_t;

static spi_write_t spi_log[64];
static int spi_log_count;
static bool spi_fail;

static void mock_cs_low(void *ctx) { (void)ctx; }
static void mock_cs_high(void *ctx) { (void)ctx; }

static bool mock_transfer(void *ctx, const uint8_t *tx,
                          uint8_t *rx, uint16_t len)
{
    (void)ctx; (void)rx;
    if (spi_fail) return false;
    if (len == 2 && spi_log_count < 64) {
        spi_log[spi_log_count].reg = tx[0];
        spi_log[spi_log_count].val = tx[1];
        spi_log_count++;
    }
    return true;
}

static max7219_spi_t mock_spi = {
    .transfer = mock_transfer,
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .ctx      = NULL
};

static void mock_reset(void)
{
    spi_log_count = 0;
    memset(spi_log, 0, sizeof(spi_log));
    spi_fail = false;
}

TEST(test_init_null)
{
    ASSERT_EQ(max7219_init(NULL, &mock_spi), MAX7219_ERR_PARAM);
    max7219_t dev;
    ASSERT_EQ(max7219_init(&dev, NULL), MAX7219_ERR_PARAM);
}

TEST(test_init_sequence)
{
    mock_reset();
    max7219_t dev;
    ASSERT_EQ(max7219_init(&dev, &mock_spi), MAX7219_OK);
    ASSERT_EQ(spi_log[0].reg, 0x0F);
    ASSERT_EQ(spi_log[0].val, 0x00);
    ASSERT_EQ(spi_log[1].reg, 0x0B);
    ASSERT_EQ(spi_log[1].val, 0x07);
    ASSERT_EQ(spi_log[2].reg, 0x09);
    ASSERT_EQ(spi_log[2].val, 0x00);
    ASSERT_EQ(spi_log[3].reg, 0x0A);
    ASSERT_EQ(spi_log[3].val, 0x08);
    ASSERT_EQ(spi_log[4].reg, 0x0C);
    ASSERT_EQ(spi_log[4].val, 0x01);
}

TEST(test_set_digit)
{
    mock_reset();
    max7219_t dev;
    max7219_init(&dev, &mock_spi);
    spi_log_count = 0;
    ASSERT_EQ(max7219_set_digit(&dev, 3, 0xAB), MAX7219_OK);
    ASSERT_EQ(spi_log[0].reg, 0x04);
    ASSERT_EQ(spi_log[0].val, 0xAB);
}

TEST(test_set_digit_oob)
{
    max7219_t dev;
    max7219_init(&dev, &mock_spi);
    ASSERT_EQ(max7219_set_digit(&dev, 8, 0xFF), MAX7219_ERR_PARAM);
}

TEST(test_intensity)
{
    mock_reset();
    max7219_t dev;
    max7219_init(&dev, &mock_spi);
    spi_log_count = 0;
    ASSERT_EQ(max7219_set_intensity(&dev, 5), MAX7219_OK);
    ASSERT_EQ(spi_log[0].reg, 0x0A);
    ASSERT_EQ(spi_log[0].val, 5);
}

TEST(test_intensity_clamp)
{
    mock_reset();
    max7219_t dev;
    max7219_init(&dev, &mock_spi);
    spi_log_count = 0;
    ASSERT_EQ(max7219_set_intensity(&dev, 0xFF), MAX7219_OK);
    ASSERT_EQ(spi_log[0].val, 0x0F);
}

TEST(test_shutdown)
{
    mock_reset();
    max7219_t dev;
    max7219_init(&dev, &mock_spi);
    spi_log_count = 0;
    ASSERT_EQ(max7219_shutdown(&dev, true), MAX7219_OK);
    ASSERT_EQ(spi_log[0].reg, 0x0C);
    ASSERT_EQ(spi_log[0].val, 0x00);
    ASSERT_EQ(max7219_shutdown(&dev, false), MAX7219_OK);
    ASSERT_EQ(spi_log[1].val, 0x01);
}

TEST(test_clear)
{
    mock_reset();
    max7219_t dev;
    max7219_init(&dev, &mock_spi);
    spi_log_count = 0;
    ASSERT_EQ(max7219_clear(&dev), MAX7219_OK);
    ASSERT_EQ(spi_log_count, 8);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(spi_log[i].reg, 0x01 + i);
        ASSERT_EQ(spi_log[i].val, 0x00);
    }
}

TEST(test_spi_failure)
{
    mock_reset();
    spi_fail = true;
    max7219_t dev;
    ASSERT_EQ(max7219_init(&dev, &mock_spi), MAX7219_ERR_SPI);
}

TEST(test_scan_limit)
{
    mock_reset();
    max7219_t dev;
    max7219_init(&dev, &mock_spi);
    ASSERT_EQ(max7219_set_scan_limit(&dev, 0), MAX7219_ERR_PARAM);
    ASSERT_EQ(max7219_set_scan_limit(&dev, 9), MAX7219_ERR_PARAM);
    spi_log_count = 0;
    ASSERT_EQ(max7219_set_scan_limit(&dev, 4), MAX7219_OK);
    ASSERT_EQ(spi_log[0].reg, 0x0B);
    ASSERT_EQ(spi_log[0].val, 3);
}

TEST_MAIN("max7219")
    RUN(test_init_null);
    RUN(test_init_sequence);
    RUN(test_set_digit);
    RUN(test_set_digit_oob);
    RUN(test_intensity);
    RUN(test_intensity_clamp);
    RUN(test_shutdown);
    RUN(test_clear);
    RUN(test_spi_failure);
    RUN(test_scan_limit);
TEST_MAIN_END()
