// SPDX-License-Identifier: MIT
/*
 * test_hw_model_mcp2515.c -- tests using MCP2515 hardware register model
 *
 * Validates the mcp2515 driver against a register-accurate model with
 * SPI command protocol (READ/WRITE/BIT_MODIFY/RESET/READ_STATUS),
 * mode control state machine, TX/RX buffer operations, and bit timing
 * configuration.  Tests init, mode transitions, CNF registers, TX send,
 * TX completion, RX receive, filter/mask config, and mode protection.
 */

#include "test_harness.h"
#include "hw_model_mcp2515.h"
#include "../mcp2515.h"
#include <string.h>

static mcp2515_model_t model;

static const mcp2515_spi_t model_spi = {
    .cs_low   = mcp2515_model_cs_low,
    .cs_high  = mcp2515_model_cs_high,
    .xfer     = mcp2515_model_xfer,
    .ctx      = &model,
    .delay_us = mcp2515_model_delay_us,
};

static void setup(void)
{
    mcp2515_model_init(&model);
}

/* Reach the chip the way the bus does, without the driver */
static void spi_write(uint8_t addr, uint8_t val)
{
    mcp2515_model_cs_low(&model);
    mcp2515_model_xfer(&model, MCP_SPI_WRITE);
    mcp2515_model_xfer(&model, addr);
    mcp2515_model_xfer(&model, val);
    mcp2515_model_cs_high(&model);
}

static uint8_t spi_read(uint8_t addr)
{
    mcp2515_model_cs_low(&model);
    mcp2515_model_xfer(&model, MCP_SPI_READ);
    mcp2515_model_xfer(&model, addr);
    uint8_t val = mcp2515_model_xfer(&model, 0xFF);
    mcp2515_model_cs_high(&model);
    return val;
}

static void spi_bit_modify(uint8_t addr, uint8_t mask, uint8_t val)
{
    mcp2515_model_cs_low(&model);
    mcp2515_model_xfer(&model, MCP_SPI_BIT_MODIFY);
    mcp2515_model_xfer(&model, addr);
    mcp2515_model_xfer(&model, mask);
    mcp2515_model_xfer(&model, val);
    mcp2515_model_cs_high(&model);
}

/* --- 1. Init sets config mode then transitions to normal --- */

TEST(test_init_config_mode)
{
    setup();
    mcp2515_t dev;
    mcp2515_err_t err = mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);
    ASSERT_EQ(err, MCP2515_OK);
    /* After init, chip should be in normal mode */
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_NORMAL);
    /* Reset should have been called */
    ASSERT_TRUE(model.reset_count > 0);
}

/* DS20001801J p.55: the OST "keeps the device in a Reset state for 128 OSC1 clock
 * cycles after the occurrence of a Power-on Reset, SPI Reset", and "no SPI
 * protocol operations should be attempted until after the OST has expired". */
TEST(test_init_waits_out_the_ost_after_reset)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);
    ASSERT_EQ(model.reset_count, 1);
    ASSERT_EQ(model.spi_during_ost, 0);
    ASSERT_GE(model.delay_us_total, MCP_OST_US);
}

/* --- 2. Mode transition: normal -> loopback --- */

TEST(test_mode_to_loopback)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    mcp2515_err_t err = mcp2515_set_mode(&dev, MCP2515_MODE_LOOPBACK);
    ASSERT_EQ(err, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_LOOPBACK);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_LOOPBACK);
}

/* --- 3. Mode transition: normal -> config -> normal --- */

TEST(test_mode_roundtrip)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_250KBPS);

    /* To config */
    mcp2515_err_t err = mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);
    ASSERT_EQ(err, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_CONFIG);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_CONFIG);

    /* Back to normal */
    err = mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL);
    ASSERT_EQ(err, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_NORMAL);
}

/* --- 4. Bit timing config: CNF registers for each speed --- */

TEST(test_cnf_125kbps)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_125KBPS);

    ASSERT_EQ(model.hw.regs[MCP_REG_CNF1], 0x03);
    ASSERT_EQ(model.hw.regs[MCP_REG_CNF2], 0x90);
    ASSERT_EQ(model.hw.regs[MCP_REG_CNF3], 0x02);
}

TEST(test_cnf_500kbps)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    ASSERT_EQ(model.hw.regs[MCP_REG_CNF1], 0x00);
    ASSERT_EQ(model.hw.regs[MCP_REG_CNF2], 0x90);
    ASSERT_EQ(model.hw.regs[MCP_REG_CNF3], 0x02);
}

TEST(test_cnf_1mbps)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_1MBPS), MCP2515_ERR_PARAM);
}

/* --- 5. TX buffer load and send --- */

TEST(test_tx_buffer_load_and_send)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg = {
        .id = 0x123,
        .dlc = 3,
        .data = {0xAA, 0xBB, 0xCC, 0, 0, 0, 0, 0},
        .extended = false,
        .rtr = false,
    };
    mcp2515_err_t err = mcp2515_send(&dev, &msg);
    ASSERT_EQ(err, MCP2515_OK);

    /* Verify TXB0 registers: ID=0x123 */
    /* SIDH = 0x123 >> 3 = 0x24 */
    ASSERT_EQ(model.hw.regs[MCP_REG_TXB0SIDH], 0x24);
    /* SIDL = (0x123 << 5) & 0xE0 = 0x60 */
    ASSERT_EQ(model.hw.regs[MCP_REG_TXB0SIDL], 0x60);
    /* DLC */
    ASSERT_EQ(model.hw.regs[MCP_REG_TXB0DLC], 3);
    /* Data bytes */
    ASSERT_EQ(model.hw.regs[MCP_REG_TXB0D0], 0xAA);
    ASSERT_EQ(model.hw.regs[(uint8_t)(MCP_REG_TXB0D0 + 1)], 0xBB);
    ASSERT_EQ(model.hw.regs[(uint8_t)(MCP_REG_TXB0D0 + 2)], 0xCC);
    /* TXREQ should be set */
    ASSERT_TRUE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);
}

/* --- 6. TX complete: TXREQ clears --- */

TEST(test_txreq_clears_on_completion)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg = {
        .id = 0x100,
        .dlc = 1,
        .data = {0xFF, 0, 0, 0, 0, 0, 0, 0},
        .extended = false,
        .rtr = false,
    };
    mcp2515_err_t err = mcp2515_send(&dev, &msg);
    ASSERT_EQ(err, MCP2515_OK);

    /* TXREQ is set after send */
    ASSERT_TRUE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);

    /* p.15: "the TXREQ bit will be cleared, the TXnIF bit (CANINTF) will be set" */
    mcp2515_model_finish_tx(&model);
    ASSERT_FALSE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);
    ASSERT_TRUE(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_TX0IF);

    /* TX complete counter should have been incremented by the model */
    ASSERT_TRUE(model.tx_complete_count >= 1);
}

/* --- 7. TX busy: second send fails when TXREQ still set --- */

TEST(test_tx_busy_rejected)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    /* Manually set TXREQ to simulate a pending transmission */
    model.hw.regs[MCP_REG_TXB0CTRL] = MCP_TXB_TXREQ;

    mcp2515_msg_t msg = {
        .id = 0x200,
        .dlc = 1,
        .data = {0x42, 0, 0, 0, 0, 0, 0, 0},
        .extended = false,
        .rtr = false,
    };
    mcp2515_err_t err = mcp2515_send(&dev, &msg);
    ASSERT_EQ(err, MCP2515_ERR_TX_BUSY);
}

/* --- 8. RX: receive standard frame --- */

TEST(test_rx_standard_frame)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    mcp2515_model_inject_rx_std(&model, 0x7FF, data, 4);

    ASSERT_TRUE(mcp2515_has_message(&dev));

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    mcp2515_err_t err = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(err, MCP2515_OK);
    ASSERT_EQ(msg.id, 0x7FFU);
    ASSERT_EQ(msg.dlc, 4);
    ASSERT_FALSE(msg.extended);
    ASSERT_EQ(msg.data[0], 0x11);
    ASSERT_EQ(msg.data[3], 0x44);

    /* RX0IF should be cleared after receive */
    ASSERT_FALSE(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_RX0IF);
}

/* --- 9. RX: receive extended frame --- */

TEST(test_rx_extended_frame)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    uint8_t data[] = {0xCA, 0xFE};
    mcp2515_model_inject_rx_ext(&model, 0x12345, data, 2);

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    mcp2515_err_t err = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(err, MCP2515_OK);
    ASSERT_TRUE(msg.extended);
    ASSERT_EQ(msg.id, 0x12345U);
    ASSERT_EQ(msg.dlc, 2);
    ASSERT_EQ(msg.data[0], 0xCA);
    ASSERT_EQ(msg.data[1], 0xFE);
}

/* --- 10. Filter config requires config mode --- */

TEST(test_filter_requires_config_mode)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    /* In normal mode: filter set should fail */
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);
    mcp2515_err_t err = mcp2515_set_filter(&dev, 0, 0x123, false);
    ASSERT_EQ(err, MCP2515_ERR_PARAM);

    /* Switch to config mode: filter set should succeed */
    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);
    err = mcp2515_set_filter(&dev, 0, 0x123, false);
    ASSERT_EQ(err, MCP2515_OK);
}

/* --- 11. Mask config requires config mode --- */

TEST(test_mask_requires_config_mode)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    /* In normal mode: mask set should fail */
    mcp2515_err_t err = mcp2515_set_mask(&dev, 0, 0x7FF, false);
    ASSERT_EQ(err, MCP2515_ERR_PARAM);

    /* Switch to config mode: mask set should succeed */
    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);
    err = mcp2515_set_mask(&dev, 0, 0x7FF, false);
    ASSERT_EQ(err, MCP2515_OK);

    /* Verify mask register: 0x7FF >> 3 = 0xFF */
    ASSERT_EQ(model.hw.regs[MCP_REG_RXM0SIDH], 0xFF);
}

/* --- 12. No message returns ERR_NO_MSG --- */

TEST(test_no_message_pending)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    ASSERT_FALSE(mcp2515_has_message(&dev));

    mcp2515_msg_t msg;
    mcp2515_err_t err = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(err, MCP2515_ERR_NO_MSG);
}

/* --- 13. read_status returns the pre-set status byte --- */

TEST(test_read_status_byte)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    model.status_byte = 0x55;
    uint8_t st = mcp2515_read_status(&dev);
    ASSERT_EQ(st, 0x55);
}

/* --- 14. read_errors returns EFLG register --- */

TEST(test_read_errors_register)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    model.hw.regs[MCP_REG_EFLG] = 0xA3;
    uint8_t ef = mcp2515_read_errors(&dev);
    ASSERT_EQ(ef, 0xA3);
}

/* --- 15. Extended TX frame encodes all four ID registers --- */

TEST(test_tx_extended_frame)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg = {
        .id = 0x12345,
        .dlc = 2,
        .data = {0xDE, 0xAD, 0, 0, 0, 0, 0, 0},
        .extended = true,
        .rtr = false,
    };
    mcp2515_err_t err = mcp2515_send(&dev, &msg);
    ASSERT_EQ(err, MCP2515_OK);

    /* SIDL must have EXIDE bit (0x08) set */
    ASSERT_TRUE(model.hw.regs[MCP_REG_TXB0SIDL] & 0x08);
    /* DLC */
    ASSERT_EQ(model.hw.regs[MCP_REG_TXB0DLC], 2);
    /* Data */
    ASSERT_EQ(model.hw.regs[MCP_REG_TXB0D0], 0xDE);
    ASSERT_EQ(model.hw.regs[(uint8_t)(MCP_REG_TXB0D0 + 1)], 0xAD);
}

/* --- 16. Mode transitions: sleep and listen --- */

TEST(test_mode_to_sleep)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    mcp2515_err_t err = mcp2515_set_mode(&dev, MCP2515_MODE_SLEEP);
    ASSERT_EQ(err, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_SLEEP);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_SLEEP);
}

TEST(test_mode_to_listen)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    mcp2515_err_t err = mcp2515_set_mode(&dev, MCP2515_MODE_LISTEN);
    ASSERT_EQ(err, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_LISTEN);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_LISTEN);
}

/* The model against the manual, without the driver's wake code: p.59 says the
 * oscillator stops in Sleep and only WAKIF with WAKIE set wakes the chip. */
TEST(test_model_sleep_needs_wakif_and_wakie)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_SLEEP), MCP2515_OK);

    /* A mode request alone does nothing */
    hw_model__write_reg(&model.hw, MCP_REG_CANCTRL, MCP_OPMOD_NORMAL);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_SLEEP);

    /* WAKIF without WAKIE does nothing either */
    hw_model__write_reg(&model.hw, MCP_REG_CANINTF, MCP_CANINTF_WAKIF);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_SLEEP);
    hw_model__write_reg(&model.hw, MCP_REG_CANINTF, 0x00);

    /* Both wake it, in Listen-Only mode, and the OST runs again */
    hw_model__write_reg(&model.hw, MCP_REG_CANINTE, MCP_CANINTE_WAKIE);
    hw_model__write_reg(&model.hw, MCP_REG_CANINTF, MCP_CANINTF_WAKIF);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
              MCP_OPMOD_LISTEN);
    ASSERT_EQ(model.ost_left_us, MCP_OST_US);
}

/* Every mode can be reached from Sleep, with no SPI operation inside the OST
 * and no wake-up interrupt left behind */
TEST(test_leave_sleep_to_every_mode)
{
    static const struct { mcp2515_mode_t mode; uint8_t opmod; } to[] = {
        { MCP2515_MODE_NORMAL,   MCP_OPMOD_NORMAL   },
        { MCP2515_MODE_LOOPBACK, MCP_OPMOD_LOOPBACK },
        { MCP2515_MODE_LISTEN,   MCP_OPMOD_LISTEN   },
        { MCP2515_MODE_CONFIG,   MCP_OPMOD_CONFIG   },
    };

    for (size_t i = 0; i < sizeof(to) / sizeof(to[0]); i++) {
        setup();
        mcp2515_t dev;
        mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);
        ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_SLEEP), MCP2515_OK);

        ASSERT_EQ(mcp2515_set_mode(&dev, to[i].mode), MCP2515_OK);
        ASSERT_EQ(dev.mode, to[i].mode);
        ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK,
                  to[i].opmod);
        ASSERT_EQ(model.spi_during_ost, 0);
        ASSERT_EQ(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_WAKIF, 0);
        ASSERT_EQ(model.hw.regs[MCP_REG_CANINTE] & MCP_CANINTE_WAKIE, 0);
    }
}

/* Sleep and back, more than once */
TEST(test_sleep_cycles)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_SLEEP), MCP2515_OK);
        ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL), MCP2515_OK);
    }
    ASSERT_EQ(model.spi_during_ost, 0);
}

/* A chip that stays asleep is a timeout, not a success, and leaves no flag set,
 * so that the next attempt can set WAKIF again */
TEST(test_leave_sleep_chip_stays_asleep)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_SLEEP), MCP2515_OK);

    hw_model_set_on_write(&model.hw, MCP_REG_CANINTF, NULL);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL), MCP2515_ERR_TIMEOUT);
    ASSERT_EQ(dev.mode, MCP2515_MODE_SLEEP);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_WAKIF, 0);

    hw_model_set_on_write(&model.hw, MCP_REG_CANINTF, mcp2515_on_canintf_write);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL), MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);
}

/* --- 17. Init with invalid speed returns ERR_PARAM --- */

TEST(test_init_invalid_speed_hw)
{
    setup();
    mcp2515_t dev;
    mcp2515_err_t err = mcp2515_init(&dev, &model_spi, (mcp2515_speed_t)99);
    ASSERT_EQ(err, MCP2515_ERR_PARAM);
}

/* --- 18. Receive from RXB1 --- */

TEST(test_rx_from_rxb1)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    uint8_t data[] = {0x11, 0x22};
    mcp2515_model_inject_rx1_std(&model, 0x3FF, data, 2);

    ASSERT_TRUE(mcp2515_has_message(&dev));

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    mcp2515_err_t err = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(err, MCP2515_OK);
    ASSERT_EQ(msg.id, 0x3FFU);
    ASSERT_EQ(msg.dlc, 2);
    ASSERT_FALSE(msg.extended);
    ASSERT_EQ(msg.data[0], 0x11);
    ASSERT_EQ(msg.data[1], 0x22);

    /* RX1IF should be cleared */
    ASSERT_FALSE(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_RX1IF);
}

/* --- 19. Extended filter encoding via set_filter --- */

TEST(test_filter_extended_encoding)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);
    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);

    mcp2515_err_t err = mcp2515_set_filter(&dev, 2, 0x12345, true);
    ASSERT_EQ(err, MCP2515_OK);
    /* Filter 2 base = 0x08; SIDL (0x09) must have EXIDE bit */
    ASSERT_TRUE(model.hw.regs[0x09] & 0x08);
}

/* --- 20. Extended mask encoding via set_mask --- */

TEST(test_mask_extended_encoding)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);
    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);

    mcp2515_err_t err = mcp2515_set_mask(&dev, 0, 0x1FFFFFFF, true);
    ASSERT_EQ(err, MCP2515_OK);
    /* RXM0 at 0x20. p.37: RXMnSIDL has no EXIDE, bits 4:2 are U-0 */
    ASSERT_EQ_HEX(model.hw.regs[0x20], 0xFF);
    ASSERT_EQ_HEX(model.hw.regs[0x21], 0xE3);
    ASSERT_EQ_HEX(model.hw.regs[0x22], 0xFF);
    ASSERT_EQ_HEX(model.hw.regs[0x23], 0xFF);
}

/* --- 21. RTR frame TX sets DLC bit 6 --- */

TEST(test_tx_rtr_frame)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg = {
        .id = 0x100,
        .dlc = 0,
        .data = {0, 0, 0, 0, 0, 0, 0, 0},
        .extended = false,
        .rtr = true,
    };
    mcp2515_err_t err = mcp2515_send(&dev, &msg);
    ASSERT_EQ(err, MCP2515_OK);
    /* DLC register bit 6 = RTR */
    ASSERT_TRUE(model.hw.regs[MCP_REG_TXB0DLC] & 0x40);
}

/* --- 22. 250 kbps CNF register values --- */

TEST(test_cnf_250kbps)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_250KBPS);

    ASSERT_EQ(model.hw.regs[MCP_REG_CNF1], 0x01);
    ASSERT_EQ(model.hw.regs[MCP_REG_CNF2], 0x90);
    ASSERT_EQ(model.hw.regs[MCP_REG_CNF3], 0x02);
}

TEST(test_send_succeeds_when_the_frame_goes_out_at_once)
{
    setup();
    mcp2515_t dev;
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);
    model.auto_clear_txreq = true;

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 1, .data = {0xAA}, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    ASSERT_FALSE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);
    ASSERT_TRUE(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_TX0IF);
}

static void next_frame_arrives(void *user, uint8_t reg, uint8_t old_val, uint8_t new_val)
{
    (void)old_val;
    (void)new_val;
    mcp2515_model_t *m = (mcp2515_model_t *)user;
    m->hw.regs[reg] |= MCP_CANINTF_RX0IF;
}

TEST(test_receive_succeeds_when_the_next_frame_is_already_in)
{
    setup();
    mcp2515_t dev;
    uint8_t data[1] = {0x5A};
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS);
    mcp2515_model_inject_rx_std(&model, 0x100, data, 1);
    hw_model_set_on_write(&model.hw, MCP_REG_CANINTF, next_frame_arrives);

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    ASSERT_EQ(mcp2515_receive(&dev, &msg), MCP2515_OK);
    ASSERT_EQ(msg.data[0], 0x5A);
}

/* DS20001801J p.59: "When changing modes, the mode will not actually change until
 * all pending message transmissions are complete. The requested mode must be
 * verified by reading the OPMODE[2:0] bits". */
TEST(test_mode_change_waits_for_a_pending_transmission)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 1, .data = {0xAA}, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);

    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG), MCP2515_ERR_TIMEOUT);
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK, MCP_OPMOD_NORMAL);

    /* The request is still in REQOP, so the chip changes once the frame is out */
    mcp2515_model_finish_tx(&model);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK, MCP_OPMOD_CONFIG);

    /* and asking again brings the driver back in step */
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG), MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_CONFIG);
}

/* p.16: "The MCU can request to abort a message in a specific message buffer by
 * clearing the associated TXREQ bit". Nothing is pending any more after that. */
TEST(test_model_abort_by_clearing_txreq_ends_the_wait)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 0, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    spi_write(MCP_REG_CANCTRL, MCP_OPMOD_CONFIG);
    ASSERT_EQ(spi_read(MCP_REG_CANSTAT) & MCP_CANSTAT_OPMOD_MASK, MCP_OPMOD_NORMAL);

    spi_bit_modify(MCP_REG_TXB0CTRL, MCP_TXB_TXREQ, 0x00);
    ASSERT_EQ(spi_read(MCP_REG_CANSTAT) & MCP_CANSTAT_OPMOD_MASK, MCP_OPMOD_CONFIG);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_TX0IF, 0);
}

/* p.16: "The MCU can request to abort a message in a specific message buffer by
 * clearing the associated TXREQ bit". mcp2515_send uses buffer 0. */
TEST(test_abort_takes_back_a_pending_frame)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 1, .data = {0xAA}, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_ERR_TX_BUSY);

    ASSERT_EQ(mcp2515_abort_tx(&dev), MCP2515_OK);
    ASSERT_EQ(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ, 0);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_TX0IF, 0);

    /* the buffer is the caller's again */
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
}

/* p.16: unlike ABAT, which "MUST be reset", TXREQ leaves nothing to undo, and
 * "Aborting a message by resetting the TXREQ bit does NOT cause the ABTF bit to be
 * set". The priority bits of the buffer stay as they are. */
TEST(test_abort_touches_only_txreq)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    spi_write(MCP_REG_TXB0CTRL, 0x03);
    mcp2515_msg_t msg = { .id = 0x123, .dlc = 0, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    ASSERT_EQ(mcp2515_abort_tx(&dev), MCP2515_OK);

    ASSERT_EQ_HEX(model.hw.regs[MCP_REG_TXB0CTRL], 0x03);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANCTRL] & 0x10, 0);    /* ABAT stays 0 */
}

/* The case the README left open: a frame that cannot go out holds up a mode change
 * (p.59), and the abort is the way out. */
TEST(test_abort_releases_a_held_mode_change)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 0, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG), MCP2515_ERR_TIMEOUT);

    ASSERT_EQ(mcp2515_abort_tx(&dev), MCP2515_OK);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG), MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_CONFIG);
}

/* Nothing pending is no fault: the frame may just have gone out. */
TEST(test_abort_with_nothing_pending)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);
    ASSERT_EQ(mcp2515_abort_tx(&dev), MCP2515_OK);

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 0, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    mcp2515_model_finish_tx(&model);
    ASSERT_EQ(mcp2515_abort_tx(&dev), MCP2515_OK);
    ASSERT_TRUE(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_TX0IF);
}

/* p.16 note 1: a frame on the bus goes on, so the abort must not claim the buffer
 * is free, and a later call sees the end of it. */
TEST(test_abort_of_a_frame_on_the_bus_is_busy)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 0, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    model.tx_on_wire = true;
    ASSERT_EQ(mcp2515_abort_tx(&dev), MCP2515_ERR_TX_BUSY);
    ASSERT_TRUE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);

    model.tx_on_wire = false;
    mcp2515_model_finish_tx(&model);
    ASSERT_EQ(mcp2515_abort_tx(&dev), MCP2515_OK);
    ASSERT_TRUE(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_TX0IF);
}

TEST(test_abort_without_a_device)
{
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(mcp2515_abort_tx(NULL), MCP2515_ERR_SPI);
    ASSERT_EQ(mcp2515_abort_tx(&dev), MCP2515_ERR_SPI);
}

/* p.59 and p.60: Listen-Only and Sleep send nothing, and Normal "is also the only
 * mode in which the MCP2515 will transmit messages over the CAN bus" */
TEST(test_model_only_normal_and_loopback_finish_a_transmission)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_LISTEN), MCP2515_OK);

    spi_bit_modify(MCP_REG_TXB0CTRL, MCP_TXB_TXREQ, MCP_TXB_TXREQ);
    mcp2515_model_finish_tx(&model);
    ASSERT_TRUE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_TX0IF, 0);
}

/* p.59, Listen-Only: "no messages will be transmitted while in this mode". Sleep:
 * "The TXCAN pin will remain in the recessive state". p.60: Normal "is also the
 * only mode in which the MCP2515 will transmit messages over the CAN bus", and
 * Loopback passes frames to the receive buffers. A frame queued anywhere else
 * could never go out and, by p.59, would hold up every later mode change. */
TEST(test_send_refused_where_no_frame_can_go_out)
{
    static const mcp2515_mode_t silent[] = {
        MCP2515_MODE_LISTEN, MCP2515_MODE_SLEEP, MCP2515_MODE_CONFIG,
    };
    mcp2515_msg_t msg = { .id = 0x123, .dlc = 1, .data = {0xAA}, .extended = false, .rtr = false };

    for (size_t i = 0; i < sizeof(silent) / sizeof(silent[0]); i++) {
        setup();
        mcp2515_t dev;
        ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);
        ASSERT_EQ(mcp2515_set_mode(&dev, silent[i]), MCP2515_OK);

        ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_ERR_PARAM);
        ASSERT_EQ(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ, 0);
    }
}

TEST(test_send_works_in_loopback)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_LOOPBACK), MCP2515_OK);

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 1, .data = {0xAA}, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    ASSERT_TRUE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);
    mcp2515_model_finish_tx(&model);
    ASSERT_FALSE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);
}

/* p.59: "Configuration mode is the only mode where the following registers are
 * modifiable: CNF1, CNF2, CNF3 registers, TXRTSCTRL register, Filter registers,
 * Mask registers" */
TEST(test_model_config_only_registers_ignore_writes_elsewhere)
{
    static const uint8_t regs[] = {
        MCP_REG_CNF3, MCP_REG_CNF2, MCP_REG_CNF1, MCP_REG_TXRTSCTRL,
        0x00, 0x03, 0x04, 0x07, 0x08, 0x0B,   /* RXF0 to RXF2 */
        0x10, 0x13, 0x14, 0x17, 0x18, 0x1B,   /* RXF3 to RXF5 */
        0x20, 0x23, 0x24, 0x27,               /* RXM0, RXM1 */
    };

    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    for (size_t i = 0; i < sizeof(regs); i++) {
        uint8_t before = model.hw.regs[regs[i]];
        spi_write(regs[i], (uint8_t)~before);
        ASSERT_EQ_HEX(model.hw.regs[regs[i]], before);
        spi_bit_modify(regs[i], 0xFF, (uint8_t)~before);
        ASSERT_EQ_HEX(model.hw.regs[regs[i]], before);
    }

    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG), MCP2515_OK);
    spi_write(MCP_REG_CNF1, 0x41);
    ASSERT_EQ_HEX(model.hw.regs[MCP_REG_CNF1], 0x41);
    spi_write(0x20, 0x7E);
    ASSERT_EQ_HEX(model.hw.regs[0x20], 0x7E);
}

/* Note 1 of every RXFn and RXMn register, p.34 to p.38: "The Mask and Filter
 * registers read all '0's when in any mode except Configuration mode." */
TEST(test_model_masks_and_filters_read_zero_outside_config)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG), MCP2515_OK);
    ASSERT_EQ(mcp2515_set_mask(&dev, 0, 0x7FF, false), MCP2515_OK);
    ASSERT_EQ(mcp2515_set_filter(&dev, 5, 0x7FF, false), MCP2515_OK);
    ASSERT_EQ_HEX(spi_read(MCP_REG_RXM0SIDH), 0xFF);
    ASSERT_EQ_HEX(spi_read(0x18), 0xFF);

    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL), MCP2515_OK);
    ASSERT_EQ_HEX(spi_read(MCP_REG_RXM0SIDH), 0x00);
    ASSERT_EQ_HEX(spi_read(0x18), 0x00);

    /* They kept their value */
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG), MCP2515_OK);
    ASSERT_EQ_HEX(spi_read(MCP_REG_RXM0SIDH), 0xFF);
}

/* p.63: Table 11-2, POR/Reset value */
TEST(test_model_reset_values)
{
    setup();
    mcp2515_model_cs_low(&model);
    mcp2515_model_xfer(&model, MCP_SPI_RESET);
    mcp2515_model_cs_high(&model);
    mcp2515_model_delay_us(&model, MCP_OST_US);

    ASSERT_EQ_HEX(spi_read(MCP_REG_CANSTAT), 0x80);
    ASSERT_EQ_HEX(spi_read(MCP_REG_CANCTRL), 0x87);
    ASSERT_EQ_HEX(spi_read(MCP_REG_CANINTE), 0x00);
    ASSERT_EQ_HEX(spi_read(MCP_REG_CANINTF), 0x00);
    ASSERT_EQ_HEX(spi_read(MCP_REG_EFLG), 0x00);
    ASSERT_EQ_HEX(spi_read(MCP_REG_TXB0CTRL), 0x00);
}

/* Bits marked R in the register tables: the transmit flags, the receive buffers,
 * CANSTAT and most of EFLG belong to the chip */
TEST(test_model_read_only_bits_ignore_writes)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    spi_write(MCP_REG_CANSTAT, 0xFF);
    ASSERT_EQ(spi_read(MCP_REG_CANSTAT) & MCP_CANSTAT_OPMOD_MASK, MCP_OPMOD_NORMAL);

    /* ABTF, MLOA and TXERR are status, TXP and TXREQ are the MCU's */
    spi_write(MCP_REG_TXB0CTRL, 0x73);
    ASSERT_EQ_HEX(spi_read(MCP_REG_TXB0CTRL), 0x03);

    spi_write(MCP_REG_RXB0SIDH, 0xA5);
    ASSERT_EQ_HEX(spi_read(MCP_REG_RXB0SIDH), 0x00);
    spi_write(MCP_REG_EFLG, 0xFF);
    ASSERT_EQ_HEX(spi_read(MCP_REG_EFLG), 0xC0);
}

/* p.15: "When TXREQ is set, the ABTF, MLOA and TXERR bits (TXBnCTRL[5:4]) will be
 * cleared automatically." */
TEST(test_model_txreq_clears_the_transmit_flags)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    model.hw.regs[MCP_REG_TXB0CTRL] = 0x70;
    spi_bit_modify(MCP_REG_TXB0CTRL, MCP_TXB_TXREQ, MCP_TXB_TXREQ);
    ASSERT_EQ_HEX(model.hw.regs[MCP_REG_TXB0CTRL], MCP_TXB_TXREQ);
}

/* Section 12.10 note: "Executing the BIT MODIFY command on registers that are not
 * bit-modifiable will force the mask to FFh." */
TEST(test_model_bit_modify_is_a_write_on_other_registers)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    spi_write(MCP_REG_TXB0D0, 0xFF);
    spi_bit_modify(MCP_REG_TXB0D0, 0x0F, 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MCP_REG_TXB0D0], 0x00);

    /* CANINTE is shaded in Table 11-1, so the mask holds there */
    spi_write(MCP_REG_CANINTE, 0xFF);
    spi_bit_modify(MCP_REG_CANINTE, 0x0F, 0x00);
    ASSERT_EQ_HEX(model.hw.regs[MCP_REG_CANINTE], 0xF0);
}

/* Section 12.3: "The internal Address Pointer is automatically incremented to the
 * next address once each byte of data is shifted out". 12.4: "It is possible to
 * write to sequential registers by continuing to clock in data bytes". */
TEST(test_model_address_pointer_runs_on)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    mcp2515_model_cs_low(&model);
    mcp2515_model_xfer(&model, MCP_SPI_WRITE);
    mcp2515_model_xfer(&model, MCP_REG_TXB0D0);
    mcp2515_model_xfer(&model, 0x11);
    mcp2515_model_xfer(&model, 0x22);
    mcp2515_model_cs_high(&model);
    ASSERT_EQ_HEX(model.hw.regs[MCP_REG_TXB0D0], 0x11);
    ASSERT_EQ_HEX(model.hw.regs[MCP_REG_TXB0D0 + 1], 0x22);

    mcp2515_model_cs_low(&model);
    mcp2515_model_xfer(&model, MCP_SPI_READ);
    mcp2515_model_xfer(&model, MCP_REG_TXB0D0);
    ASSERT_EQ_HEX(mcp2515_model_xfer(&model, 0xFF), 0x11);
    ASSERT_EQ_HEX(mcp2515_model_xfer(&model, 0xFF), 0x22);
    mcp2515_model_cs_high(&model);
}

/* p.15 note: "The TXREQ bit (TXBnCTRL[3]) must be clear (indicating the transmit
 * buffer is not pending transmission) before writing to the transmit buffer." */
TEST(test_send_leaves_a_pending_buffer_alone)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);

    mcp2515_msg_t msg = { .id = 0x123, .dlc = 2, .data = {0xAA, 0xBB}, .extended = false, .rtr = false };
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_OK);
    ASSERT_EQ(model.tx_buffer_writes_pending, 0);
    ASSERT_EQ(mcp2515_send(&dev, &msg), MCP2515_ERR_TX_BUSY);
    ASSERT_EQ(model.tx_buffer_writes_pending, 0);

    /* The counter does count */
    spi_write(MCP_REG_TXB0D0, 0x00);
    ASSERT_EQ(model.tx_buffer_writes_pending, 1);
}

/* p.59: "The MCP2515 will wake-up when bus activity occurs", "If the WAKIE bit is
 * set, the device will wake-up and generate an interrupt", in Listen-Only mode */
TEST(test_model_bus_activity_wakes_only_with_wakie)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_SLEEP), MCP2515_OK);

    mcp2515_model_bus_activity(&model);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK, MCP_OPMOD_SLEEP);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_WAKIF, 0);

    spi_bit_modify(MCP_REG_CANINTE, MCP_CANINTE_WAKIE, MCP_CANINTE_WAKIE);
    mcp2515_model_bus_activity(&model);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK, MCP_OPMOD_LISTEN);
    ASSERT_TRUE(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_WAKIF);
    ASSERT_EQ(model.ost_left_us, MCP_OST_US);
}

/* The bus woke the chip while the driver still thinks it sleeps */
TEST(test_leave_sleep_after_the_bus_woke_the_chip)
{
    setup();
    mcp2515_t dev;
    ASSERT_EQ(mcp2515_init(&dev, &model_spi, MCP2515_SPEED_500KBPS), MCP2515_OK);
    spi_bit_modify(MCP_REG_CANINTE, MCP_CANINTE_WAKIE, MCP_CANINTE_WAKIE);
    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_SLEEP), MCP2515_OK);

    mcp2515_model_bus_activity(&model);
    mcp2515_model_delay_us(&model, MCP_OST_US);

    ASSERT_EQ(mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL), MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK, MCP_OPMOD_NORMAL);
    ASSERT_EQ(model.hw.regs[MCP_REG_CANINTF] & MCP_CANINTF_WAKIF, 0);
    ASSERT_EQ(model.spi_during_ost, 0);
}

TEST_MAIN("mcp2515 hardware model")
    RUN(test_init_config_mode);
    RUN(test_init_waits_out_the_ost_after_reset);
    RUN(test_mode_to_loopback);
    RUN(test_mode_roundtrip);
    RUN(test_cnf_125kbps);
    RUN(test_cnf_500kbps);
    RUN(test_cnf_1mbps);
    RUN(test_tx_buffer_load_and_send);
    RUN(test_txreq_clears_on_completion);
    RUN(test_tx_busy_rejected);
    RUN(test_rx_standard_frame);
    RUN(test_rx_extended_frame);
    RUN(test_filter_requires_config_mode);
    RUN(test_mask_requires_config_mode);
    RUN(test_no_message_pending);
    RUN(test_read_status_byte);
    RUN(test_read_errors_register);
    RUN(test_tx_extended_frame);
    RUN(test_mode_to_sleep);
    RUN(test_mode_to_listen);
    RUN(test_model_sleep_needs_wakif_and_wakie);
    RUN(test_leave_sleep_to_every_mode);
    RUN(test_sleep_cycles);
    RUN(test_leave_sleep_chip_stays_asleep);
    RUN(test_init_invalid_speed_hw);
    RUN(test_rx_from_rxb1);
    RUN(test_filter_extended_encoding);
    RUN(test_mask_extended_encoding);
    RUN(test_tx_rtr_frame);
    RUN(test_cnf_250kbps);
    RUN(test_send_succeeds_when_the_frame_goes_out_at_once);
    RUN(test_receive_succeeds_when_the_next_frame_is_already_in);
    RUN(test_mode_change_waits_for_a_pending_transmission);
    RUN(test_abort_takes_back_a_pending_frame);
    RUN(test_abort_touches_only_txreq);
    RUN(test_abort_releases_a_held_mode_change);
    RUN(test_abort_with_nothing_pending);
    RUN(test_abort_of_a_frame_on_the_bus_is_busy);
    RUN(test_abort_without_a_device);
    RUN(test_model_abort_by_clearing_txreq_ends_the_wait);
    RUN(test_model_only_normal_and_loopback_finish_a_transmission);
    RUN(test_send_refused_where_no_frame_can_go_out);
    RUN(test_send_works_in_loopback);
    RUN(test_model_config_only_registers_ignore_writes_elsewhere);
    RUN(test_model_masks_and_filters_read_zero_outside_config);
    RUN(test_model_reset_values);
    RUN(test_model_read_only_bits_ignore_writes);
    RUN(test_model_txreq_clears_the_transmit_flags);
    RUN(test_model_bit_modify_is_a_write_on_other_registers);
    RUN(test_model_address_pointer_runs_on);
    RUN(test_send_leaves_a_pending_buffer_alone);
    RUN(test_model_bus_activity_wakes_only_with_wakie);
    RUN(test_leave_sleep_after_the_bus_woke_the_chip);
TEST_MAIN_END()
