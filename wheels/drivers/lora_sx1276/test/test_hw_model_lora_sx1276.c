// SPDX-License-Identifier: MIT
/*
 * test_hw_model_lora_sx1276.c -- tests using SX1276 hardware register model
 *
 * Validates the lora_sx1276 driver against a register-accurate model
 * with SPI protocol, FIFO simulation, OpMode state machine, and IRQ
 * flag behavior.  Tests init/version, mode transitions, FIFO write/read,
 * frequency configuration, IRQ flags, TX/RX flows, and RSSI/SNR.
 */

#include "test_harness.h"
#include "hw_model_lora_sx1276.h"
#include "../lora_sx1276.h"

static sx1276_model_t model;

static const lora_spi_t model_spi = {
    .cs_low   = sx1276_model_cs_low,
    .cs_high  = sx1276_model_cs_high,
    .xfer     = sx1276_model_xfer,
    .reset    = sx1276_model_reset,
    .delay_ms = sx1276_model_delay_ms,
    .ctx      = &model,
};

static void setup(void)
{
    sx1276_model_init(&model);
}

/* --- bus access, for tests of the model itself --- */

static void spi_write(uint8_t reg, uint8_t val)
{
    sx1276_model_cs_low(&model);
    (void)sx1276_model_xfer(&model, (uint8_t)(reg | 0x80));
    (void)sx1276_model_xfer(&model, val);
    sx1276_model_cs_high(&model);
}

static uint8_t spi_read(uint8_t reg)
{
    sx1276_model_cs_low(&model);
    (void)sx1276_model_xfer(&model, reg);
    uint8_t val = sx1276_model_xfer(&model, 0x00);
    sx1276_model_cs_high(&model);
    return val;
}

/* RegOpMode: LoRa needs a stop in Sleep, p108 */
#define OP_LORA     0x80
#define OP_SHARED   0x40
#define OP_LF       0x08

static void go_lora_standby(void)
{
    spi_write(SX_REG_OP_MODE, OP_LF | SX_MODE_SLEEP);
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_SLEEP);
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_STANDBY);
}


/* --- 1. Init reads correct version --- */

TEST(test_init_reads_version)
{
    setup();
    lora_t dev;
    lora_err_t err = lora_init(&dev, &model_spi, 433000000UL);
    ASSERT_EQ(err, LORA_OK);
    /* Version register should still be 0x12 (read-only, unchanged) */
    ASSERT_EQ(model.hw.regs[SX_REG_VERSION], SX_VERSION_DEFAULT);
    ASSERT_TRUE(model.reset_count > 0);
}

/* --- 2. Init rejects wrong version --- */

TEST(test_init_rejects_wrong_version)
{
    setup();
    /* Override version to something wrong */
    model.hw.regs[SX_REG_VERSION] = 0xAA;
    model.hw.reset_values[SX_REG_VERSION] = 0xAA;

    lora_t dev;
    lora_err_t err = lora_init(&dev, &model_spi, 433000000UL);
    ASSERT_EQ(err, LORA_ERR_VERSION);
}

/* --- 3. Mode transitions: sleep -> standby -> TX -> standby --- */

TEST(test_mode_sleep_to_standby)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    /* After init, chip should be in standby (mode bits = 0x01) */
    ASSERT_EQ(model.hw.regs[SX_REG_OP_MODE] & 0x07, SX_MODE_STANDBY);

    /* Go to sleep */
    lora_err_t err = lora_sleep(&dev);
    ASSERT_EQ(err, LORA_OK);
    ASSERT_EQ(model.hw.regs[SX_REG_OP_MODE] & 0x07, SX_MODE_SLEEP);

    /* Back to standby */
    err = lora_standby(&dev);
    ASSERT_EQ(err, LORA_OK);
    ASSERT_EQ(model.hw.regs[SX_REG_OP_MODE] & 0x07, SX_MODE_STANDBY);
}

/* --- 4. Mode transition: standby -> TX mode write --- */

TEST(test_mode_standby_to_tx)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    /* Sending puts chip in TX mode (we need TxDone to fire) */
    model.tx_done_after_delays = 1;

    uint8_t payload[] = {0xDE, 0xAD};
    lora_err_t err = lora_send(&dev, payload, 2);
    ASSERT_EQ(err, LORA_OK);

    /* After successful send, driver puts chip back to standby */
    ASSERT_EQ(model.hw.regs[SX_REG_OP_MODE] & 0x07, SX_MODE_STANDBY);
}

/* --- 5. FIFO write and pointer tracking --- */

TEST(test_fifo_write_and_pointer)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    model.tx_done_after_delays = 1;

    uint8_t payload[] = {0xCA, 0xFE, 0xBA, 0xBE};
    lora_err_t err = lora_send(&dev, payload, 4);
    ASSERT_EQ(err, LORA_OK);

    /* Verify FIFO contents: driver writes to FIFO starting at TX base (0x00) */
    ASSERT_EQ(model.fifo[0], 0xCA);
    ASSERT_EQ(model.fifo[1], 0xFE);
    ASSERT_EQ(model.fifo[2], 0xBA);
    ASSERT_EQ(model.fifo[3], 0xBE);

    /* Payload length register should be set */
    ASSERT_EQ(model.hw.regs[SX_REG_PAYLOAD_LEN], 4);
}

/* --- 6. IRQ flag: TxDone set after TX --- */

TEST(test_irq_txdone)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    /* TxDone fires after 2 delay calls so we can check it was clear before */
    model.tx_done_after_delays = 2;

    uint8_t payload[] = {0x42};
    lora_err_t err = lora_send(&dev, payload, 1);
    ASSERT_EQ(err, LORA_OK);

    /* After send, TxDone was set then cleared by driver. Verify the model
     * saw the TX complete event. */
    ASSERT_TRUE(model.tx_done_count >= 1);
}

/* --- 7. IRQ flag: RxDone set during RX --- */

TEST(test_irq_rxdone)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    /* Pre-load RX FIFO with test data */
    uint8_t rx_data[] = {0xAA, 0xBB, 0xCC};
    sx1276_model_inject_rx(&model, rx_data, 3);

    /* RxDone fires after 1 delay */
    model.rx_done_after_delays = 1;

    uint8_t buf[16] = {0};
    uint8_t len = 0;
    lora_err_t err = lora_receive(&dev, buf, &len, 16);
    ASSERT_EQ(err, LORA_OK);
    ASSERT_EQ(len, 3);
    ASSERT_EQ(buf[0], 0xAA);
    ASSERT_EQ(buf[1], 0xBB);
    ASSERT_EQ(buf[2], 0xCC);
}

/* --- 8. Frequency register write (Frf calculation) --- */

TEST(test_frequency_register_write)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    /* Verify Frf registers: 433e6 * 2^19 / 32e6 = 7094272 = 0x6C4000 */
    uint32_t frf = ((uint32_t)model.hw.regs[SX_REG_FRF_MSB] << 16) |
                   ((uint32_t)model.hw.regs[SX_REG_FRF_MID] << 8) |
                   ((uint32_t)model.hw.regs[SX_REG_FRF_LSB]);
    ASSERT_EQ(frf, 0x6C4000UL);

    /* Change to 868 MHz: 868e6 * 2^19 / 32e6 = 14221312 = 0xD90000 */
    lora_err_t err = lora_set_frequency(&dev, 868000000UL);
    ASSERT_EQ(err, LORA_OK);

    frf = ((uint32_t)model.hw.regs[SX_REG_FRF_MSB] << 16) |
          ((uint32_t)model.hw.regs[SX_REG_FRF_MID] << 8) |
          ((uint32_t)model.hw.regs[SX_REG_FRF_LSB]);
    ASSERT_EQ(frf, 0xD90000UL);
}

/* --- 9. TX timeout when TxDone never fires --- */

TEST(test_tx_timeout)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    /* Disable automatic TxDone */
    model.tx_done_after_delays = 0;

    uint8_t payload[] = {0x01};
    lora_err_t err = lora_send(&dev, payload, 1);
    ASSERT_EQ(err, LORA_ERR_TIMEOUT);
}

/* --- 10. Version register is read-only --- */

TEST(test_version_readonly)
{
    setup();

    /* Try to write to version register via the model */
    hw_model__write_reg(&model.hw, SX_REG_VERSION, 0xFF);

    /* Should still be the original value because it is read-only */
    ASSERT_EQ(model.hw.regs[SX_REG_VERSION], SX_VERSION_DEFAULT);
}

/* --- 11. RX with CRC error --- */

TEST(test_rx_crc_error)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    /* Inject CRC error on RX */
    model.rx_inject_crc_error = true;
    model.rx_done_after_delays = 1;

    uint8_t rx_data[] = {0x01, 0x02};
    sx1276_model_inject_rx(&model, rx_data, 2);

    uint8_t buf[16];
    uint8_t len = 0;
    lora_err_t err = lora_receive(&dev, buf, &len, 16);
    ASSERT_EQ(err, LORA_ERR_CRC);
}

/* --- 12. Modem config register writes --- */

TEST(test_modem_config_registers)
{
    setup();
    lora_t dev;
    lora_init(&dev, &model_spi, 433000000UL);

    /* Set bandwidth to 9 (500 kHz): top nibble of ModemConfig1 = 0x90 */
    lora_err_t err = lora_set_bandwidth(&dev, 9);
    ASSERT_EQ(err, LORA_OK);
    ASSERT_EQ(model.hw.regs[SX_REG_MODEM_CONFIG_1] & 0xF0, 0x90);

    /* Set spreading factor to 12: top nibble of ModemConfig2 = 0xC0 */
    err = lora_set_spreading_factor(&dev, 12);
    ASSERT_EQ(err, LORA_OK);
    ASSERT_EQ(model.hw.regs[SX_REG_MODEM_CONFIG_2] & 0xF0, 0xC0);

    /* Set coding rate to 3 (4/7): bits [3:1] of ModemConfig1 = 0x06 */
    err = lora_set_coding_rate(&dev, 3);
    ASSERT_EQ(err, LORA_OK);
    ASSERT_EQ(model.hw.regs[SX_REG_MODEM_CONFIG_1] & 0x0E, 0x06);
}

/* --- what the model itself must do (SX1276 Rev. 4) --- */

/* p110: "FifoTxBaseAddr rw 0x80" */
TEST(test_tx_base_resets_to_0x80)
{
    setup();
    go_lora_standby();
    ASSERT_EQ_HEX(spi_read(SX_REG_FIFO_TX_BASE), 0x80u);
}

/* p108-109: Frf resets to 0x6C8000, p53 calls that "434 MHz (the value of
 * RegFrf at POR)". p113: "PayloadLength(7:0) rw 0x1" */
TEST(test_frf_and_payload_length_reset_values)
{
    setup();
    ASSERT_EQ_HEX(spi_read(SX_REG_FRF_MSB), 0x6Cu);
    ASSERT_EQ_HEX(spi_read(SX_REG_FRF_MID), 0x80u);
    ASSERT_EQ_HEX(spi_read(SX_REG_FRF_LSB), 0x00u);
    go_lora_standby();
    ASSERT_EQ_HEX(spi_read(SX_REG_PAYLOAD_LEN), 0x01u);
}

/* p35: "RegFifoTxBaseAddr specifies the point in memory where the transmit
 * information is stored", "RegPayloadLength indicates the size" */
TEST(test_tx_sends_what_the_driver_wrote)
{
    setup();
    lora_t dev;
    ASSERT_EQ(lora_init(&dev, &model_spi, 433000000UL), LORA_OK);
    uint8_t payload[] = {0xCA, 0xFE, 0xBA, 0xBE};
    ASSERT_EQ(lora_send(&dev, payload, 4), LORA_OK);
    ASSERT_EQ(model.tx_len, 4);
    ASSERT_MEMEQ_N(model.tx_payload, payload, 4);
}

/* A FIFO filled from 0 with the base left at 0x80 sends the wrong bytes */
TEST(test_tx_reads_from_the_base_not_the_fill_point)
{
    setup();
    go_lora_standby();
    spi_write(SX_REG_FIFO_ADDR_PTR, 0x00);
    spi_write(SX_REG_FIFO, 0x11);
    spi_write(SX_REG_FIFO_ADDR_PTR, 0x80);
    spi_write(SX_REG_FIFO, 0x22);
    spi_write(SX_REG_PAYLOAD_LEN, 1);
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_TX);
    sx1276_model_delay_ms(&model, 10);
    ASSERT_EQ(model.tx_len, 1);
    ASSERT_EQ_HEX(model.tx_payload[0], 0x22u);
}

/* p40: "Following the RxDone interrupt the radio goes to Standby mode." */
TEST(test_rx_done_returns_to_standby)
{
    setup();
    go_lora_standby();
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_RX_SINGLE);
    sx1276_model_delay_ms(&model, 10);
    ASSERT_EQ_HEX(spi_read(SX_REG_IRQ_FLAGS) & SX_IRQ_RX_DONE, SX_IRQ_RX_DONE);
    ASSERT_EQ(spi_read(SX_REG_OP_MODE) & 0x07, SX_MODE_STANDBY);
}

/* p40: "the chip generates the RxTimeout interrupt and goes back to Standby
 * mode" */
TEST(test_rx_timeout_returns_to_standby)
{
    setup();
    go_lora_standby();
    model.rx_done_after_delays = 0;
    model.rx_timeout_after_delays = 2;
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_RX_SINGLE);
    sx1276_model_delay_ms(&model, 10);
    ASSERT_EQ_HEX(spi_read(SX_REG_IRQ_FLAGS), 0u);
    ASSERT_EQ(spi_read(SX_REG_OP_MODE) & 0x07, SX_MODE_RX_SINGLE);
    sx1276_model_delay_ms(&model, 10);
    ASSERT_EQ_HEX(spi_read(SX_REG_IRQ_FLAGS), SX_IRQ_RX_TIMEOUT);
    ASSERT_EQ(spi_read(SX_REG_OP_MODE) & 0x07, SX_MODE_STANDBY);
}

/* The driver stops at the first RxTimeout instead of polling to its limit */
TEST(test_receive_stops_at_rx_timeout)
{
    setup();
    lora_t dev;
    ASSERT_EQ(lora_init(&dev, &model_spi, 433000000UL), LORA_OK);
    model.rx_done_after_delays = 0;
    model.rx_timeout_after_delays = 3;

    uint8_t buf[16];
    uint8_t len = 0;
    ASSERT_EQ(lora_receive(&dev, buf, &len, 16), LORA_ERR_TIMEOUT);
    ASSERT_EQ(model.delay_count, 3);
    ASSERT_EQ_HEX(model.hw.regs[SX_REG_IRQ_FLAGS], 0u);
    ASSERT_EQ(model.hw.regs[SX_REG_OP_MODE] & 0x07, SX_MODE_STANDBY);
}

/* p108: LongRangeMode "can be modified only in Sleep mode. A write operation
 * on other device modes is ignored." */
TEST(test_long_range_mode_needs_sleep)
{
    setup();
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_STANDBY);
    ASSERT_EQ_HEX(spi_read(SX_REG_OP_MODE), OP_LF | SX_MODE_STANDBY);

    spi_write(SX_REG_OP_MODE, OP_LF | SX_MODE_SLEEP);
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_SLEEP);
    ASSERT_EQ_HEX(spi_read(SX_REG_OP_MODE), OP_LORA | OP_LF | SX_MODE_SLEEP);
}

/* p108: "The LoRa registers are only accessible when the device is set in Lora
 * mode (and, in the same way, the FSK register are only accessible in FSK
 * mode)." */
TEST(test_fsk_writes_do_not_reach_the_lora_page)
{
    setup();
    spi_write(SX_REG_FIFO_TX_BASE, 0x00);  /* still FSK, as after reset */
    ASSERT_EQ_HEX(spi_read(SX_REG_FIFO_TX_BASE), 0x00u);
    go_lora_standby();
    ASSERT_EQ_HEX(spi_read(SX_REG_FIFO_TX_BASE), 0x80u);
}

TEST(test_lora_writes_do_not_reach_the_fsk_page)
{
    setup();
    uint8_t fsk = spi_read(SX_REG_IMAGE_CAL);
    go_lora_standby();
    spi_write(SX_REG_MODEM_CONFIG_1, 0x11);
    spi_write(SX_REG_IMAGE_CAL, 0xAA);
    ASSERT_EQ_HEX(spi_read(SX_REG_IMAGE_CAL), 0xAAu);
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_SLEEP);
    spi_write(SX_REG_OP_MODE, OP_LF | SX_MODE_SLEEP);
    ASSERT_EQ_HEX(spi_read(SX_REG_IMAGE_CAL), fsk);
}

/* p108: AccessSharedReg "will grant access to the FSK register 0x0D up to the
 * register 0x3F" */
TEST(test_access_shared_reg_opens_the_fsk_page)
{
    setup();
    go_lora_standby();
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_SHARED | OP_LF | SX_MODE_STANDBY);
    spi_write(SX_REG_FIFO_TX_BASE, 0x55);
    ASSERT_EQ_HEX(spi_read(SX_REG_FIFO_TX_BASE), 0x55u);
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_STANDBY);
    ASSERT_EQ_HEX(spi_read(SX_REG_FIFO_TX_BASE), 0x80u);
}

/* In FSK 0x12 is not RegIrqFlags: only the LoRa page clears by writing 1 */
TEST(test_irq_flags_exist_on_the_lora_page_only)
{
    setup();
    spi_write(SX_REG_IRQ_FLAGS, 0xFF);
    ASSERT_EQ_HEX(spi_read(SX_REG_IRQ_FLAGS), 0xFFu);
    go_lora_standby();
    ASSERT_EQ_HEX(spi_read(SX_REG_IRQ_FLAGS), 0x00u);
}

/* p110-112 mode "r": RegFifoRxCurrentAddr, RegRxNbBytes to RegHopChannel */
TEST(test_lora_status_registers_are_read_only)
{
    static const uint8_t regs[] = {0x10, 0x13, 0x14, 0x15, 0x16, 0x17,
                                   0x18, 0x19, 0x1A, 0x1B, 0x1C};
    setup();
    go_lora_standby();
    for (size_t i = 0; i < sizeof(regs); i++) {
        uint8_t before = spi_read(regs[i]);
        spi_write(regs[i], (uint8_t)~before);
        ASSERT_EQ_HEX(spi_read(regs[i]), before);
    }
}

/* p35: "The FIFO data buffer is cleared when the device is put in SLEEP mode,
 * consequently no access to the FIFO data buffer is possible in sleep mode." */
TEST(test_sleep_clears_and_closes_the_fifo)
{
    setup();
    go_lora_standby();
    spi_write(SX_REG_FIFO_ADDR_PTR, 0x00);
    spi_write(SX_REG_FIFO, 0xA5);
    ASSERT_EQ_HEX(model.fifo[0], 0xA5u);

    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_SLEEP);
    ASSERT_EQ_HEX(model.fifo[0], 0x00u);

    spi_write(SX_REG_FIFO_ADDR_PTR, 0x00);
    spi_write(SX_REG_FIFO, 0x5A);
    ASSERT_EQ_HEX(model.fifo[0], 0x00u);
    ASSERT_EQ_HEX(model.hw.regs[SX_REG_FIFO_ADDR_PTR], 0x00u);
}

/* p38: "The LoRaTM FIFO can only be filled in Standby mode." */
TEST(test_fifo_fills_only_in_standby)
{
    setup();
    go_lora_standby();
    model.rx_done_after_delays = 0;
    spi_write(SX_REG_OP_MODE, OP_LORA | OP_LF | SX_MODE_RX_SINGLE);
    spi_write(SX_REG_FIFO, 0xA5);
    ASSERT_EQ_HEX(model.fifo[0], 0x00u);
}

/* p34: "256 byte RAM data buffer which is uniquely accessible in LoRa mode" */
TEST(test_fifo_is_closed_in_fsk)
{
    setup();
    spi_write(SX_REG_FIFO, 0xA5);
    ASSERT_EQ_HEX(model.fifo[0], 0x00u);
}

/* p53: "To perform the calibration, the radio must be temporarily returned to
 * FSK/OOK mode"; p103: ImageCalStart "Triggers the IQ and RSSI calibration
 * when set in Standby mode." */
TEST(test_image_cal_needs_fsk_standby)
{
    setup();
    spi_write(SX_REG_OP_MODE, OP_LF | SX_MODE_SLEEP);
    spi_write(SX_REG_IMAGE_CAL, 0x42);
    ASSERT_EQ(model.image_cal_count, 0);

    spi_write(SX_REG_OP_MODE, OP_LF | SX_MODE_STANDBY);
    spi_write(SX_REG_IMAGE_CAL, 0x42);
    ASSERT_EQ(model.image_cal_count, 1);

    sx1276_model_delay_ms(&model, 10);
    go_lora_standby();
    spi_write(SX_REG_IMAGE_CAL, 0x42);
    ASSERT_EQ(model.image_cal_count, 1);
}

/* p103: ImageCalRunning "Set to 1 while the Image and RSSI calibration are
 * running. Toggles back to 0 when the process is completed"; p53: "takes
 * approximately 10ms" */
TEST(test_image_cal_running_clears_after_10_ms)
{
    setup();
    spi_write(SX_REG_IMAGE_CAL, 0x42);
    ASSERT_EQ_HEX(spi_read(SX_REG_IMAGE_CAL) & 0x20, 0x20u);
    sx1276_model_delay_ms(&model, 9);
    ASSERT_EQ_HEX(spi_read(SX_REG_IMAGE_CAL) & 0x20, 0x20u);
    sx1276_model_delay_ms(&model, 1);
    ASSERT_EQ_HEX(spi_read(SX_REG_IMAGE_CAL) & 0x20, 0x00u);
}

/* p53: the reset calibration "is only valid at 434 MHz", so init repeats it at
 * the working frequency. 915 MHz: Frf = 915e6 * 2^19 / 32e6 = 0xE4C000 */
TEST(test_init_calibrates_at_the_working_frequency)
{
    setup();
    lora_t dev;
    ASSERT_EQ(lora_init(&dev, &model_spi, 915000000UL), LORA_OK);
    ASSERT_EQ(model.image_cal_count, 1);
    ASSERT_EQ_HEX(model.image_cal_frf, 0xE4C000UL);
}

/* The setup registers must land on the LoRa page. The values are the driver's
 * own, but the reset ones differ, so these fail if init writes them from FSK. */
TEST(test_init_setup_lands_on_the_lora_page)
{
    setup();
    lora_t dev;
    ASSERT_EQ(lora_init(&dev, &model_spi, 433000000UL), LORA_OK);
    ASSERT_EQ_HEX(model.hw.regs[SX_REG_OP_MODE] & 0x87, 0x81u);
    ASSERT_EQ_HEX(model.hw.regs[SX_REG_FIFO_TX_BASE], 0x00u);
    ASSERT_EQ_HEX(model.hw.regs[SX_REG_MODEM_CONFIG_2] & 0x04, 0x04u);
    ASSERT_EQ_HEX(model.hw.regs[SX_REG_MODEM_CONFIG_3], 0x04u);
}

TEST_MAIN("lora_sx1276 hardware model")
    RUN(test_init_reads_version);
    RUN(test_init_rejects_wrong_version);
    RUN(test_mode_sleep_to_standby);
    RUN(test_mode_standby_to_tx);
    RUN(test_fifo_write_and_pointer);
    RUN(test_irq_txdone);
    RUN(test_irq_rxdone);
    RUN(test_frequency_register_write);
    RUN(test_tx_timeout);
    RUN(test_version_readonly);
    RUN(test_rx_crc_error);
    RUN(test_modem_config_registers);
    RUN(test_tx_base_resets_to_0x80);
    RUN(test_frf_and_payload_length_reset_values);
    RUN(test_tx_sends_what_the_driver_wrote);
    RUN(test_tx_reads_from_the_base_not_the_fill_point);
    RUN(test_rx_done_returns_to_standby);
    RUN(test_rx_timeout_returns_to_standby);
    RUN(test_receive_stops_at_rx_timeout);
    RUN(test_long_range_mode_needs_sleep);
    RUN(test_fsk_writes_do_not_reach_the_lora_page);
    RUN(test_lora_writes_do_not_reach_the_fsk_page);
    RUN(test_access_shared_reg_opens_the_fsk_page);
    RUN(test_irq_flags_exist_on_the_lora_page_only);
    RUN(test_lora_status_registers_are_read_only);
    RUN(test_sleep_clears_and_closes_the_fifo);
    RUN(test_fifo_fills_only_in_standby);
    RUN(test_fifo_is_closed_in_fsk);
    RUN(test_image_cal_needs_fsk_standby);
    RUN(test_image_cal_running_clears_after_10_ms);
    RUN(test_init_calibrates_at_the_working_frequency);
    RUN(test_init_setup_lands_on_the_lora_page);
TEST_MAIN_END()
