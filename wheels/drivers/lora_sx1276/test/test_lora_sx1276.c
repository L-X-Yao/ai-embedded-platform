// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "lora_sx1276.h"

/* ---------- mock SPI ---------- */

/* Register file: mock the SX1276 register space */
static uint8_t mock_regs[128];
static int     mock_xfer_count;
static uint8_t mock_cs_state;
static int     mock_delay_total;
static int     mock_reset_count;

/* FIFO write capture */
static uint8_t fifo_written[256];
static int     fifo_written_len;

/* FIFO read data (pre-loaded for receive tests) */
static uint8_t fifo_read_data[256];
static int     fifo_read_len;
static int     fifo_read_idx;

/* SPI transaction tracking */
static uint8_t spi_current_addr;
static int     spi_byte_index;  /* 0=address byte, 1+=data bytes */
static int     spi_is_write;

/* Failure injection */
static int     mock_fail_spi;

/* RegImageCal: 1 = run in FSK mode, -1 = run in LoRa mode, 0 = never */
static int     mock_image_cal_seen;
static int     mock_image_cal_stuck;

/* Deferred IRQ: set mock_regs[0x12] after N delay_ms calls */
static int     mock_delay_count;
static int     mock_irq_after_delays;  /* 0 = disabled */
static uint8_t mock_irq_value;

static void mock_reset_all(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_xfer_count = 0;
    mock_cs_state = 1;
    mock_delay_total = 0;
    mock_reset_count = 0;
    mock_image_cal_seen = 0;
    mock_image_cal_stuck = 0;
    mock_regs[0x1C] = 0x40;  /* CrcOnPayload */
    fifo_written_len = 0;
    fifo_read_len = 0;
    fifo_read_idx = 0;
    spi_current_addr = 0;
    spi_byte_index = 0;
    spi_is_write = 0;
    mock_fail_spi = 0;
    mock_delay_count = 0;
    mock_irq_after_delays = 0;
    mock_irq_value = 0;
    memset(fifo_written, 0, sizeof(fifo_written));
    memset(fifo_read_data, 0, sizeof(fifo_read_data));

    /* Set version register to SX1276 default */
    mock_regs[0x42] = 0x12;
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
        /* address byte */
        spi_is_write = (tx & 0x80) ? 1 : 0;
        spi_current_addr = tx & 0x7F;
        spi_byte_index = 1;
        return 0x00;
    }

    /* data byte(s) */
    if (spi_is_write) {
        if (spi_current_addr == 0x00) {
            /* FIFO write */
            if (fifo_written_len < (int)sizeof(fifo_written))
                fifo_written[fifo_written_len++] = tx;
        } else if (spi_current_addr == 0x12) {
            /* IRQ flags: write-1-to-clear (real SX1276 behavior) */
            mock_regs[0x12] = (uint8_t)(mock_regs[0x12] & ~tx);
        } else if (spi_current_addr == 0x01) {
            /* LongRangeMode only changes in Sleep */
            if ((mock_regs[0x01] & 0x07) != 0) {
                tx = (uint8_t)((tx & 0x7F) | (mock_regs[0x01] & 0x80));
            }
            mock_regs[0x01] = tx;
        } else if (spi_current_addr == 0x3B) {
            if (tx & 0x40) mock_image_cal_seen = (mock_regs[0x01] & 0x80) ? -1 : 1;
            mock_regs[0x3B] = mock_image_cal_stuck ? (uint8_t)(tx | 0x20) : tx;
        } else {
            mock_regs[spi_current_addr] = tx;
        }
        return 0x00;
    } else {
        /* read */
        if (spi_current_addr == 0x00) {
            /* FIFO read */
            if (fifo_read_idx < fifo_read_len)
                return fifo_read_data[fifo_read_idx++];
            return 0x00;
        }
        return mock_regs[spi_current_addr];
    }
}

static void mock_hw_reset(void *ctx)
{
    (void)ctx;
    mock_reset_count++;
    mock_regs[0x01] = 0x09;  /* FSK, standby */
}

static void mock_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_delay_total += (int)ms;
    mock_delay_count++;

    /* Simulate hardware setting IRQ flags after some delay */
    if (mock_irq_after_delays > 0 &&
        mock_delay_count >= mock_irq_after_delays) {
        mock_regs[0x12] |= mock_irq_value;
    }
}

static const lora_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .reset    = mock_hw_reset,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));

    lora_err_t e = lora_init(&dev, &mock_spi, 433000000UL);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_TRUE(mock_reset_count > 0);
    ASSERT_TRUE(dev.spi == &mock_spi);
    ASSERT_EQ(dev.frequency, 433000000UL);
}

TEST(test_init_version_check)
{
    mock_reset_all();
    /* Set wrong version */
    mock_regs[0x42] = 0xAA;

    lora_t dev;
    memset(&dev, 0, sizeof(dev));

    lora_err_t e = lora_init(&dev, &mock_spi, 433000000UL);
    ASSERT_EQ(e, LORA_ERR_VERSION);
}

TEST(test_init_null_dev)
{
    mock_reset_all();
    ASSERT_EQ(lora_init(NULL, &mock_spi, 433000000UL), LORA_ERR_SPI);
}

TEST(test_init_null_spi)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(lora_init(&dev, NULL, 433000000UL), LORA_ERR_SPI);
}

TEST(test_init_null_callbacks)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));

    /* SPI struct with missing reset callback */
    lora_spi_t bad_spi = {
        .cs_low   = mock_cs_low,
        .cs_high  = mock_cs_high,
        .xfer     = mock_xfer,
        .reset    = NULL,
        .delay_ms = mock_delay,
        .ctx      = NULL,
    };
    ASSERT_EQ(lora_init(&dev, &bad_spi, 433000000UL), LORA_ERR_SPI);
}

TEST(test_set_frequency_433)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* Frf = 433e6 * 2^19 / 32e6 = 7094272 = 0x6C4000 (approx) */
    /* Verify the three Frf registers were written */
    uint32_t frf = ((uint32_t)mock_regs[0x06] << 16) |
                   ((uint32_t)mock_regs[0x07] <<  8) |
                   ((uint32_t)mock_regs[0x08]);
    /* Expected: 433000000 * 524288 / 32000000 = 7094272 = 0x6C4000 */
    ASSERT_EQ(frf, 0x6C4000UL);
}

TEST(test_set_frequency_915)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* Now change to 915 MHz */
    lora_err_t e = lora_set_frequency(&dev, 915000000UL);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(dev.frequency, 915000000UL);

    uint32_t frf = ((uint32_t)mock_regs[0x06] << 16) |
                   ((uint32_t)mock_regs[0x07] <<  8) |
                   ((uint32_t)mock_regs[0x08]);
    /* 915e6 * 2^19 / 32e6 = 14991360 = 0xE4C000 */
    ASSERT_EQ(frf, 0xE4C000UL);
}

TEST(test_set_frequency_out_of_range)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    ASSERT_EQ(lora_set_frequency(&dev, 50000000UL), LORA_ERR_PARAM);
    ASSERT_EQ(lora_set_frequency(&dev, 1100000000UL), LORA_ERR_PARAM);
}

TEST(test_set_bandwidth)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* BW=7 (125kHz) → top nibble = 0x70 */
    lora_err_t e = lora_set_bandwidth(&dev, 7);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(dev.bandwidth, 7);
    ASSERT_EQ(mock_regs[0x1D] & 0xF0, 0x70);

    /* BW=10 out of range */
    ASSERT_EQ(lora_set_bandwidth(&dev, 10), LORA_ERR_PARAM);
}

TEST(test_set_spreading_factor)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    lora_err_t e = lora_set_spreading_factor(&dev, 12);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(dev.spreading_factor, 12);
    ASSERT_EQ(mock_regs[0x1E] & 0xF0, 0xC0);

    /* SF=5 out of range */
    ASSERT_EQ(lora_set_spreading_factor(&dev, 5), LORA_ERR_PARAM);
    /* SF=13 out of range */
    ASSERT_EQ(lora_set_spreading_factor(&dev, 13), LORA_ERR_PARAM);
}

TEST(test_set_coding_rate)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    lora_err_t e = lora_set_coding_rate(&dev, 4);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(dev.coding_rate, 4);
    ASSERT_EQ(mock_regs[0x1D] & 0x0E, 0x08);

    /* CR=0 out of range */
    ASSERT_EQ(lora_set_coding_rate(&dev, 0), LORA_ERR_PARAM);
    /* CR=5 out of range */
    ASSERT_EQ(lora_set_coding_rate(&dev, 5), LORA_ERR_PARAM);
}

TEST(test_set_tx_power)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* Normal power: 10 dBm */
    lora_err_t e = lora_set_tx_power(&dev, 10);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(dev.tx_power, 10);
    ASSERT_EQ(mock_regs[0x4D], 0x84); /* PA_DAC default */
    ASSERT_EQ(mock_regs[0x09], (uint8_t)(0x80 | 8)); /* 10-2=8 */

    /* High power: 20 dBm */
    e = lora_set_tx_power(&dev, 20);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(mock_regs[0x4D], 0x87); /* PA_DAC high power */
    ASSERT_EQ(mock_regs[0x09], (uint8_t)(0x80 | 15)); /* 20-5=15 */

    /* Out of range */
    ASSERT_EQ(lora_set_tx_power(&dev, 1), LORA_ERR_PARAM);
    ASSERT_EQ(lora_set_tx_power(&dev, 21), LORA_ERR_PARAM);
}

TEST(test_send_fifo_write)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* Simulate hardware: TxDone fires after first poll delay */
    mock_delay_count = 0;
    mock_irq_after_delays = 1;
    mock_irq_value = 0x08; /* IRQ_TX_DONE */

    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    lora_err_t e = lora_send(&dev, payload, 4);
    ASSERT_EQ(e, LORA_OK);

    /* Verify FIFO was written with our payload */
    ASSERT_EQ(fifo_written_len, 4);
    ASSERT_EQ(fifo_written[0], 0xDE);
    ASSERT_EQ(fifo_written[1], 0xAD);
    ASSERT_EQ(fifo_written[2], 0xBE);
    ASSERT_EQ(fifo_written[3], 0xEF);

    /* Verify payload length register */
    ASSERT_EQ(mock_regs[0x22], 4);
}

TEST(test_send_null_data)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    ASSERT_EQ(lora_send(&dev, NULL, 5), LORA_ERR_PARAM);
}

TEST(test_send_timeout)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* IRQ flags stay 0 → never TxDone */
    mock_regs[0x12] = 0x00;

    uint8_t payload[] = {0x01};
    lora_err_t e = lora_send(&dev, payload, 1);
    ASSERT_EQ(e, LORA_ERR_TIMEOUT);
}

TEST(test_receive_ok)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* Pre-load FIFO read data */
    fifo_read_data[0] = 0xCA;
    fifo_read_data[1] = 0xFE;
    fifo_read_data[2] = 0xBA;
    fifo_read_len = 3;

    /* Pre-set payload size and RX address (not cleared by IRQ write) */
    mock_regs[0x13] = 3;    /* RX_NB_BYTES */
    mock_regs[0x10] = 0;    /* FIFO_RX_CURRENT */

    /* Simulate hardware: RxDone fires after first poll delay */
    mock_delay_count = 0;
    mock_irq_after_delays = 1;
    mock_irq_value = 0x40; /* IRQ_RX_DONE only (no CRC error) */

    uint8_t buf[16];
    uint8_t len = 0;
    lora_err_t e = lora_receive(&dev, buf, &len, 16);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(len, 3);
    ASSERT_EQ(buf[0], 0xCA);
    ASSERT_EQ(buf[1], 0xFE);
    ASSERT_EQ(buf[2], 0xBA);
}

TEST(test_receive_crc_error)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* Simulate hardware: RxDone + CRC error after first poll delay */
    mock_delay_count = 0;
    mock_irq_after_delays = 1;
    mock_irq_value = 0x40 | 0x20; /* IRQ_RX_DONE + PAYLOAD_CRC_ERR */

    uint8_t buf[16];
    uint8_t len = 0;
    lora_err_t e = lora_receive(&dev, buf, &len, 16);
    ASSERT_EQ(e, LORA_ERR_CRC);
}

TEST(test_packet_available)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    mock_regs[0x12] = 0x00;
    ASSERT_FALSE(lora_packet_available(&dev));

    mock_regs[0x12] = 0x40; /* RxDone */
    ASSERT_TRUE(lora_packet_available(&dev));
}

TEST(test_rssi_snr)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 915000000UL);

    /* PKT_RSSI = 100 → RSSI = -157 + 100 = -57 dBm (HF port) */
    mock_regs[0x1A] = 100;
    ASSERT_EQ(lora_get_rssi(&dev), -57);

    /* Change to LF frequency and re-check offset */
    dev.frequency = 433000000UL;
    ASSERT_EQ(lora_get_rssi(&dev), -64); /* -164 + 100 */

    /* PKT_SNR = 40 (signed) → 40/4 = 10 dB */
    mock_regs[0x19] = 40;
    ASSERT_EQ(lora_get_snr(&dev), 10);

    /* Negative SNR: 0xE0 = -32 as int8_t → -32/4 = -8 dB */
    mock_regs[0x19] = 0xE0;
    ASSERT_EQ(lora_get_snr(&dev), -8);

    /* Negative SNR: 0xFC = -4 as int8_t → -4/4 = -1 dB */
    mock_regs[0x19] = 0xFC;
    ASSERT_EQ(lora_get_snr(&dev), -1);
}

TEST(test_receive_timeout)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* IRQ flags stay 0 → RxDone never fires */
    mock_regs[0x12] = 0x00;

    uint8_t buf[16];
    uint8_t len = 0;
    lora_err_t e = lora_receive(&dev, buf, &len, 16);
    ASSERT_EQ(e, LORA_ERR_TIMEOUT);
}

TEST(test_send_zero_len)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    uint8_t payload[] = {0x01};
    ASSERT_EQ(lora_send(&dev, payload, 0), LORA_ERR_PARAM);
}

TEST(test_set_tx_power_high_power_boundary)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* Boundary: 18 dBm is > 17 → high power path */
    lora_err_t e = lora_set_tx_power(&dev, 18);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(mock_regs[0x4D], 0x87); /* PA_DAC high power */
    ASSERT_EQ(mock_regs[0x09], (uint8_t)(0x80 | 13)); /* 18-5=13 */

    /* Boundary: 17 dBm is the highest normal power path value */
    e = lora_set_tx_power(&dev, 17);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(mock_regs[0x4D], 0x84); /* PA_DAC default */
    ASSERT_EQ(mock_regs[0x09], (uint8_t)(0x80 | 15)); /* 17-2=15 */
}

TEST(test_receive_truncates_to_max_len)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* Inject 8 bytes but only accept 4 (max_len=4) */
    fifo_read_data[0] = 0x11;
    fifo_read_data[1] = 0x22;
    fifo_read_data[2] = 0x33;
    fifo_read_data[3] = 0x44;
    fifo_read_data[4] = 0x55;
    fifo_read_data[5] = 0x66;
    fifo_read_data[6] = 0x77;
    fifo_read_data[7] = 0x88;
    fifo_read_len = 8;

    mock_regs[0x13] = 8;   /* RX_NB_BYTES = 8 */
    mock_regs[0x10] = 0;   /* FIFO_RX_CURRENT */

    mock_delay_count = 0;
    mock_irq_after_delays = 1;
    mock_irq_value = 0x40; /* IRQ_RX_DONE */

    uint8_t buf[16];
    uint8_t len = 0;
    lora_err_t e = lora_receive(&dev, buf, &len, 4);
    ASSERT_EQ(e, LORA_OK);
    ASSERT_EQ(len, 4);  /* truncated to max_len */
    ASSERT_EQ(buf[0], 0x11);
    ASSERT_EQ(buf[3], 0x44);
}

TEST(test_rssi_hf_boundary)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    /* 868 MHz exactly is the HF boundary */
    lora_init(&dev, &mock_spi, 868000000UL);

    mock_regs[0x1A] = 157;
    /* HF: -157 + 157 = 0 dBm */
    ASSERT_EQ(lora_get_rssi(&dev), 0);
}

TEST(test_snr_truncation)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* SNR raw = 3 → 3/4 = 0 dB (truncates toward zero) */
    mock_regs[0x19] = 3;
    ASSERT_EQ(lora_get_snr(&dev), 0);

    /* SNR raw = -3 (0xFD) → -3/4 = 0 dB (truncates toward zero) */
    mock_regs[0x19] = 0xFD;
    ASSERT_EQ(lora_get_snr(&dev), 0);

    /* SNR raw = -12 (0xF4) → -12/4 = -3 dB */
    mock_regs[0x19] = 0xF4;
    ASSERT_EQ(lora_get_snr(&dev), -3);
}

TEST(test_set_bandwidth_all_valid)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* All valid bandwidth values 0-9 */
    uint8_t bw;
    for (bw = 0; bw <= 9; bw++) {
        lora_err_t e = lora_set_bandwidth(&dev, bw);
        ASSERT_EQ(e, LORA_OK);
        ASSERT_EQ(dev.bandwidth, bw);
        ASSERT_EQ(mock_regs[0x1D] & 0xF0, (uint8_t)(bw << 4));
    }
}

TEST(test_set_spreading_factor_all_valid)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* All valid SF values 6-12 */
    uint8_t sf;
    for (sf = 6; sf <= 12; sf++) {
        lora_err_t e = lora_set_spreading_factor(&dev, sf);
        ASSERT_EQ(e, LORA_OK);
        ASSERT_EQ(dev.spreading_factor, sf);
        ASSERT_EQ(mock_regs[0x1E] & 0xF0, (uint8_t)(sf << 4));
    }
}

TEST(test_set_coding_rate_all_valid)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    /* All valid CR values 1-4 */
    uint8_t cr;
    for (cr = 1; cr <= 4; cr++) {
        lora_err_t e = lora_set_coding_rate(&dev, cr);
        ASSERT_EQ(e, LORA_OK);
        ASSERT_EQ(dev.coding_rate, cr);
        ASSERT_EQ(mock_regs[0x1D] & 0x0E, (uint8_t)(cr << 1));
    }
}

TEST(test_sleep_standby)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    lora_init(&dev, &mock_spi, 433000000UL);

    lora_err_t e = lora_sleep(&dev);
    ASSERT_EQ(e, LORA_OK);
    /* OpMode should be LoRa + Sleep (0x80 | 0x00) */
    ASSERT_EQ(mock_regs[0x01], 0x80);

    e = lora_standby(&dev);
    ASSERT_EQ(e, LORA_OK);
    /* OpMode should be LoRa + Standby (0x80 | 0x01) */
    ASSERT_EQ(mock_regs[0x01], 0x81);
}

TEST(test_init_really_enters_lora_mode)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(lora_init(&dev, &mock_spi, 433000000UL), LORA_OK);
    ASSERT_EQ(mock_regs[0x01] & 0x80, 0x80);
}

TEST(test_init_calibrates_at_the_frequency_in_use)
{
    mock_reset_all();
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(lora_init(&dev, &mock_spi, 868000000UL), LORA_OK);
    /* the calibration runs in FSK mode, before the switch to LoRa */
    ASSERT_EQ(mock_image_cal_seen, 1);
}

TEST(test_init_reports_a_stuck_calibration)
{
    mock_reset_all();
    mock_image_cal_stuck = 1;
    lora_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(lora_init(&dev, &mock_spi, 868000000UL), LORA_ERR_TIMEOUT);
}

TEST_MAIN("lora_sx1276")
    RUN(test_init_ok);
    RUN(test_init_version_check);
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_init_null_callbacks);
    RUN(test_set_frequency_433);
    RUN(test_set_frequency_915);
    RUN(test_set_frequency_out_of_range);
    RUN(test_set_bandwidth);
    RUN(test_set_spreading_factor);
    RUN(test_set_coding_rate);
    RUN(test_set_tx_power);
    RUN(test_send_fifo_write);
    RUN(test_send_null_data);
    RUN(test_send_timeout);
    RUN(test_receive_ok);
    RUN(test_receive_crc_error);
    RUN(test_receive_timeout);
    RUN(test_packet_available);
    RUN(test_rssi_snr);
    RUN(test_sleep_standby);
    RUN(test_send_zero_len);
    RUN(test_set_tx_power_high_power_boundary);
    RUN(test_receive_truncates_to_max_len);
    RUN(test_rssi_hf_boundary);
    RUN(test_snr_truncation);
    RUN(test_set_bandwidth_all_valid);
    RUN(test_set_spreading_factor_all_valid);
    RUN(test_set_coding_rate_all_valid);
    RUN(test_init_really_enters_lora_mode);
    RUN(test_init_calibrates_at_the_frequency_in_use);
    RUN(test_init_reports_a_stuck_calibration);
TEST_MAIN_END()
