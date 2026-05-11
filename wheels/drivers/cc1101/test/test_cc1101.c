// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "cc1101.h"

/* ---------- mock SPI ---------- */

static uint8_t mock_regs[256];
static int     mock_xfer_count;
static uint8_t mock_cs_state;
static int     mock_delay_total;

/* FIFO write capture */
static uint8_t fifo_written[256];
static int     fifo_written_len;

/* FIFO read data */
static uint8_t fifo_read_data[256];
static int     fifo_read_len;
static int     fifo_read_idx;

/* SPI transaction tracking */
static uint8_t spi_current_header;
static int     spi_byte_index;
static int     spi_is_read;
static int     spi_is_burst;

/* Deferred state change */
static int     mock_delay_count;
static int     mock_idle_after_delays;

static void mock_reset_all(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_xfer_count = 0;
    mock_cs_state = 1;
    mock_delay_total = 0;
    fifo_written_len = 0;
    fifo_read_len = 0;
    fifo_read_idx = 0;
    spi_current_header = 0;
    spi_byte_index = 0;
    spi_is_read = 0;
    spi_is_burst = 0;
    mock_delay_count = 0;
    mock_idle_after_delays = 0;
    memset(fifo_written, 0, sizeof(fifo_written));
    memset(fifo_read_data, 0, sizeof(fifo_read_data));

    /* Set chip identity */
    mock_regs[0x30] = 0x00; /* PARTNUM */
    mock_regs[0x31] = 0x14; /* VERSION */
    mock_regs[0x35] = 0x01; /* MARCSTATE = IDLE */
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_cs_state = 0;
    spi_byte_index = 0;
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_cs_state = 1;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    mock_xfer_count++;

    if (spi_byte_index == 0) {
        spi_current_header = tx;
        spi_is_read = (tx & 0x80) ? 1 : 0;
        spi_is_burst = (tx & 0x40) ? 1 : 0;
        spi_byte_index = 1;

        uint8_t addr = tx & 0x3F;

        /* Command strobes (0x30-0x3D): single byte, no data follows */
        if (addr >= 0x30 && addr <= 0x3D && !spi_is_read && !spi_is_burst) {
            /* Strobe command */
            return 0x00;
        }
        return 0x00;
    }

    /* Data byte */
    uint8_t addr = spi_current_header & 0x3F;

    if (spi_is_read) {
        /* Read */
        if (addr == 0x3F && spi_is_burst) {
            /* RX FIFO burst read */
            if (fifo_read_idx < fifo_read_len)
                return fifo_read_data[fifo_read_idx++];
            return 0x00;
        }
        /* Regular register read (status regs use burst bit for read) */
        return mock_regs[addr];
    } else {
        /* Write */
        if (addr == 0x3F && spi_is_burst) {
            /* TX FIFO burst write */
            if (fifo_written_len < (int)sizeof(fifo_written))
                fifo_written[fifo_written_len++] = tx;
        } else {
            mock_regs[addr] = tx;
        }
        return 0x00;
    }
}

static void mock_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_delay_total += (int)ms;
    mock_delay_count++;

    if (mock_idle_after_delays > 0 &&
        mock_delay_count >= mock_idle_after_delays) {
        mock_regs[0x35] = 0x01; /* MARCSTATE = IDLE */
    }
}

static const cc1101_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

static const cc1101_cfg_t default_cfg = {
    .frequency_hz = 433000000UL,
    .data_rate    = 0x0C,
    .modulation   = 0,  /* 2-FSK */
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));

    cc1101_err_t e = cc1101_init(&dev, &mock_spi, &default_cfg);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_TRUE(dev.spi == &mock_spi);
    ASSERT_EQ(dev.cfg.frequency_hz, 433000000UL);
}

TEST(test_init_version_fail)
{
    mock_reset_all();
    mock_regs[0x31] = 0xFF; /* Wrong version */

    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_err_t e = cc1101_init(&dev, &mock_spi, &default_cfg);
    ASSERT_EQ(e, CC1101_ERR_VERSION);
}

TEST(test_init_null_params)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));

    ASSERT_EQ(cc1101_init(NULL, &mock_spi, &default_cfg), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_init(&dev, NULL, &default_cfg), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_init(&dev, &mock_spi, NULL), CC1101_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));

    cc1101_spi_t bad_spi = {
        .cs_low   = mock_cs_low,
        .cs_high  = NULL,  /* missing */
        .xfer     = mock_xfer,
        .delay_ms = mock_delay,
        .ctx      = NULL,
    };
    ASSERT_EQ(cc1101_init(&dev, &bad_spi, &default_cfg), CC1101_ERR_SPI);
}

TEST(test_set_frequency_433)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    /* FREQ = 433e6 * 2^16 / 26e6 = 1091799 = 0x10A762 (approx) */
    uint32_t freq_word = ((uint32_t)mock_regs[0x0D] << 16) |
                         ((uint32_t)mock_regs[0x0E] << 8) |
                         ((uint32_t)mock_regs[0x0F]);
    uint32_t expected = (uint32_t)(((uint64_t)433000000UL << 16) / 26000000UL);
    ASSERT_EQ(freq_word, expected);
}

TEST(test_set_frequency_out_of_range)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    ASSERT_EQ(cc1101_set_frequency(&dev, 200000000UL), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_set_frequency(&dev, 1000000000UL), CC1101_ERR_PARAM);
}

TEST(test_set_frequency_null)
{
    ASSERT_EQ(cc1101_set_frequency(NULL, 433000000UL), CC1101_ERR_PARAM);
}

TEST(test_set_power)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    cc1101_err_t e = cc1101_set_power(&dev, 0xC0);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(mock_regs[0x3E], 0xC0);
}

TEST(test_set_power_null)
{
    ASSERT_EQ(cc1101_set_power(NULL, 0xC0), CC1101_ERR_PARAM);
}

TEST(test_transmit_ok)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    /* MARCSTATE returns IDLE on first poll */
    mock_regs[0x35] = 0x01;

    uint8_t payload[] = {0xDE, 0xAD, 0xBE};
    cc1101_err_t e = cc1101_transmit(&dev, payload, 3);
    ASSERT_EQ(e, CC1101_OK);

    /* Verify FIFO: length byte + payload */
    ASSERT_EQ(fifo_written_len, 4);
    ASSERT_EQ(fifo_written[0], 3);    /* length */
    ASSERT_EQ(fifo_written[1], 0xDE);
    ASSERT_EQ(fifo_written[2], 0xAD);
    ASSERT_EQ(fifo_written[3], 0xBE);
}

TEST(test_transmit_null)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    ASSERT_EQ(cc1101_transmit(&dev, NULL, 5), CC1101_ERR_PARAM);
    uint8_t d[] = {1};
    ASSERT_EQ(cc1101_transmit(&dev, d, 0), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_transmit(NULL, d, 1), CC1101_ERR_PARAM);
}

TEST(test_transmit_too_long)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    uint8_t big[62];
    memset(big, 0xAA, sizeof(big));
    ASSERT_EQ(cc1101_transmit(&dev, big, 62), CC1101_ERR_PARAM);
}

TEST(test_transmit_timeout)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    /* MARCSTATE stays in TX, never returns to IDLE */
    mock_regs[0x35] = 0x13;

    uint8_t payload[] = {0x01};
    cc1101_err_t e = cc1101_transmit(&dev, payload, 1);
    ASSERT_EQ(e, CC1101_ERR_TIMEOUT);
}

TEST(test_receive_ok)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    /* Pre-load FIFO data */
    fifo_read_data[0] = 0xCA;
    fifo_read_data[1] = 0xFE;
    fifo_read_len = 2;

    /* RXBYTES > 0 on first poll, pkt_len = 2 */
    mock_regs[0x3B] = 3;  /* RXBYTES */
    mock_regs[0x3F] = 2;  /* pkt_len in FIFO (read via single reg read) */

    uint8_t buf[16];
    uint8_t len = 0;
    cc1101_err_t e = cc1101_receive(&dev, buf, 16, &len);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(len, 2);
    ASSERT_EQ(buf[0], 0xCA);
    ASSERT_EQ(buf[1], 0xFE);
}

TEST(test_receive_null)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    uint8_t buf[16];
    uint8_t len;
    ASSERT_EQ(cc1101_receive(NULL, buf, 16, &len), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_receive(&dev, NULL, 16, &len), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_receive(&dev, buf, 16, NULL), CC1101_ERR_PARAM);
}

TEST(test_receive_timeout)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    mock_regs[0x3B] = 0; /* RXBYTES stays 0 */

    uint8_t buf[16];
    uint8_t len = 0;
    cc1101_err_t e = cc1101_receive(&dev, buf, 16, &len);
    ASSERT_EQ(e, CC1101_ERR_TIMEOUT);
}

TEST(test_get_rssi)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    /* raw=200 (>=128): RSSI = (200-256)/2 - 74 = -56/2 - 74 = -28 - 74 = -102 */
    mock_regs[0x34] = 200;
    int16_t rssi;
    cc1101_err_t e = cc1101_get_rssi(&dev, &rssi);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(rssi, -102);

    /* raw=80 (<128): RSSI = 80/2 - 74 = 40 - 74 = -34 */
    mock_regs[0x34] = 80;
    e = cc1101_get_rssi(&dev, &rssi);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(rssi, -34);
}

TEST(test_get_rssi_null)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    int16_t rssi;
    ASSERT_EQ(cc1101_get_rssi(NULL, &rssi), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_get_rssi(&dev, NULL), CC1101_ERR_PARAM);
}

TEST(test_idle)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    cc1101_err_t e = cc1101_idle(&dev);
    ASSERT_EQ(e, CC1101_OK);
}

TEST(test_idle_null)
{
    ASSERT_EQ(cc1101_idle(NULL), CC1101_ERR_PARAM);
}

TEST(test_set_frequency_868)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_spi, &default_cfg);

    cc1101_err_t e = cc1101_set_frequency(&dev, 868000000UL);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(dev.cfg.frequency_hz, 868000000UL);

    uint32_t freq_word = ((uint32_t)mock_regs[0x0D] << 16) |
                         ((uint32_t)mock_regs[0x0E] << 8) |
                         ((uint32_t)mock_regs[0x0F]);
    uint32_t expected = (uint32_t)(((uint64_t)868000000UL << 16) / 26000000UL);
    ASSERT_EQ(freq_word, expected);
}

TEST(test_modulation_config)
{
    mock_reset_all();
    cc1101_cfg_t gfsk_cfg = {
        .frequency_hz = 433000000UL,
        .data_rate = 0x0C,
        .modulation = 1,  /* GFSK */
    };

    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_err_t e = cc1101_init(&dev, &mock_spi, &gfsk_cfg);
    ASSERT_EQ(e, CC1101_OK);

    /* MDMCFG2: modulation bits [6:4] = 001 for GFSK */
    ASSERT_EQ(mock_regs[0x12] & 0x70, 0x10);
}

TEST_MAIN("cc1101")
    RUN(test_init_ok);
    RUN(test_init_version_fail);
    RUN(test_init_null_params);
    RUN(test_init_null_callbacks);
    RUN(test_set_frequency_433);
    RUN(test_set_frequency_868);
    RUN(test_set_frequency_out_of_range);
    RUN(test_set_frequency_null);
    RUN(test_set_power);
    RUN(test_set_power_null);
    RUN(test_transmit_ok);
    RUN(test_transmit_null);
    RUN(test_transmit_too_long);
    RUN(test_transmit_timeout);
    RUN(test_receive_ok);
    RUN(test_receive_null);
    RUN(test_receive_timeout);
    RUN(test_get_rssi);
    RUN(test_get_rssi_null);
    RUN(test_idle);
    RUN(test_idle_null);
    RUN(test_modulation_config);
TEST_MAIN_END()