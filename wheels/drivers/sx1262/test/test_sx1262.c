// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "sx1262.h"

/* ---------- mock SPI ---------- */

#define SPI_LOG_SIZE 4096

static uint8_t spi_log[SPI_LOG_SIZE];
static int     spi_log_idx;
static int     mock_cs_state;
static int     mock_busy;
static int     mock_delay_total;

/* IRQ status returned by GetIrqStatus command */
static uint16_t mock_irq_status;
static int      mock_delay_count;
static int      mock_irq_after_delays;

/* RX buffer status returned by GetRxBufferStatus */
static uint8_t mock_rx_payload_len;
static uint8_t mock_rx_start_offset;

/* Read buffer data */
static uint8_t mock_rx_data[256];
static int     mock_rx_data_idx;

/* Packet status for GetPacketStatus */
static uint8_t mock_rssi_raw;

/* Command tracking */
static uint8_t last_opcode;
static uint8_t last_params[16];
static int     last_param_count;

/* Write buffer capture */
static uint8_t tx_buf_captured[256];
static int     tx_buf_captured_len;
static uint8_t tx_buf_offset;

/* Internal state for SPI transfer mock */
static int     xfer_byte_idx;
static uint8_t current_opcode;

static void mock_reset_all(void)
{
    memset(spi_log, 0, sizeof(spi_log));
    spi_log_idx = 0;
    mock_cs_state = 1;
    mock_busy = 0;
    mock_delay_total = 0;
    mock_irq_status = 0;
    mock_delay_count = 0;
    mock_irq_after_delays = 0;
    mock_rx_payload_len = 0;
    mock_rx_start_offset = 0;
    mock_rx_data_idx = 0;
    mock_rssi_raw = 0;
    last_opcode = 0;
    last_param_count = 0;
    tx_buf_captured_len = 0;
    tx_buf_offset = 0;
    xfer_byte_idx = 0;
    current_opcode = 0;
    memset(mock_rx_data, 0, sizeof(mock_rx_data));
    memset(last_params, 0, sizeof(last_params));
    memset(tx_buf_captured, 0, sizeof(tx_buf_captured));
}

static void mock_cs_select(void *ctx)
{
    (void)ctx;
    mock_cs_state = 0;
    xfer_byte_idx = 0;
    current_opcode = 0;
}

static void mock_cs_deselect(void *ctx)
{
    (void)ctx;
    mock_cs_state = 1;
}

static uint8_t mock_transfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (spi_log_idx < SPI_LOG_SIZE) {
        spi_log[spi_log_idx++] = tx;
    }

    if (xfer_byte_idx == 0) {
        /* Opcode byte */
        current_opcode = tx;
        last_opcode = tx;
        last_param_count = 0;
        xfer_byte_idx++;
        return 0x00;
    }

    xfer_byte_idx++;

    /* Handle specific command responses */
    if (current_opcode == 0x12) {
        /* GetIrqStatus: NOP(status), byte0(MSB), byte1(LSB) */
        if (xfer_byte_idx == 2) return 0x00; /* status */
        if (xfer_byte_idx == 3) return (uint8_t)(mock_irq_status >> 8);
        if (xfer_byte_idx == 4) return (uint8_t)(mock_irq_status & 0xFF);
    }

    if (current_opcode == 0x13) {
        /* GetRxBufferStatus: NOP(status), payload_len, start_offset */
        if (xfer_byte_idx == 2) return 0x00; /* status */
        if (xfer_byte_idx == 3) return mock_rx_payload_len;
        if (xfer_byte_idx == 4) return mock_rx_start_offset;
    }

    if (current_opcode == 0x14) {
        /* GetPacketStatus: NOP(status), rssiPkt, snrPkt, signalRssi */
        if (xfer_byte_idx == 2) return 0x00;
        if (xfer_byte_idx == 3) return mock_rssi_raw;
        if (xfer_byte_idx == 4) return 0x00;
        if (xfer_byte_idx == 5) return 0x00;
    }

    if (current_opcode == 0x1E) {
        /* ReadBuffer: offset, NOP(status), data... */
        if (xfer_byte_idx == 2) { /* offset byte */ return 0x00; }
        if (xfer_byte_idx == 3) return 0x00; /* status */
        if (mock_rx_data_idx < 256)
            return mock_rx_data[mock_rx_data_idx++];
    }

    if (current_opcode == 0x0E) {
        /* WriteBuffer: offset byte, then data */
        if (xfer_byte_idx == 2) {
            tx_buf_offset = tx;
            return 0x00;
        }
        if (tx_buf_captured_len < 256) {
            tx_buf_captured[tx_buf_captured_len++] = tx;
        }
    }

    /* Track params for other commands */
    if (last_param_count < 16) {
        last_params[last_param_count++] = tx;
    }

    return 0x00;
}

static bool mock_is_busy(void *ctx)
{
    (void)ctx;
    return mock_busy != 0;
}

static void mock_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_delay_total += (int)ms;
    mock_delay_count++;

    if (mock_irq_after_delays > 0 &&
        mock_delay_count >= mock_irq_after_delays) {
        /* Simulate IRQ firing */
    }
}

static const sx1262_spi_t mock_spi = {
    .cs_select   = mock_cs_select,
    .cs_deselect = mock_cs_deselect,
    .transfer    = mock_transfer,
    .is_busy     = mock_is_busy,
    .delay_ms    = mock_delay,
    .ctx         = NULL,
};

static const sx1262_cfg_t default_cfg = {
    .frequency_hz     = 868000000UL,
    .tx_power_dbm     = 14,
    .spreading_factor = SX1262_SF7,
    .bandwidth        = SX1262_BW_125K,
    .coding_rate      = SX1262_CR_4_5,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));

    sx1262_err_t e = sx1262_init(&dev, &mock_spi, &default_cfg);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_TRUE(dev.spi == &mock_spi);
    ASSERT_EQ(dev.cfg.frequency_hz, 868000000UL);
    ASSERT_EQ(dev.packet_type, SX1262_PACKET_LORA);
}

TEST(test_init_null_params)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));

    ASSERT_EQ(sx1262_init(NULL, &mock_spi, &default_cfg), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_init(&dev, NULL, &default_cfg), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_init(&dev, &mock_spi, NULL), SX1262_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));

    sx1262_spi_t bad_spi = {
        .cs_select   = mock_cs_select,
        .cs_deselect = NULL,  /* missing */
        .transfer    = mock_transfer,
        .is_busy     = mock_is_busy,
        .delay_ms    = mock_delay,
        .ctx         = NULL,
    };
    ASSERT_EQ(sx1262_init(&dev, &bad_spi, &default_cfg), SX1262_ERR_SPI);
}

TEST(test_set_frequency)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    sx1262_err_t e = sx1262_set_frequency(&dev, 915000000UL);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(dev.cfg.frequency_hz, 915000000UL);
}

TEST(test_set_frequency_null)
{
    ASSERT_EQ(sx1262_set_frequency(NULL, 868000000UL), SX1262_ERR_PARAM);
}

TEST(test_configure_lora)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    sx1262_err_t e = sx1262_configure_lora(&dev, SX1262_SF12,
                                           SX1262_BW_250K, SX1262_CR_4_8);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(dev.cfg.spreading_factor, SX1262_SF12);
    ASSERT_EQ(dev.cfg.bandwidth, SX1262_BW_250K);
    ASSERT_EQ(dev.cfg.coding_rate, SX1262_CR_4_8);
}

TEST(test_configure_lora_invalid_sf)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    /* SF=4 is below minimum */
    ASSERT_EQ(sx1262_configure_lora(&dev, (sx1262_sf_t)4,
              SX1262_BW_125K, SX1262_CR_4_5), SX1262_ERR_PARAM);
    /* SF=13 is above maximum */
    ASSERT_EQ(sx1262_configure_lora(&dev, (sx1262_sf_t)13,
              SX1262_BW_125K, SX1262_CR_4_5), SX1262_ERR_PARAM);
}

TEST(test_configure_lora_invalid_cr)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF7,
              SX1262_BW_125K, (sx1262_cr_t)0), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF7,
              SX1262_BW_125K, (sx1262_cr_t)5), SX1262_ERR_PARAM);
}

TEST(test_transmit_ok)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    /* IRQ fires TxDone after first poll */
    mock_irq_after_delays = 1;
    mock_irq_status = 0x0001; /* IRQ_TX_DONE */

    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    sx1262_err_t e = sx1262_transmit(&dev, payload, 4);
    ASSERT_EQ(e, SX1262_OK);

    /* Verify write buffer captured the payload */
    ASSERT_EQ(tx_buf_captured_len, 4);
    ASSERT_EQ(tx_buf_captured[0], 0xDE);
    ASSERT_EQ(tx_buf_captured[1], 0xAD);
    ASSERT_EQ(tx_buf_captured[2], 0xBE);
    ASSERT_EQ(tx_buf_captured[3], 0xEF);
}

TEST(test_transmit_null)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    ASSERT_EQ(sx1262_transmit(&dev, NULL, 5), SX1262_ERR_PARAM);
    uint8_t d[] = {1};
    ASSERT_EQ(sx1262_transmit(&dev, d, 0), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_transmit(NULL, d, 1), SX1262_ERR_PARAM);
}

TEST(test_transmit_timeout)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    /* IRQ stays 0 -- no TxDone */
    mock_irq_status = 0x0000;

    uint8_t payload[] = {0x01};
    sx1262_err_t e = sx1262_transmit(&dev, payload, 1);
    ASSERT_EQ(e, SX1262_ERR_TIMEOUT);
}

TEST(test_receive_ok)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    /* Pre-load RX data */
    mock_rx_data[0] = 0xCA;
    mock_rx_data[1] = 0xFE;
    mock_rx_data[2] = 0xBA;
    mock_rx_payload_len = 3;
    mock_rx_start_offset = 0x80;

    /* IRQ fires RxDone after first poll */
    mock_irq_after_delays = 1;
    mock_irq_status = 0x0002; /* IRQ_RX_DONE */

    uint8_t buf[16];
    uint8_t len = 0;
    sx1262_err_t e = sx1262_receive(&dev, buf, 16, &len);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(len, 3);
    ASSERT_EQ(buf[0], 0xCA);
    ASSERT_EQ(buf[1], 0xFE);
    ASSERT_EQ(buf[2], 0xBA);
}

TEST(test_receive_truncate)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    mock_rx_data[0] = 0x11;
    mock_rx_data[1] = 0x22;
    mock_rx_data[2] = 0x33;
    mock_rx_data[3] = 0x44;
    mock_rx_payload_len = 4;
    mock_rx_start_offset = 0x80;

    mock_irq_after_delays = 1;
    mock_irq_status = 0x0002;

    uint8_t buf[16];
    uint8_t len = 0;
    /* capacity=2, payload=4 -> truncated to 2 */
    sx1262_err_t e = sx1262_receive(&dev, buf, 2, &len);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(len, 2);
    ASSERT_EQ(buf[0], 0x11);
    ASSERT_EQ(buf[1], 0x22);
}

TEST(test_receive_null)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    uint8_t buf[16];
    uint8_t len;
    ASSERT_EQ(sx1262_receive(NULL, buf, 16, &len), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_receive(&dev, NULL, 16, &len), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_receive(&dev, buf, 16, NULL), SX1262_ERR_PARAM);
}

TEST(test_receive_timeout)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    mock_irq_status = 0x0000;

    uint8_t buf[16];
    uint8_t len = 0;
    sx1262_err_t e = sx1262_receive(&dev, buf, 16, &len);
    ASSERT_EQ(e, SX1262_ERR_TIMEOUT);
}

TEST(test_get_rssi)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    /* rssiPkt=120 -> RSSI = -120/2 = -60 dBm */
    mock_rssi_raw = 120;

    int16_t rssi;
    sx1262_err_t e = sx1262_get_rssi(&dev, &rssi);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(rssi, -60);
}

TEST(test_get_rssi_null)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    int16_t rssi;
    ASSERT_EQ(sx1262_get_rssi(NULL, &rssi), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_get_rssi(&dev, NULL), SX1262_ERR_PARAM);
}

TEST(test_sleep)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    sx1262_err_t e = sx1262_sleep(&dev);
    ASSERT_EQ(e, SX1262_OK);
}

TEST(test_standby)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    sx1262_err_t e = sx1262_standby(&dev);
    ASSERT_EQ(e, SX1262_OK);
}

TEST(test_sleep_null)
{
    ASSERT_EQ(sx1262_sleep(NULL), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_standby(NULL), SX1262_ERR_PARAM);
}

TEST(test_busy_timeout)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    /* Manually set up dev with busy SPI */
    dev.spi = &mock_spi;
    dev.cfg = default_cfg;
    mock_busy = 1; /* permanently busy */

    sx1262_err_t e = sx1262_set_frequency(&dev, 915000000UL);
    ASSERT_EQ(e, SX1262_ERR_BUSY);
}

TEST(test_configure_lora_ldro)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_spi, &default_cfg);

    /* SF11 should enable LDRO */
    sx1262_err_t e = sx1262_configure_lora(&dev, SX1262_SF11,
                                           SX1262_BW_125K, SX1262_CR_4_5);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(dev.cfg.spreading_factor, SX1262_SF11);

    /* SF7 should disable LDRO */
    e = sx1262_configure_lora(&dev, SX1262_SF7,
                              SX1262_BW_125K, SX1262_CR_4_5);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(dev.cfg.spreading_factor, SX1262_SF7);
}

TEST(test_configure_lora_null)
{
    ASSERT_EQ(sx1262_configure_lora(NULL, SX1262_SF7,
              SX1262_BW_125K, SX1262_CR_4_5), SX1262_ERR_PARAM);
}

TEST_MAIN("sx1262")
    RUN(test_init_ok);
    RUN(test_init_null_params);
    RUN(test_init_null_callbacks);
    RUN(test_set_frequency);
    RUN(test_set_frequency_null);
    RUN(test_configure_lora);
    RUN(test_configure_lora_invalid_sf);
    RUN(test_configure_lora_invalid_cr);
    RUN(test_configure_lora_ldro);
    RUN(test_configure_lora_null);
    RUN(test_transmit_ok);
    RUN(test_transmit_null);
    RUN(test_transmit_timeout);
    RUN(test_receive_ok);
    RUN(test_receive_truncate);
    RUN(test_receive_null);
    RUN(test_receive_timeout);
    RUN(test_get_rssi);
    RUN(test_get_rssi_null);
    RUN(test_sleep);
    RUN(test_standby);
    RUN(test_sleep_null);
    RUN(test_busy_timeout);
TEST_MAIN_END()