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

    /* Simulate TX completion: clear TXREQ externally (hardware does this) */
    model.hw.regs[MCP_REG_TXB0CTRL] =
        (uint8_t)(model.hw.regs[MCP_REG_TXB0CTRL] & ~MCP_TXB_TXREQ);
    ASSERT_FALSE(model.hw.regs[MCP_REG_TXB0CTRL] & MCP_TXB_TXREQ);

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
    /* Mask 0 base = 0x20; SIDL (0x21) must have EXIDE bit */
    ASSERT_TRUE(model.hw.regs[0x21] & 0x08);
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
TEST_MAIN_END()
