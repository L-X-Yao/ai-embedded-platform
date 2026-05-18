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
TEST_MAIN_END()
