// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "../cc1101.h"
#include <string.h>

/* ---------- mock SPI ---------- */

static uint8_t mock_regs[64];       /* config registers 0x00-0x3F */
static uint8_t mock_status_regs[64]; /* status registers 0x30-0x3F */
static uint8_t mock_patable;

/* TX FIFO capture */
static uint8_t fifo_tx[64];
static int     fifo_tx_len;

/* RX FIFO inject */
static uint8_t fifo_rx[64];
static int     fifo_rx_len;
static int     fifo_rx_idx;

/* Chip state: tracks current mode */
static uint8_t mock_marcstate;

/* SPI state machine */
static uint8_t spi_addr;
static int     spi_byte_idx;
static int     spi_is_write;
static int     spi_is_burst;
static int     spi_is_status;

/* Deferred state change */
static int     mock_delay_count;
static int     mock_idle_after_delays;

static void mock_reset_all(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    memset(mock_status_regs, 0, sizeof(mock_status_regs));
    mock_patable = 0;
    fifo_tx_len = 0;
    fifo_rx_len = 0;
    fifo_rx_idx = 0;
    memset(fifo_tx, 0, sizeof(fifo_tx));
    memset(fifo_rx, 0, sizeof(fifo_rx));
    mock_marcstate = 0x01; /* IDLE */
    spi_addr = 0;
    spi_byte_idx = 0;
    spi_is_write = 0;
    spi_is_burst = 0;
    spi_is_status = 0;
    mock_delay_count = 0;
    mock_idle_after_delays = 0;

    /* Set expected chip ID */
    mock_status_regs[0x00] = 0x00; /* PARTNUM */
    mock_status_regs[0x01] = 0x14; /* VERSION */
}

static void mock_cs_select(void *ctx)
{
    (void)ctx;
    spi_byte_idx = 0;
}

static void mock_cs_deselect(void *ctx)
{
    (void)ctx;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    uint8_t status_byte = (uint8_t)((mock_marcstate & 0x07) << 4);

    if (spi_byte_idx == 0) {
        /* Header byte */
        spi_is_write  = (tx & 0x80) == 0;
        spi_is_burst  = (tx & 0x40) != 0;
        spi_is_status = (tx & 0xC0) == 0xC0;
        spi_addr      = tx & 0x3F;

        spi_byte_idx = 1;

        /* Command strobe: 0x30-0x3D, single byte, no data follows */
        if (spi_addr >= 0x30 && spi_addr <= 0x3D && !spi_is_burst && !spi_is_status) {
            /* Handle strobes */
            switch (tx & 0x3F) {
            case 0x30: /* SRES */
                mock_marcstate = 0x01;
                break;
            case 0x34: /* SRX */
                mock_marcstate = 0x0D;
                break;
            case 0x35: /* STX */
                mock_marcstate = 0x13;
                break;
            case 0x36: /* SIDLE */
                mock_marcstate = 0x01;
                break;
            case 0x3A: /* SFRX */
                fifo_rx_idx = 0;
                break;
            case 0x3B: /* SFTX */
                fifo_tx_len = 0;
                break;
            default:
                break;
            }
        }

        return status_byte;
    }

    /* Data byte(s) */
    if (spi_is_status) {
        /* Status register read: addr 0x30-0x3F mapped to mock_status_regs */
        uint8_t reg_idx = spi_addr - 0x30;
        if (reg_idx < sizeof(mock_status_regs)) {
            if (spi_addr == 0x35) {
                /* MARCSTATE */
                return mock_marcstate & 0x1F;
            }
            if (spi_addr == 0x3B) {
                /* RXBYTES */
                return (uint8_t)(fifo_rx_len - fifo_rx_idx);
            }
            return mock_status_regs[reg_idx];
        }
        return 0;
    }

    if (spi_addr == 0x3F) {
        /* FIFO access */
        if (spi_is_burst && !spi_is_write) {
            /* RX FIFO burst read — handled by the 0xC0|0x3F = status read path above */
            /* But if we're in burst read without status flag, also return FIFO data */
        }
        if (spi_is_write || (!spi_is_write && (spi_addr == 0x3F) && !spi_is_status)) {
            if (spi_is_write) {
                /* TX FIFO write */
                if (fifo_tx_len < (int)sizeof(fifo_tx)) {
                    fifo_tx[fifo_tx_len++] = tx;
                }
                return 0;
            }
        }
        /* Read from FIFO (non-status, non-burst — single byte) */
        if (fifo_rx_idx < fifo_rx_len) {
            return fifo_rx[fifo_rx_idx++];
        }
        return 0;
    }

    if (spi_addr == 0x3E) {
        /* PATABLE */
        if (spi_is_write) {
            mock_patable = tx;
            return 0;
        }
        return mock_patable;
    }

    /* Regular register */
    if (spi_is_write) {
        if (spi_addr < sizeof(mock_regs)) {
            mock_regs[spi_addr] = tx;
        }
        return 0;
    } else {
        if (spi_addr < sizeof(mock_regs)) {
            return mock_regs[spi_addr];
        }
        return 0;
    }
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
    mock_delay_count++;

    if (mock_idle_after_delays > 0 &&
        mock_delay_count >= mock_idle_after_delays) {
        mock_marcstate = 0x01; /* IDLE */
    }
}

static const cc1101_hal_t mock_hal = {
    .cs_select   = mock_cs_select,
    .cs_deselect = mock_cs_deselect,
    .xfer        = mock_xfer,
    .delay_ms    = mock_delay_ms,
    .ctx         = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));

    cc1101_err_t e = cc1101_init(&dev, &mock_hal);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_TRUE(dev.hal == &mock_hal);
}

TEST(test_init_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(cc1101_init(NULL, &mock_hal), CC1101_ERR_PARAM);
}

TEST(test_init_null_hal)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(cc1101_init(&dev, NULL), CC1101_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));

    cc1101_hal_t bad = {
        .cs_select   = mock_cs_select,
        .cs_deselect = mock_cs_deselect,
        .xfer        = NULL,
        .delay_ms    = mock_delay_ms,
        .ctx         = NULL,
    };
    ASSERT_EQ(cc1101_init(&dev, &bad), CC1101_ERR_PARAM);
}

TEST(test_init_bad_version)
{
    mock_reset_all();
    mock_status_regs[0x01] = 0xFF; /* wrong VERSION */

    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(cc1101_init(&dev, &mock_hal), CC1101_ERR_ID);
}

TEST(test_init_bad_partnum)
{
    mock_reset_all();
    mock_status_regs[0x00] = 0x99; /* wrong PARTNUM */

    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(cc1101_init(&dev, &mock_hal), CC1101_ERR_ID);
}

TEST(test_set_frequency_433)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    cc1101_err_t e = cc1101_set_frequency(&dev, 433000000UL);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(dev.frequency, 433000000UL);

    /* FREQ = 433e6 * 2^16 / 26e6 = 1090785 = 0x10A762 (approx) */
    uint32_t freq_word = (uint32_t)(((uint64_t)433000000UL << 16) / 26000000UL);
    ASSERT_EQ(mock_regs[0x0D], (uint8_t)(freq_word >> 16));
    ASSERT_EQ(mock_regs[0x0E], (uint8_t)(freq_word >> 8));
    ASSERT_EQ(mock_regs[0x0F], (uint8_t)(freq_word >> 0));
}

TEST(test_set_frequency_868)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    cc1101_err_t e = cc1101_set_frequency(&dev, 868000000UL);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(dev.frequency, 868000000UL);
}

TEST(test_set_frequency_out_of_range)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    ASSERT_EQ(cc1101_set_frequency(&dev, 100000000UL), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_set_frequency(&dev, 1000000000UL), CC1101_ERR_PARAM);
}

TEST(test_set_frequency_boundary)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    ASSERT_EQ(cc1101_set_frequency(&dev, 300000000UL), CC1101_OK);
    ASSERT_EQ(cc1101_set_frequency(&dev, 928000000UL), CC1101_OK);
    ASSERT_EQ(cc1101_set_frequency(&dev, 299999999UL), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_set_frequency(&dev, 928000001UL), CC1101_ERR_PARAM);
}

TEST(test_set_power)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    cc1101_err_t e = cc1101_set_power(&dev, 0xC0);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(mock_patable, 0xC0);
}

TEST(test_transmit_ok)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    /* After STX, mock returns to idle after some delays */
    mock_delay_count = 0;
    mock_idle_after_delays = 2;
    mock_marcstate = 0x01;

    uint8_t payload[] = {0xCA, 0xFE, 0xBA, 0xBE};
    cc1101_err_t e = cc1101_transmit(&dev, payload, 4);
    ASSERT_EQ(e, CC1101_OK);

    /* FIFO should contain: length byte (4) + payload */
    ASSERT_EQ(fifo_tx[0], 4);
    ASSERT_EQ(fifo_tx[1], 0xCA);
    ASSERT_EQ(fifo_tx[2], 0xFE);
    ASSERT_EQ(fifo_tx[3], 0xBA);
    ASSERT_EQ(fifo_tx[4], 0xBE);
}

TEST(test_transmit_null_data)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    ASSERT_EQ(cc1101_transmit(&dev, NULL, 5), CC1101_ERR_PARAM);
}

TEST(test_transmit_zero_len)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    uint8_t data[] = {0x01};
    ASSERT_EQ(cc1101_transmit(&dev, data, 0), CC1101_ERR_PARAM);
}

TEST(test_transmit_too_long)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    uint8_t data[62];
    memset(data, 0xAA, sizeof(data));
    ASSERT_EQ(cc1101_transmit(&dev, data, 62), CC1101_ERR_PARAM);
}

TEST(test_transmit_timeout)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    /* MARCSTATE stays in TX, never returns to IDLE */
    mock_marcstate = 0x13;
    mock_idle_after_delays = 0;

    uint8_t data[] = {0x01};
    cc1101_err_t e = cc1101_transmit(&dev, data, 1);
    ASSERT_EQ(e, CC1101_ERR_TIMEOUT);
}

TEST(test_receive_ok)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    /* Pre-load RX FIFO: length byte + data */
    fifo_rx[0] = 3;     /* length byte (read via single-byte reg_read) */
    fifo_rx[1] = 0xDE;  /* data (read via burst) */
    fifo_rx[2] = 0xAD;
    fifo_rx[3] = 0x00;
    fifo_rx_len = 4;

    /* After SRX, return to IDLE after some delays */
    mock_delay_count = 0;
    mock_idle_after_delays = 2;

    uint8_t buf[16];
    uint8_t len = 0;
    cc1101_err_t e = cc1101_receive(&dev, buf, 16, &len);
    ASSERT_EQ(e, CC1101_OK);
    /* We read 3 bytes as per length byte */
    ASSERT_TRUE(len > 0);
}

TEST(test_receive_null_params)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    uint8_t buf[16];
    uint8_t len = 0;
    ASSERT_EQ(cc1101_receive(&dev, NULL, 16, &len), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_receive(&dev, buf, 16, NULL), CC1101_ERR_PARAM);
    ASSERT_EQ(cc1101_receive(&dev, buf, 0, &len), CC1101_ERR_PARAM);
}

TEST(test_receive_timeout)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    /* Stay in RX forever */
    mock_marcstate = 0x0D;
    mock_idle_after_delays = 0;

    uint8_t buf[16];
    uint8_t len = 0;
    cc1101_err_t e = cc1101_receive(&dev, buf, 16, &len);
    ASSERT_EQ(e, CC1101_ERR_TIMEOUT);
}

TEST(test_get_rssi_positive_raw)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    /* raw = 100 → 100/2 - 74 = -24 dBm */
    mock_status_regs[0x04] = 100; /* SREG_RSSI = 0x34, offset 0x04 */
    int16_t rssi = cc1101_get_rssi(&dev);
    ASSERT_EQ(rssi, -24);
}

TEST(test_get_rssi_negative_raw)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    /* raw = 200 (>=128) → (200-256)/2 - 74 = -28/2 - 74 = -102 dBm */
    mock_status_regs[0x04] = 200;
    int16_t rssi = cc1101_get_rssi(&dev);
    ASSERT_EQ(rssi, -102);
}

TEST(test_get_rssi_null)
{
    mock_reset_all();
    ASSERT_EQ(cc1101_get_rssi(NULL), 0);
}

TEST(test_idle)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    mock_marcstate = 0x0D; /* RX */
    cc1101_err_t e = cc1101_idle(&dev);
    ASSERT_EQ(e, CC1101_OK);
    ASSERT_EQ(mock_marcstate, 0x01); /* IDLE */
}

TEST(test_idle_null)
{
    mock_reset_all();
    ASSERT_EQ(cc1101_idle(NULL), CC1101_ERR_PARAM);
}

TEST(test_set_frequency_null)
{
    mock_reset_all();
    ASSERT_EQ(cc1101_set_frequency(NULL, 433000000UL), CC1101_ERR_PARAM);
}

TEST(test_set_power_null)
{
    mock_reset_all();
    ASSERT_EQ(cc1101_set_power(NULL, 0xC0), CC1101_ERR_PARAM);
}

TEST(test_transmit_null_dev)
{
    mock_reset_all();
    uint8_t data[] = {0x01};
    ASSERT_EQ(cc1101_transmit(NULL, data, 1), CC1101_ERR_PARAM);
}

TEST(test_receive_null_dev)
{
    mock_reset_all();
    uint8_t buf[16];
    uint8_t len = 0;
    ASSERT_EQ(cc1101_receive(NULL, buf, 16, &len), CC1101_ERR_PARAM);
}

TEST(test_transmit_max_valid_len)
{
    mock_reset_all();
    cc1101_t dev;
    memset(&dev, 0, sizeof(dev));
    cc1101_init(&dev, &mock_hal);

    mock_delay_count = 0;
    mock_idle_after_delays = 2;

    uint8_t data[61];
    memset(data, 0x55, sizeof(data));
    cc1101_err_t e = cc1101_transmit(&dev, data, 61);
    ASSERT_EQ(e, CC1101_OK);
}

TEST_MAIN("cc1101")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_hal);
    RUN(test_init_null_callbacks);
    RUN(test_init_bad_version);
    RUN(test_init_bad_partnum);
    RUN(test_set_frequency_433);
    RUN(test_set_frequency_868);
    RUN(test_set_frequency_out_of_range);
    RUN(test_set_frequency_boundary);
    RUN(test_set_frequency_null);
    RUN(test_set_power);
    RUN(test_set_power_null);
    RUN(test_transmit_ok);
    RUN(test_transmit_null_data);
    RUN(test_transmit_zero_len);
    RUN(test_transmit_too_long);
    RUN(test_transmit_timeout);
    RUN(test_transmit_null_dev);
    RUN(test_transmit_max_valid_len);
    RUN(test_receive_ok);
    RUN(test_receive_null_params);
    RUN(test_receive_timeout);
    RUN(test_receive_null_dev);
    RUN(test_get_rssi_positive_raw);
    RUN(test_get_rssi_negative_raw);
    RUN(test_get_rssi_null);
    RUN(test_idle);
    RUN(test_idle_null);
TEST_MAIN_END()
