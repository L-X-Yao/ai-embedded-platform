#include "test_harness.h"
#include <string.h>
#include "mcp2515.h"

/* ---------- mock MCP2515 register file ---------- */

#define MOCK_REG_SIZE 256
static uint8_t mock_regs[MOCK_REG_SIZE];
static int     mock_cs;

/* SPI protocol state machine */
static uint8_t mock_spi_cmd;
static uint8_t mock_spi_addr;
static int     mock_spi_phase;  /* 0=idle, 1=cmd, 2=addr, 3=data */
static uint8_t mock_bm_mask;

/* for READ_STATUS instruction */
static uint8_t mock_status_byte;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_cs = 1;
    mock_spi_cmd = 0;
    mock_spi_addr = 0;
    mock_spi_phase = 0;
    mock_bm_mask = 0;
    mock_status_byte = 0;

    /* after hardware reset, chip is in config mode (0x80 in CANSTAT) */
    mock_regs[0x0E] = 0x80;  /* CANSTAT: config mode */
    mock_regs[0x0F] = 0x80;  /* CANCTRL: config mode */
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_cs = 0;
    mock_spi_phase = 1;
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_cs = 1;
    mock_spi_phase = 0;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    if (mock_spi_phase == 1) {
        mock_spi_cmd = tx;
        if (tx == 0xC0) {
            /* RESET: put chip into config mode */
            mock_regs[0x0E] = 0x80;
            mock_regs[0x0F] = 0x80;
            return 0xFF;
        }
        if (tx == 0xA0) {
            /* READ_STATUS: next byte returns status */
            mock_spi_phase = 3;
            return 0xFF;
        }
        mock_spi_phase = 2;
        return 0xFF;
    }

    if (mock_spi_phase == 2) {
        mock_spi_addr = tx;
        if (mock_spi_cmd == 0x05) {
            /* BIT_MODIFY: next byte is mask */
            mock_spi_phase = 4;
        } else {
            mock_spi_phase = 3;
        }
        return 0xFF;
    }

    if (mock_spi_phase == 3) {
        if (mock_spi_cmd == 0x03) {
            /* READ: return register value */
            return mock_regs[mock_spi_addr];
        }
        if (mock_spi_cmd == 0x02) {
            /* WRITE: store value */
            mock_regs[mock_spi_addr] = tx;
            /* simulate mode change via CANCTRL write */
            if (mock_spi_addr == 0x0F) {
                mock_regs[0x0E] = (uint8_t)((mock_regs[0x0E] & ~0xE0) |
                                             (tx & 0xE0));
            }
            return 0xFF;
        }
        if (mock_spi_cmd == 0xA0) {
            /* READ_STATUS: return status byte */
            return mock_status_byte;
        }
        return 0xFF;
    }

    if (mock_spi_phase == 4) {
        /* BIT_MODIFY: this byte is the mask */
        mock_bm_mask = tx;
        mock_spi_phase = 5;
        return 0xFF;
    }

    if (mock_spi_phase == 5) {
        /* BIT_MODIFY: this byte is the data */
        mock_regs[mock_spi_addr] = (uint8_t)(
            (mock_regs[mock_spi_addr] & ~mock_bm_mask) |
            (tx & mock_bm_mask));
        /* simulate mode change */
        if (mock_spi_addr == 0x0F) {
            mock_regs[0x0E] = (uint8_t)((mock_regs[0x0E] & ~0xE0) |
                                         (mock_regs[0x0F] & 0xE0));
        }
        return 0xFF;
    }

    return 0xFF;
}

static const mcp2515_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ---------- helper: inject an RX message into mock registers ---------- */

static void inject_rx_std(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    /* RXB0: standard frame */
    mock_regs[0x61] = (uint8_t)(id >> 3);          /* SIDH */
    mock_regs[0x62] = (uint8_t)((id << 5) & 0xE0); /* SIDL, IDE=0 */
    mock_regs[0x63] = 0;
    mock_regs[0x64] = 0;
    mock_regs[0x65] = dlc;
    for (uint8_t i = 0; i < dlc && i < 8; i++)
        mock_regs[0x66 + i] = data[i];
    /* set RX0IF */
    mock_regs[0x2C] |= 0x01;
}

static void inject_rx_ext(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    /* RXB0: extended frame */
    mock_regs[0x61] = (uint8_t)(id >> 21);
    mock_regs[0x62] = (uint8_t)(((id >> 13) & 0xE0) | 0x08 |
                                 ((id >> 16) & 0x03));
    mock_regs[0x63] = (uint8_t)(id >> 8);
    mock_regs[0x64] = (uint8_t)(id);
    mock_regs[0x65] = dlc;
    for (uint8_t i = 0; i < dlc && i < 8; i++)
        mock_regs[0x66 + i] = data[i];
    mock_regs[0x2C] |= 0x01;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_err_t e = mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);
    /* verify CNF1 was written (500 kbps => 0x00) */
    ASSERT_EQ(mock_regs[0x2A], 0x00);
}

TEST(test_null_params)
{
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(mcp2515_init(NULL, &mock_spi, MCP2515_SPEED_500KBPS),
              MCP2515_ERR_SPI);
    ASSERT_EQ(mcp2515_init(&dev, NULL, MCP2515_SPEED_500KBPS),
              MCP2515_ERR_SPI);

    /* send/receive with NULL */
    dev.spi = &mock_spi;
    ASSERT_EQ(mcp2515_send(NULL, NULL), MCP2515_ERR_SPI);
    ASSERT_EQ(mcp2515_receive(NULL, NULL), MCP2515_ERR_SPI);

    mcp2515_msg_t msg;
    ASSERT_EQ(mcp2515_send(&dev, NULL), MCP2515_ERR_SPI);
    ASSERT_EQ(mcp2515_receive(&dev, NULL), MCP2515_ERR_SPI);

    /* set_filter/set_mask with NULL dev */
    ASSERT_EQ(mcp2515_set_filter(NULL, 0, 0, false), MCP2515_ERR_SPI);
    ASSERT_EQ(mcp2515_set_mask(NULL, 0, 0), MCP2515_ERR_SPI);

    /* has_message with NULL */
    ASSERT_FALSE(mcp2515_has_message(NULL));

    /* read_status/read_errors with NULL */
    ASSERT_EQ(mcp2515_read_status(NULL), 0);
    ASSERT_EQ(mcp2515_read_errors(NULL), 0);

    (void)msg;
}

TEST(test_set_mode_normal)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_250KBPS);
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);

    /* switch to loopback */
    mcp2515_err_t e = mcp2515_set_mode(&dev, MCP2515_MODE_LOOPBACK);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_LOOPBACK);
}

TEST(test_set_mode_loopback)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_250KBPS);

    mcp2515_err_t e = mcp2515_set_mode(&dev, MCP2515_MODE_LOOPBACK);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_LOOPBACK);

    /* verify CANSTAT reflects loopback (0x40) */
    ASSERT_EQ(mock_regs[0x0E] & 0xE0, 0x40);
}

TEST(test_send_standard_frame)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg = {
        .id = 0x123,
        .dlc = 3,
        .data = {0xAA, 0xBB, 0xCC},
        .extended = false,
        .rtr = false,
    };
    mcp2515_err_t e = mcp2515_send(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);

    /* check TXB0SIDH: 0x123 >> 3 = 0x24 */
    ASSERT_EQ(mock_regs[0x31], 0x24);
    /* check TXB0SIDL: (0x123 << 5) & 0xE0 = 0x60 */
    ASSERT_EQ(mock_regs[0x32], 0x60);
    /* DLC */
    ASSERT_EQ(mock_regs[0x35], 3);
    /* data */
    ASSERT_EQ(mock_regs[0x36], 0xAA);
    ASSERT_EQ(mock_regs[0x37], 0xBB);
    ASSERT_EQ(mock_regs[0x38], 0xCC);
    /* TXREQ bit set */
    ASSERT_TRUE(mock_regs[0x30] & 0x08);
}

TEST(test_send_extended_frame)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg = {
        .id = 0x12345,
        .dlc = 2,
        .data = {0xDE, 0xAD},
        .extended = true,
        .rtr = false,
    };
    mcp2515_err_t e = mcp2515_send(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);

    /* SIDL should have EXIDE bit set (0x08) */
    ASSERT_TRUE(mock_regs[0x32] & 0x08);
    /* DLC */
    ASSERT_EQ(mock_regs[0x35], 2);
}

TEST(test_receive_message)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    /* inject a standard frame: ID=0x7FF, DLC=4 */
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    inject_rx_std(0x7FF, data, 4);

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    mcp2515_err_t e = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_EQ(msg.id, 0x7FFU);
    ASSERT_EQ(msg.dlc, 4);
    ASSERT_FALSE(msg.extended);
    ASSERT_EQ(msg.data[0], 0x11);
    ASSERT_EQ(msg.data[3], 0x44);

    /* RX0IF should be cleared */
    ASSERT_FALSE(mock_regs[0x2C] & 0x01);
}

TEST(test_has_message)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    /* no message pending */
    ASSERT_FALSE(mcp2515_has_message(&dev));

    /* inject one */
    uint8_t data[] = {0x42};
    inject_rx_std(0x100, data, 1);
    ASSERT_TRUE(mcp2515_has_message(&dev));
}

TEST(test_no_message)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg;
    mcp2515_err_t e = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(e, MCP2515_ERR_NO_MSG);
}

TEST(test_set_filter)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    /* must be in config mode for filter */
    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);

    mcp2515_err_t e = mcp2515_set_filter(&dev, 0, 0x123, false);
    ASSERT_EQ(e, MCP2515_OK);

    /* filter_num out of range */
    e = mcp2515_set_filter(&dev, 6, 0x123, false);
    ASSERT_EQ(e, MCP2515_ERR_PARAMS);

    /* not in config mode */
    mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL);
    e = mcp2515_set_filter(&dev, 0, 0x123, false);
    ASSERT_EQ(e, MCP2515_ERR_PARAMS);
}

TEST(test_set_mask)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);

    mcp2515_err_t e = mcp2515_set_mask(&dev, 0, 0x7FF);
    ASSERT_EQ(e, MCP2515_OK);

    /* verify mask register: 0x7FF >> 3 = 0xFF */
    ASSERT_EQ(mock_regs[0x20], 0xFF);
    /* SIDL: (0x7FF << 5) & 0xE0 = 0xE0 */
    ASSERT_EQ(mock_regs[0x21], 0xE0);

    /* mask_num out of range */
    e = mcp2515_set_mask(&dev, 2, 0x7FF);
    ASSERT_EQ(e, MCP2515_ERR_PARAMS);

    /* not in config mode */
    mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL);
    e = mcp2515_set_mask(&dev, 0, 0x7FF);
    ASSERT_EQ(e, MCP2515_ERR_PARAMS);
}

TEST(test_read_status)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mock_status_byte = 0x42;
    uint8_t st = mcp2515_read_status(&dev);
    ASSERT_EQ(st, 0x42);
}

TEST(test_read_errors)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mock_regs[0x2D] = 0xAB;  /* EFLG register */
    uint8_t ef = mcp2515_read_errors(&dev);
    ASSERT_EQ(ef, 0xAB);
}

TEST(test_speed_config)
{
    /* Test all 4 speed settings write correct CNF registers */
    mock_reset();
    mcp2515_t dev;

    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_125KBPS);
    ASSERT_EQ(mock_regs[0x2A], 0x03);  /* CNF1 */
    ASSERT_EQ(mock_regs[0x29], 0x90);  /* CNF2 */
    ASSERT_EQ(mock_regs[0x28], 0x02);  /* CNF3 */

    mock_reset();
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_250KBPS);
    ASSERT_EQ(mock_regs[0x2A], 0x01);

    mock_reset();
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);
    ASSERT_EQ(mock_regs[0x2A], 0x00);

    mock_reset();
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_1MBPS);
    ASSERT_EQ(mock_regs[0x2A], 0x00);
    ASSERT_EQ(mock_regs[0x29], 0x80);
    ASSERT_EQ(mock_regs[0x28], 0x00);
}

TEST(test_tx_busy)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    /* set TXREQ bit in TXB0CTRL to simulate busy */
    mock_regs[0x30] = 0x08;

    mcp2515_msg_t msg = {
        .id = 0x100,
        .dlc = 1,
        .data = {0xFF},
        .extended = false,
        .rtr = false,
    };
    mcp2515_err_t e = mcp2515_send(&dev, &msg);
    ASSERT_EQ(e, MCP2515_ERR_TX_BUSY);
}

TEST(test_send_dlc_too_large)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg = {
        .id = 0x100,
        .dlc = 9,
        .data = {0},
        .extended = false,
        .rtr = false,
    };
    mcp2515_err_t e = mcp2515_send(&dev, &msg);
    ASSERT_EQ(e, MCP2515_ERR_PARAMS);
}

TEST(test_receive_extended_frame)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    uint8_t data[] = {0xCA, 0xFE};
    inject_rx_ext(0x12345, data, 2);

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    mcp2515_err_t e = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_TRUE(msg.extended);
    ASSERT_EQ(msg.id, 0x12345U);
    ASSERT_EQ(msg.dlc, 2);
    ASSERT_EQ(msg.data[0], 0xCA);
    ASSERT_EQ(msg.data[1], 0xFE);
}

/* ---------- main ---------- */

TEST_MAIN("mcp2515")
    RUN(test_init_ok);
    RUN(test_null_params);
    RUN(test_set_mode_normal);
    RUN(test_set_mode_loopback);
    RUN(test_send_standard_frame);
    RUN(test_send_extended_frame);
    RUN(test_receive_message);
    RUN(test_has_message);
    RUN(test_no_message);
    RUN(test_set_filter);
    RUN(test_set_mask);
    RUN(test_read_status);
    RUN(test_read_errors);
    RUN(test_speed_config);
    RUN(test_tx_busy);
    RUN(test_send_dlc_too_large);
    RUN(test_receive_extended_frame);
TEST_MAIN_END()
