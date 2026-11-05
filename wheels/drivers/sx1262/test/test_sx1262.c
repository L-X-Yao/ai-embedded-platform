// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "../sx1262.h"
#include <string.h>

/* ---------- mock SPI ---------- */

/* Command log: track what commands were sent */
#define MAX_CMD_LOG 64
static uint8_t cmd_log[MAX_CMD_LOG][16];
static size_t  cmd_log_len[MAX_CMD_LOG];
static int     cmd_log_count;

/* Read data injection */
static uint8_t mock_read_data[16];
static size_t  mock_read_len;

/* Buffer write/read capture */
static uint8_t buf_written[256];
static int     buf_written_len;
static uint8_t buf_read_data[256];
static int     buf_read_len;

/* IRQ status injection */
static uint16_t mock_irq_status;
static int      mock_delay_count;
static int      mock_irq_after_delays;

/* RX buffer status injection */
static uint8_t mock_rx_payload_len;
static uint8_t mock_rx_offset;

/* RSSI injection */
static uint8_t mock_rssi_raw;

/* Busy pin simulation */
static bool mock_busy;
static bool mock_asleep;  /* BUSY stays high until NSS falls */

/* SPI error injection */
static bool mock_spi_error;

/* Status byte */
static uint8_t mock_status_byte;

/* Value returned for any ReadRegister */
static uint8_t mock_reg_read;

static void mock_reset_all(void)
{
    cmd_log_count = 0;
    memset(cmd_log, 0, sizeof(cmd_log));
    memset(cmd_log_len, 0, sizeof(cmd_log_len));
    memset(mock_read_data, 0, sizeof(mock_read_data));
    mock_read_len = 0;
    buf_written_len = 0;
    buf_read_len = 0;
    memset(buf_written, 0, sizeof(buf_written));
    memset(buf_read_data, 0, sizeof(buf_read_data));
    mock_irq_status = 0;
    mock_delay_count = 0;
    mock_irq_after_delays = 0;
    mock_rx_payload_len = 0;
    mock_rx_offset = 0;
    mock_rssi_raw = 0;
    mock_busy = false;
    mock_asleep = false;
    mock_spi_error = false;
    mock_status_byte = 0x22; /* standby mode */
    mock_reg_read = 0;
}

static void mock_cs_select(void *ctx)
{
    (void)ctx;
    mock_asleep = false;
}

static void mock_cs_deselect(void *ctx)
{
    (void)ctx;
}

static void mock_transfer(void *ctx, const uint8_t *tx, uint8_t *rx,
                          size_t len)
{
    (void)ctx;

    if (len == 0) return;

    /* Log the command */
    if (cmd_log_count < MAX_CMD_LOG) {
        size_t copy = len < 16 ? len : 16;
        if (tx) memcpy(cmd_log[cmd_log_count], tx, copy);
        cmd_log_len[cmd_log_count] = len;
        cmd_log_count++;
    }

    uint8_t opcode = tx ? tx[0] : 0;

    if (rx) {
        memset(rx, 0, len);
        /* Status byte is always second byte in response for read commands */
        if (len >= 2) {
            rx[1] = mock_status_byte;
        }
    }

    /* Handle specific opcodes */
    switch (opcode) {
    case 0x0E: /* WriteBuffer */
        if (tx && len > 2) {
            size_t data_len = len - 2;
            if (data_len <= sizeof(buf_written)) {
                memcpy(buf_written, &tx[2], data_len);
                buf_written_len = (int)data_len;
            }
        }
        break;

    case 0x1E: /* ReadBuffer */
        if (rx && len > 3) {
            size_t data_len = len - 3;
            size_t copy = (data_len < (size_t)buf_read_len)
                          ? data_len : (size_t)buf_read_len;
            memcpy(&rx[3], buf_read_data, copy);
        }
        break;

    case 0x12: /* GetIrqStatus */
        if (rx && len >= 4) {
            rx[2] = (uint8_t)(mock_irq_status >> 8);
            rx[3] = (uint8_t)(mock_irq_status & 0xFF);
        }
        break;

    case 0x13: /* GetRxBufferStatus */
        if (rx && len >= 4) {
            rx[2] = mock_rx_payload_len;
            rx[3] = mock_rx_offset;
        }
        break;

    case 0x14: /* GetPacketStatus */
        if (rx && len >= 3) {
            rx[2] = mock_rssi_raw;
        }
        break;

    case 0x1D: /* ReadRegister */
        if (rx && len >= 5) {
            rx[4] = mock_reg_read;
        }
        break;

    case 0xC0: /* GetStatus */
        if (rx && len >= 2) {
            rx[1] = mock_status_byte;
        }
        break;

    default:
        break;
    }
}

static bool mock_is_busy(void *ctx)
{
    (void)ctx;
    return mock_busy || mock_asleep;
}

static bool mock_get_spi_error(void *ctx)
{
    (void)ctx;
    return mock_spi_error;
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
    mock_delay_count++;

    if (mock_irq_after_delays > 0 &&
        mock_delay_count >= mock_irq_after_delays) {
        mock_irq_status = 0x0001; /* TxDone */
    }
}

static const sx1262_hal_t mock_hal = {
    .cs_select      = mock_cs_select,
    .cs_deselect    = mock_cs_deselect,
    .transfer       = mock_transfer,
    .is_busy        = mock_is_busy,
    .delay_ms       = mock_delay_ms,
    .get_spi_error  = mock_get_spi_error,
    .ctx            = NULL,
};

static int last_cmd(uint8_t opcode)
{
    for (int i = cmd_log_count - 1; i >= 0; i--) {
        if (cmd_log[i][0] == opcode) return i;
    }
    return -1;
}

static int last_register_write(uint16_t addr)
{
    for (int i = cmd_log_count - 1; i >= 0; i--) {
        if (cmd_log[i][0] == 0x0D && cmd_log[i][1] == (uint8_t)(addr >> 8) &&
            cmd_log[i][2] == (uint8_t)addr) {
            return i;
        }
    }
    return -1;
}

static void init_dev(sx1262_t *dev)
{
    mock_reset_all();
    memset(dev, 0, sizeof(*dev));
    (void)sx1262_init(dev, &mock_hal);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));

    sx1262_err_t e = sx1262_init(&dev, &mock_hal);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_TRUE(dev.hal == &mock_hal);
    ASSERT_EQ(dev.sf, SX1262_SF7);
    ASSERT_EQ(dev.bw, SX1262_BW_125K);
    ASSERT_EQ(dev.cr, SX1262_CR_4_5);
}

TEST(test_init_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(sx1262_init(NULL, &mock_hal), SX1262_ERR_PARAM);
}

TEST(test_init_null_hal)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(sx1262_init(&dev, NULL), SX1262_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));

    sx1262_hal_t bad_hal = {
        .cs_select   = mock_cs_select,
        .cs_deselect = NULL, /* missing */
        .transfer    = mock_transfer,
        .is_busy     = mock_is_busy,
        .delay_ms    = mock_delay_ms,
        .ctx         = NULL,
    };
    ASSERT_EQ(sx1262_init(&dev, &bad_hal), SX1262_ERR_PARAM);
}

TEST(test_init_sends_standby_and_packet_type)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));

    sx1262_init(&dev, &mock_hal);

    /* SetStandby, GetStatus to verify the chip, then SetPacketType */
    ASSERT_TRUE(cmd_log_count >= 3);
    /* First command: SetStandby (0x80) with RC param */
    ASSERT_EQ(cmd_log[0][0], 0x80);
    ASSERT_EQ(cmd_log[0][1], 0x00); /* STDBY_RC */
    ASSERT_EQ(cmd_log[1][0], 0xC0);
    /* Third command: SetPacketType (0x8A) with LoRa */
    ASSERT_EQ(cmd_log[2][0], 0x8A);
    ASSERT_EQ(cmd_log[2][1], 0x01); /* PACKET_TYPE_LORA */
}

TEST(test_set_frequency_433)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    int saved = cmd_log_count;
    sx1262_err_t e = sx1262_set_frequency(&dev, 433000000UL);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(dev.frequency, 433000000UL);

    /* Verify SetRfFrequency command (0x86) was sent */
    ASSERT_TRUE(cmd_log_count > saved);
    ASSERT_EQ(cmd_log[saved][0], 0x86);

    /* freq_reg = 433000000 * 2^25 / 32000000 = 454033408 = 0x1B0E6800 (approx) */
    uint32_t freq_reg = (uint32_t)(((uint64_t)433000000UL << 25) / 32000000UL);
    ASSERT_EQ(cmd_log[saved][1], (uint8_t)(freq_reg >> 24));
    ASSERT_EQ(cmd_log[saved][2], (uint8_t)(freq_reg >> 16));
    ASSERT_EQ(cmd_log[saved][3], (uint8_t)(freq_reg >> 8));
    ASSERT_EQ(cmd_log[saved][4], (uint8_t)(freq_reg >> 0));
}

TEST(test_set_frequency_915)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    sx1262_err_t e = sx1262_set_frequency(&dev, 915000000UL);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(dev.frequency, 915000000UL);
}

TEST(test_set_frequency_out_of_range)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    ASSERT_EQ(sx1262_set_frequency(&dev, 100000000UL), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_set_frequency(&dev, 1000000000UL), SX1262_ERR_PARAM);
}

TEST(test_configure_lora)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    int saved = cmd_log_count;
    sx1262_err_t e = sx1262_configure_lora(&dev, SX1262_SF12, SX1262_BW_125K,
                                            SX1262_CR_4_8);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(dev.sf, SX1262_SF12);
    ASSERT_EQ(dev.bw, SX1262_BW_125K);
    ASSERT_EQ(dev.cr, SX1262_CR_4_8);

    /* Should have sent SetModulationParams (0x8B) + SetPacketParams (0x8C) */
    ASSERT_TRUE(cmd_log_count >= saved + 2);
    ASSERT_EQ(cmd_log[saved][0], 0x8B);
    ASSERT_EQ(cmd_log[saved][1], 12);    /* SF12 */
    ASSERT_EQ(cmd_log[saved][2], 0x04);  /* BW_125K */
    ASSERT_EQ(cmd_log[saved][3], 0x04);  /* CR_4_8 */

    ASSERT_EQ(cmd_log[saved + 1][0], 0x8C);
}

TEST(test_configure_lora_invalid_sf)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    ASSERT_EQ(sx1262_configure_lora(&dev, (sx1262_sf_t)3, SX1262_BW_125K,
                                     SX1262_CR_4_5), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_configure_lora(&dev, (sx1262_sf_t)15, SX1262_BW_125K,
                                     SX1262_CR_4_5), SX1262_ERR_PARAM);
}

TEST(test_configure_lora_invalid_cr)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF7, SX1262_BW_125K,
                                     (sx1262_cr_t)0), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF7, SX1262_BW_125K,
                                     (sx1262_cr_t)5), SX1262_ERR_PARAM);
}

TEST(test_transmit_ok)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    /* TxDone fires after first delay */
    mock_delay_count = 0;
    mock_irq_after_delays = 2; /* after init delays + 1 poll */
    mock_irq_status = 0;

    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    sx1262_err_t e = sx1262_transmit(&dev, payload, 4);
    ASSERT_EQ(e, SX1262_OK);

    /* Verify buffer was written */
    ASSERT_EQ(buf_written_len, 4);
    ASSERT_EQ(buf_written[0], 0xDE);
    ASSERT_EQ(buf_written[1], 0xAD);
    ASSERT_EQ(buf_written[2], 0xBE);
    ASSERT_EQ(buf_written[3], 0xEF);
}

TEST(test_transmit_null_data)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    ASSERT_EQ(sx1262_transmit(&dev, NULL, 5), SX1262_ERR_PARAM);
}

TEST(test_transmit_zero_len)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    uint8_t payload[] = {0x01};
    ASSERT_EQ(sx1262_transmit(&dev, payload, 0), SX1262_ERR_PARAM);
}

TEST(test_transmit_timeout)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    /* IRQ never fires */
    mock_irq_status = 0;
    mock_irq_after_delays = 0;

    uint8_t payload[] = {0x01};
    sx1262_err_t e = sx1262_transmit(&dev, payload, 1);
    ASSERT_EQ(e, SX1262_ERR_TIMEOUT);
}

TEST(test_receive_ok)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    /* Inject RX data */
    buf_read_data[0] = 0xCA;
    buf_read_data[1] = 0xFE;
    buf_read_data[2] = 0xBA;
    buf_read_len = 3;
    mock_rx_payload_len = 3;
    mock_rx_offset = 0;

    /* RxDone fires after first poll */
    mock_delay_count = 0;
    mock_irq_after_delays = 0;
    mock_irq_status = 0x0002; /* RxDone */

    uint8_t buf[16];
    uint8_t len = 0;
    sx1262_err_t e = sx1262_receive(&dev, buf, 16, &len);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(len, 3);
    ASSERT_EQ(buf[0], 0xCA);
    ASSERT_EQ(buf[1], 0xFE);
    ASSERT_EQ(buf[2], 0xBA);
}

TEST(test_receive_truncates)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    buf_read_data[0] = 0x11;
    buf_read_data[1] = 0x22;
    buf_read_data[2] = 0x33;
    buf_read_data[3] = 0x44;
    buf_read_len = 4;
    mock_rx_payload_len = 4;
    mock_rx_offset = 0;
    mock_irq_status = 0x0002;

    uint8_t buf[2];
    uint8_t len = 0;
    sx1262_err_t e = sx1262_receive(&dev, buf, 2, &len);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(len, 2);
    ASSERT_EQ(buf[0], 0x11);
    ASSERT_EQ(buf[1], 0x22);
}

TEST(test_receive_timeout)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    mock_irq_status = 0;
    mock_irq_after_delays = 0;

    uint8_t buf[16];
    uint8_t len = 0;
    sx1262_err_t e = sx1262_receive(&dev, buf, 16, &len);
    ASSERT_EQ(e, SX1262_ERR_TIMEOUT);
}

TEST(test_receive_null_params)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    uint8_t buf[16];
    uint8_t len = 0;
    ASSERT_EQ(sx1262_receive(&dev, NULL, 16, &len), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_receive(&dev, buf, 16, NULL), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_receive(&dev, buf, 0, &len), SX1262_ERR_PARAM);
}

TEST(test_get_rssi)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    /* RSSI raw = 160 → -160/2 = -80 dBm */
    mock_rssi_raw = 160;
    int16_t rssi = sx1262_get_rssi(&dev);
    ASSERT_EQ(rssi, -80);

    /* RSSI raw = 100 → -100/2 = -50 dBm */
    mock_rssi_raw = 100;
    rssi = sx1262_get_rssi(&dev);
    ASSERT_EQ(rssi, -50);

    /* RSSI raw = 0 → 0 dBm */
    mock_rssi_raw = 0;
    rssi = sx1262_get_rssi(&dev);
    ASSERT_EQ(rssi, 0);
}

TEST(test_sleep)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    int saved = cmd_log_count;
    sx1262_err_t e = sx1262_sleep(&dev);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(cmd_log[saved][0], 0x84); /* SetSleep */
    ASSERT_EQ(cmd_log[saved][1], 0x04); /* warm start */
}

TEST(test_standby)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    int saved = cmd_log_count;
    sx1262_err_t e = sx1262_standby(&dev);
    ASSERT_EQ(e, SX1262_OK);
    ASSERT_EQ(cmd_log[saved][0], 0x80); /* SetStandby */
    ASSERT_EQ(cmd_log[saved][1], 0x00); /* RC */
}

TEST(test_get_status)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    mock_status_byte = 0x44;
    uint8_t s = sx1262_get_status(&dev);
    ASSERT_EQ(s, 0x44);
}

TEST(test_get_status_null)
{
    mock_reset_all();
    ASSERT_EQ(sx1262_get_status(NULL), 0);
}

TEST(test_sleep_null)
{
    mock_reset_all();
    ASSERT_EQ(sx1262_sleep(NULL), SX1262_ERR_PARAM);
}

TEST(test_standby_null)
{
    mock_reset_all();
    ASSERT_EQ(sx1262_standby(NULL), SX1262_ERR_PARAM);
}

TEST(test_set_frequency_null)
{
    mock_reset_all();
    ASSERT_EQ(sx1262_set_frequency(NULL, 433000000UL), SX1262_ERR_PARAM);
}

TEST(test_configure_lora_null)
{
    mock_reset_all();
    ASSERT_EQ(sx1262_configure_lora(NULL, SX1262_SF7, SX1262_BW_125K,
                                     SX1262_CR_4_5), SX1262_ERR_PARAM);
}

TEST(test_get_rssi_null)
{
    mock_reset_all();
    ASSERT_EQ(sx1262_get_rssi(NULL), 0);
}

TEST(test_busy_timeout)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));

    /* Make busy always true */
    mock_busy = true;

    /* Init should fail because busy never clears */
    sx1262_err_t e = sx1262_init(&dev, &mock_hal);
    ASSERT_EQ(e, SX1262_ERR_BUSY);
}

TEST(test_init_spi_error)
{
    mock_reset_all();
    mock_spi_error = true; /* SPI bus failure on first send_command */
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(sx1262_init(&dev, &mock_hal), SX1262_ERR_SPI);
}

TEST(test_set_frequency_boundary)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);

    /* Exact boundaries */
    ASSERT_EQ(sx1262_set_frequency(&dev, 150000000UL), SX1262_OK);
    ASSERT_EQ(sx1262_set_frequency(&dev, 960000000UL), SX1262_OK);
    ASSERT_EQ(sx1262_set_frequency(&dev, 149999999UL), SX1262_ERR_PARAM);
    ASSERT_EQ(sx1262_set_frequency(&dev, 960000001UL), SX1262_ERR_PARAM);
}

TEST(test_init_rejects_silent_bus)
{
    mock_reset_all();
    mock_status_byte = 0x00; /* nothing answered */
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(sx1262_init(&dev, &mock_hal), SX1262_ERR_SPI);
}

TEST(test_init_configures_power_amplifier)
{
    sx1262_t dev;
    init_dev(&dev);

    int pa = last_cmd(0x95);
    ASSERT_TRUE(pa >= 0);
    ASSERT_EQ(cmd_log[pa][1], 0x04); /* paDutyCycle */
    ASSERT_EQ(cmd_log[pa][2], 0x07); /* hpMax */
    ASSERT_EQ(cmd_log[pa][3], 0x00); /* deviceSel: SX1262 */
    ASSERT_EQ(cmd_log[pa][4], 0x01); /* paLut */

    int tx = last_cmd(0x8E);
    ASSERT_TRUE(tx > pa);
    ASSERT_EQ(cmd_log[tx][1], 14);   /* dBm */
    ASSERT_EQ(cmd_log[tx][2], 0x04); /* 200 us ramp */
}

TEST(test_init_sets_tx_clamp)
{
    mock_reset_all();
    mock_reg_read = 0x40;
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(sx1262_init(&dev, &mock_hal), SX1262_OK);

    int w = last_register_write(0x08D8);
    ASSERT_TRUE(w >= 0);
    ASSERT_EQ(cmd_log[w][3], 0x5E);
}

TEST(test_transmit_sets_payload_length)
{
    sx1262_t dev;
    uint8_t data[10] = { 0 };
    init_dev(&dev);
    mock_irq_after_delays = 1;

    ASSERT_EQ(sx1262_transmit(&dev, data, sizeof(data)), SX1262_OK);

    int params = last_cmd(0x8C);
    ASSERT_TRUE(params >= 0);
    ASSERT_EQ(cmd_log[params][4], 10);
    ASSERT_TRUE(params < last_cmd(0x83));
}

TEST(test_transmit_timeout_covers_time_on_air)
{
    sx1262_t dev;
    uint8_t data[255] = { 0 };
    init_dev(&dev);
    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF12, SX1262_BW_125K, SX1262_CR_4_5),
              SX1262_OK);
    mock_irq_after_delays = 1;

    ASSERT_EQ(sx1262_transmit(&dev, data, sizeof(data)), SX1262_OK);

    /* 275.25 symbols take 9020 ms; with 1000 ms margin that is 641280 RTC steps. */
    int tx = last_cmd(0x83);
    ASSERT_TRUE(tx >= 0);
    ASSERT_EQ(cmd_log[tx][1], 0x09);
    ASSERT_EQ(cmd_log[tx][2], 0xC9);
    ASSERT_EQ(cmd_log[tx][3], 0x00);
}

TEST(test_transmit_reports_chip_timeout)
{
    sx1262_t dev;
    uint8_t data[4] = { 0 };
    init_dev(&dev);
    mock_delay_count = 0;
    mock_irq_status = 0x0200;

    ASSERT_EQ(sx1262_transmit(&dev, data, sizeof(data)), SX1262_ERR_TIMEOUT);
    ASSERT_EQ(mock_delay_count, 1);

    int irq = last_cmd(0x08);
    ASSERT_EQ(cmd_log[irq][1], 0x02); /* Timeout */
    ASSERT_EQ(cmd_log[irq][2], 0x01); /* TxDone */
}

TEST(test_receive_restores_max_length)
{
    sx1262_t dev;
    uint8_t data[4] = { 0 };
    uint8_t out[8];
    uint8_t out_len = 0;
    init_dev(&dev);
    mock_irq_after_delays = 1;
    ASSERT_EQ(sx1262_transmit(&dev, data, sizeof(data)), SX1262_OK);

    mock_irq_after_delays = 0;
    mock_irq_status = 0x0200;
    (void)sx1262_receive(&dev, out, sizeof(out), &out_len);

    ASSERT_EQ(cmd_log[last_cmd(0x8C)][4], 0xFF);
}

TEST(test_receive_reports_crc_error)
{
    sx1262_t dev;
    uint8_t out[8];
    uint8_t out_len = 0;
    init_dev(&dev);
    mock_irq_status = 0x0042; /* RxDone with CrcErr */

    ASSERT_EQ(sx1262_receive(&dev, out, sizeof(out), &out_len), SX1262_ERR_CRC);
    ASSERT_EQ(last_cmd(0x1E), -1); /* nothing read from the buffer */
}

TEST(test_receive_waits_three_seconds)
{
    sx1262_t dev;
    uint8_t out[8];
    uint8_t out_len = 0;
    init_dev(&dev);
    mock_irq_status = 0x0200;

    ASSERT_EQ(sx1262_receive(&dev, out, sizeof(out), &out_len), SX1262_ERR_TIMEOUT);

    int rx = last_cmd(0x82);
    ASSERT_EQ(cmd_log[rx][1], 0x02); /* 3000 ms = 192000 steps = 0x02EE00 */
    ASSERT_EQ(cmd_log[rx][2], 0xEE);
    ASSERT_EQ(cmd_log[rx][3], 0x00);
}

TEST(test_receive_stops_rtc_after_timeout)
{
    sx1262_t dev;
    uint8_t out[8];
    uint8_t out_len = 0;
    init_dev(&dev);
    mock_reg_read = 0x10;
    mock_irq_status = 0x0200;

    ASSERT_EQ(sx1262_receive(&dev, out, sizeof(out), &out_len), SX1262_ERR_TIMEOUT);

    int rtc = last_register_write(0x0902);
    ASSERT_TRUE(rtc >= 0);
    ASSERT_EQ(cmd_log[rtc][3], 0x00);

    int evt = last_register_write(0x0944);
    ASSERT_TRUE(evt > rtc);
    ASSERT_EQ(cmd_log[evt][3], 0x12);
}

TEST(test_get_rssi_reads_packet_status)
{
    sx1262_t dev;
    init_dev(&dev);

    mock_rssi_raw = 161;
    ASSERT_EQ(sx1262_get_rssi(&dev), -81); /* rounds down, as Semtech's >> 1 */
    ASSERT_TRUE(last_cmd(0x14) >= 0);
}

TEST(test_get_rssi_reports_spi_error_as_zero)
{
    sx1262_t dev;
    init_dev(&dev);

    mock_rssi_raw = 160;
    mock_spi_error = true;
    ASSERT_EQ(sx1262_get_rssi(&dev), 0);
}

TEST(test_configure_lora_invalid_bw)
{
    sx1262_t dev;
    init_dev(&dev);
    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF7, (sx1262_bw_t)7, SX1262_CR_4_5),
              SX1262_ERR_PARAM);
}

TEST(test_configure_lora_low_data_rate_optimize)
{
    sx1262_t dev;
    init_dev(&dev);

    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF10, SX1262_BW_125K, SX1262_CR_4_5),
              SX1262_OK);
    ASSERT_EQ(cmd_log[last_cmd(0x8B)][4], 0);

    /* 16.38 ms symbols and longer: Semtech lists SF11/125k and SF12/250k. */
    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF11, SX1262_BW_125K, SX1262_CR_4_5),
              SX1262_OK);
    ASSERT_EQ(cmd_log[last_cmd(0x8B)][4], 1);

    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF12, SX1262_BW_250K, SX1262_CR_4_5),
              SX1262_OK);
    ASSERT_EQ(cmd_log[last_cmd(0x8B)][4], 1);

    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF12, SX1262_BW_500K, SX1262_CR_4_5),
              SX1262_OK);
    ASSERT_EQ(cmd_log[last_cmd(0x8B)][4], 0);
}

TEST(test_configure_lora_tx_modulation_workaround)
{
    sx1262_t dev;
    init_dev(&dev);

    mock_reg_read = 0x04;
    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF7, SX1262_BW_500K, SX1262_CR_4_5),
              SX1262_OK);
    ASSERT_EQ(cmd_log[last_register_write(0x0889)][3], 0x00);

    mock_reg_read = 0x00;
    ASSERT_EQ(sx1262_configure_lora(&dev, SX1262_SF7, SX1262_BW_125K, SX1262_CR_4_5),
              SX1262_OK);
    ASSERT_EQ(cmd_log[last_register_write(0x0889)][3], 0x04);
}

TEST(test_standby_wakes_the_chip_from_sleep)
{
    mock_reset_all();
    sx1262_t dev;
    memset(&dev, 0, sizeof(dev));
    sx1262_init(&dev, &mock_hal);
    ASSERT_EQ(sx1262_sleep(&dev), SX1262_OK);

    /* BUSY stays high through SLEEP, and only NSS falling ends it */
    mock_asleep = true;
    ASSERT_EQ(sx1262_standby(&dev), SX1262_OK);
    ASSERT_FALSE(mock_asleep);
}

TEST_MAIN("sx1262")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_hal);
    RUN(test_init_spi_error);
    RUN(test_init_null_callbacks);
    RUN(test_init_sends_standby_and_packet_type);
    RUN(test_set_frequency_433);
    RUN(test_set_frequency_915);
    RUN(test_set_frequency_out_of_range);
    RUN(test_set_frequency_boundary);
    RUN(test_set_frequency_null);
    RUN(test_configure_lora);
    RUN(test_configure_lora_invalid_sf);
    RUN(test_configure_lora_invalid_cr);
    RUN(test_configure_lora_null);
    RUN(test_transmit_ok);
    RUN(test_transmit_null_data);
    RUN(test_transmit_zero_len);
    RUN(test_transmit_timeout);
    RUN(test_receive_ok);
    RUN(test_receive_truncates);
    RUN(test_receive_timeout);
    RUN(test_receive_null_params);
    RUN(test_get_rssi);
    RUN(test_get_rssi_null);
    RUN(test_sleep);
    RUN(test_standby);
    RUN(test_sleep_null);
    RUN(test_standby_null);
    RUN(test_get_status);
    RUN(test_get_status_null);
    RUN(test_busy_timeout);
    RUN(test_init_rejects_silent_bus);
    RUN(test_init_configures_power_amplifier);
    RUN(test_init_sets_tx_clamp);
    RUN(test_transmit_sets_payload_length);
    RUN(test_transmit_timeout_covers_time_on_air);
    RUN(test_transmit_reports_chip_timeout);
    RUN(test_receive_restores_max_length);
    RUN(test_receive_reports_crc_error);
    RUN(test_receive_waits_three_seconds);
    RUN(test_receive_stops_rtc_after_timeout);
    RUN(test_get_rssi_reads_packet_status);
    RUN(test_get_rssi_reports_spi_error_as_zero);
    RUN(test_configure_lora_invalid_bw);
    RUN(test_configure_lora_low_data_rate_optimize);
    RUN(test_configure_lora_tx_modulation_workaround);
    RUN(test_standby_wakes_the_chip_from_sleep);
TEST_MAIN_END()
