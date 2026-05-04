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
    .cs_low  = mcp2515_model_cs_low,
    .cs_high = mcp2515_model_cs_high,
    .xfer    = mcp2515_model_xfer,
    .ctx     = &model,
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
    mcp2515_init(&dev, &model_spi, MCP2515_SPEED_1MBPS);

    ASSERT_EQ(model.hw.regs[MCP_REG_CNF1], 0x00);
    ASSERT_EQ(model.hw.regs[MCP_REG_CNF2], 0x80);
    ASSERT_EQ(model.hw.regs[MCP_REG_CNF3], 0x00);
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

TEST_MAIN("mcp2515 hardware model")
    RUN(test_init_config_mode);
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
TEST_MAIN_END()
