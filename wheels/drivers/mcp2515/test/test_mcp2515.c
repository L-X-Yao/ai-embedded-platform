// SPDX-License-Identifier: MIT
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
    /* RXB0: extended frame — uses explicit SID/EID decomposition per datasheet */
    uint16_t sid = (uint16_t)(id >> 18);       /* SID[10:0] */
    uint32_t eid = id & 0x3FFFFu;              /* EID[17:0] */

    mock_regs[0x61] = (uint8_t)(sid >> 3);                      /* SIDH: SID[10:3] */
    mock_regs[0x62] = (uint8_t)(((sid & 0x07u) << 5) | 0x08 |  /* SIDL: SID[2:0] + IDE */
                                 ((eid >> 16) & 0x03u));         /*       EID[17:16]     */
    mock_regs[0x63] = (uint8_t)(eid >> 8);                      /* EID8: EID[15:8] */
    mock_regs[0x64] = (uint8_t)(eid);                           /* EID0: EID[7:0]  */
    mock_regs[0x65] = dlc;
    for (uint8_t i = 0; i < dlc && i < 8; i++)
        mock_regs[0x66 + i] = data[i];
    mock_regs[0x2C] |= 0x01;
}

static void inject_rx1_std(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    /* RXB1: standard frame (registers 0x71-0x76) */
    mock_regs[0x71] = (uint8_t)(id >> 3);          /* RXB1SIDH */
    mock_regs[0x72] = (uint8_t)((id << 5) & 0xE0); /* RXB1SIDL, IDE=0 */
    mock_regs[0x73] = 0;
    mock_regs[0x74] = 0;
    mock_regs[0x75] = dlc;
    for (uint8_t i = 0; i < dlc && i < 8; i++)
        mock_regs[0x76 + i] = data[i];
    /* set RX1IF (bit 1) only — not RX0IF */
    mock_regs[0x2C] = (uint8_t)((mock_regs[0x2C] & ~0x01u) | 0x02u);
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
    ASSERT_EQ(mcp2515_set_mask(NULL, 0, 0, false), MCP2515_ERR_SPI);

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
    ASSERT_EQ(e, MCP2515_ERR_PARAM);

    /* not in config mode */
    mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL);
    e = mcp2515_set_filter(&dev, 0, 0x123, false);
    ASSERT_EQ(e, MCP2515_ERR_PARAM);
}

TEST(test_set_mask)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);

    mcp2515_err_t e = mcp2515_set_mask(&dev, 0, 0x7FF, false);
    ASSERT_EQ(e, MCP2515_OK);

    /* verify mask register: 0x7FF >> 3 = 0xFF */
    ASSERT_EQ(mock_regs[0x20], 0xFF);
    /* SIDL: (0x7FF << 5) & 0xE0 = 0xE0 */
    ASSERT_EQ(mock_regs[0x21], 0xE0);

    /* mask_num out of range */
    e = mcp2515_set_mask(&dev, 2, 0x7FF, false);
    ASSERT_EQ(e, MCP2515_ERR_PARAM);

    /* not in config mode */
    mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL);
    e = mcp2515_set_mask(&dev, 0, 0x7FF, false);
    ASSERT_EQ(e, MCP2515_ERR_PARAM);
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
    /* no legal 1 Mbps timing exists at 8 MHz: PS2 needs 2 TQ, and a bit has 4 */
    ASSERT_EQ(mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_1MBPS), MCP2515_ERR_PARAM);
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
    ASSERT_EQ(e, MCP2515_ERR_PARAM);
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

/* ---------- MCP2515_ERR_TIMEOUT: mode change never confirms ---------- */

static int mock_freeze_canstat;   /* when true, CANSTAT ignores CANCTRL writes */

/*
 * We need a custom mock_xfer that can optionally freeze CANSTAT so
 * mode-change verification times out.  Wrap the existing mock by
 * intercepting CANCTRL writes when freeze is active.
 */
static uint8_t mock_xfer_freeze(void *ctx, uint8_t tx)
{
    uint8_t result = mock_xfer(ctx, tx);

    /* After a write or bit-modify to CANCTRL (0x0F), normally the mock
     * mirrors bits to CANSTAT (0x0E).  If freezing, revert CANSTAT to
     * its previous value so wait_mode() never sees the expected mode. */
    if (mock_freeze_canstat && mock_spi_addr == 0x0F &&
        (mock_spi_cmd == 0x02 || mock_spi_cmd == 0x05)) {
        /* Keep CANSTAT stuck in its current mode — set to config (0x80)
         * regardless of what was written to CANCTRL */
        mock_regs[0x0E] = 0x80;
    }
    return result;
}

static const mcp2515_spi_t mock_spi_freeze = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer_freeze,
    .ctx     = NULL,
};

TEST(test_set_mode_timeout)
{
    mock_reset();
    mock_freeze_canstat = 0;

    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    /* Normal init first (freeze off) */
    mcp2515_err_t e = mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);
    ASSERT_EQ(e, MCP2515_OK);

    /* Now rewire to the freezing SPI and try to switch to loopback.
     * CANSTAT will stay stuck at normal (0x00), never showing loopback. */
    dev.spi = &mock_spi_freeze;
    mock_freeze_canstat = 1;
    /* Force CANSTAT to normal-mode bits so it never matches loopback (0x40) */
    mock_regs[0x0E] = 0x00;

    e = mcp2515_set_mode(&dev, MCP2515_MODE_LOOPBACK);
    ASSERT_EQ(e, MCP2515_ERR_TIMEOUT);
    /* mode should NOT have been updated since the switch failed */
    ASSERT_EQ(dev.mode, MCP2515_MODE_NORMAL);

    mock_freeze_canstat = 0;
}

TEST(test_init_mode_timeout)
{
    mock_reset();
    mock_freeze_canstat = 0;

    /* init calls wait_mode twice: once after reset (expect config), once
     * for normal mode.  If CANSTAT is frozen to 0xFF (no valid mode),
     * the first wait_mode after reset will time out. */
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));

    /* Freeze CANSTAT to 0xFF so config mode (0x80) is never confirmed */
    dev.spi = &mock_spi_freeze;
    mock_freeze_canstat = 1;
    mock_regs[0x0E] = 0xFF;

    mcp2515_err_t e = mcp2515_init(&dev, &mock_spi_freeze, MCP2515_SPEED_500KBPS);
    ASSERT_EQ(e, MCP2515_ERR_TIMEOUT);

    mock_freeze_canstat = 0;
}

TEST(test_extended_id_register_bytes)
{
    /*
     * Verify exact register byte layout for extended ID 0x1ABCDEF per
     * MCP2515 datasheet TXBnSIDH/SIDL/EID8/EID0 format.
     *
     * id = 0x1ABCDEF = 0b 0001 1010 1011 1100 1101 1110 1111 (29 bits)
     * SID[10:0] = id >> 18 = 0x06A = 0b 000 0110 1010
     * EID[17:0] = id & 0x3FFFF = 0x3CDEF = 0b 11 1100 1101 1110 1111
     *
     * SIDH = SID[10:3]  = 0x0D
     * SIDL = SID[2:0]<<5 | EXIDE | EID[17:16]
     *      = (0b010 << 5) | 0x08 | 0b11
     *      = 0x40 | 0x08 | 0x03 = 0x4B
     * EID8 = EID[15:8]  = 0xCD
     * EID0 = EID[7:0]   = 0xEF
     */
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    /* --- TX: encode and verify register bytes --- */
    mcp2515_msg_t msg = {
        .id = 0x1ABCDEF,
        .dlc = 1,
        .data = {0x42},
        .extended = true,
        .rtr = false,
    };
    mcp2515_err_t e = mcp2515_send(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);

    ASSERT_EQ(mock_regs[0x31], 0x0D);  /* SIDH: SID[10:3] */
    ASSERT_EQ(mock_regs[0x32], 0x4B);  /* SIDL: SID[2:0]<<5 | EXIDE | EID[17:16] */
    ASSERT_EQ(mock_regs[0x33], 0xCD);  /* EID8: EID[15:8] */
    ASSERT_EQ(mock_regs[0x34], 0xEF);  /* EID0: EID[7:0] */

    /* --- RX: inject same bytes and verify decode roundtrip --- */
    mock_regs[0x61] = 0x0D;
    mock_regs[0x62] = 0x4B;
    mock_regs[0x63] = 0xCD;
    mock_regs[0x64] = 0xEF;
    mock_regs[0x65] = 1;
    mock_regs[0x66] = 0x42;
    mock_regs[0x2C] |= 0x01;

    mcp2515_msg_t rx;
    memset(&rx, 0, sizeof(rx));
    e = mcp2515_receive(&dev, &rx);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_TRUE(rx.extended);
    ASSERT_EQ(rx.id, 0x1ABCDEFU);
    ASSERT_EQ(rx.dlc, 1);
    ASSERT_EQ(rx.data[0], 0x42);
}

TEST(test_set_mode_sleep)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_err_t e = mcp2515_set_mode(&dev, MCP2515_MODE_SLEEP);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_SLEEP);
    /* CANSTAT bits[7:5] should be 0x20 (sleep) */
    ASSERT_EQ(mock_regs[0x0E] & 0xE0, 0x20);
}

TEST(test_set_mode_listen)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_err_t e = mcp2515_set_mode(&dev, MCP2515_MODE_LISTEN);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_EQ(dev.mode, MCP2515_MODE_LISTEN);
    /* CANSTAT bits[7:5] should be 0x60 (listen-only) */
    ASSERT_EQ(mock_regs[0x0E] & 0xE0, 0x60);
}

TEST(test_init_invalid_speed)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    /* Speed 4 is out of the valid enum range (0-3) */
    mcp2515_err_t e = mcp2515_init(&dev, &mock_spi, (mcp2515_speed_t)4);
    ASSERT_EQ(e, MCP2515_ERR_PARAM);
}

TEST(test_receive_from_rxb1)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    /* Inject a frame into RXB1 (RX1IF set, RX0IF clear) */
    uint8_t data[] = {0xAB, 0xCD, 0xEF};
    inject_rx1_std(0x55A, data, 3);

    /* has_message should be true (RX1IF is set) */
    ASSERT_TRUE(mcp2515_has_message(&dev));

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    mcp2515_err_t e = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_EQ(msg.id, 0x55AU);
    ASSERT_EQ(msg.dlc, 3);
    ASSERT_FALSE(msg.extended);
    ASSERT_EQ(msg.data[0], 0xAB);
    ASSERT_EQ(msg.data[1], 0xCD);
    ASSERT_EQ(msg.data[2], 0xEF);

    /* RX1IF should be cleared */
    ASSERT_FALSE(mock_regs[0x2C] & 0x02);
}

TEST(test_send_rtr_frame)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    mcp2515_msg_t msg = {
        .id = 0x100,
        .dlc = 0,
        .data = {0},
        .extended = false,
        .rtr = true,
    };
    mcp2515_err_t e = mcp2515_send(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);
    /* DLC register should have RTR bit set (bit 6) */
    ASSERT_TRUE(mock_regs[0x35] & 0x40);
}

TEST(test_receive_rtr_frame)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    /* Inject a frame with RTR bit set in DLC register */
    uint8_t data[1] = {0};
    inject_rx_std(0x200, data, 0);
    mock_regs[0x65] = 0x40;  /* RTR=1, DLC=0 */

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    mcp2515_err_t e = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);
    ASSERT_TRUE(msg.rtr);
    ASSERT_EQ(msg.dlc, 0);
}

TEST(test_receive_dlc_clamped)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);

    /* Inject a frame with DLC > 8 (should be clamped to 8) */
    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    inject_rx_std(0x300, data, 9);  /* DLC=9 in register */
    mock_regs[0x65] = 0x0F;         /* raw DLC = 15 */

    mcp2515_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    mcp2515_err_t e = mcp2515_receive(&dev, &msg);
    ASSERT_EQ(e, MCP2515_OK);
    /* DLC must be clamped to 8 */
    ASSERT_EQ(msg.dlc, 8);
}

TEST(test_set_filter_all_six)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);
    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);

    /* All 6 filter numbers (0-5) must succeed */
    static const uint8_t filter_bases[] = {0x00, 0x04, 0x08, 0x10, 0x14, 0x18};
    for (uint8_t f = 0; f <= 5; f++) {
        mcp2515_err_t e = mcp2515_set_filter(&dev, f, 0x7FF, false);
        ASSERT_EQ(e, MCP2515_OK);
        /* SIDH for filter f: 0x7FF >> 3 = 0xFF */
        ASSERT_EQ(mock_regs[filter_bases[f]], 0xFF);
    }
}

TEST(test_set_filter_extended)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);
    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);

    /* Extended filter: 29-bit ID encodes into 4 registers */
    mcp2515_err_t e = mcp2515_set_filter(&dev, 0, 0x12345, true);
    ASSERT_EQ(e, MCP2515_OK);
    /* SIDL should have EXIDE bit (0x08) set */
    ASSERT_TRUE(mock_regs[0x01] & 0x08);
}

TEST(test_set_mask_extended)
{
    mock_reset();
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    mcp2515_init(&dev, &mock_spi, MCP2515_SPEED_500KBPS);
    mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);

    /* Extended mask: 29-bit mask via encode_id (should set EXIDE bit) */
    mcp2515_err_t e = mcp2515_set_mask(&dev, 1, 0x1FFFFFFF, true);
    ASSERT_EQ(e, MCP2515_OK);
    /* Mask 1 base is 0x24 — SIDL (0x25) should have EXIDE */
    ASSERT_TRUE(mock_regs[0x25] & 0x08);
}

TEST(test_set_mode_null_spi)
{
    /* set_mode with dev->spi == NULL returns ERR_SPI */
    mcp2515_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.spi = NULL;
    mcp2515_err_t e = mcp2515_set_mode(&dev, MCP2515_MODE_NORMAL);
    ASSERT_EQ(e, MCP2515_ERR_SPI);
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
    RUN(test_extended_id_register_bytes);
    RUN(test_set_mode_timeout);
    RUN(test_init_mode_timeout);
    RUN(test_set_mode_sleep);
    RUN(test_set_mode_listen);
    RUN(test_init_invalid_speed);
    RUN(test_receive_from_rxb1);
    RUN(test_send_rtr_frame);
    RUN(test_receive_rtr_frame);
    RUN(test_receive_dlc_clamped);
    RUN(test_set_filter_all_six);
    RUN(test_set_filter_extended);
    RUN(test_set_mask_extended);
    RUN(test_set_mode_null_spi);
TEST_MAIN_END()
