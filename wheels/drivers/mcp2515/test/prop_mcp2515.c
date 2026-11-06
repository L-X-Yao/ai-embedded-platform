// SPDX-License-Identifier: MIT
/*
 * prop_mcp2515.c — property-based tests for MCP2515 CAN controller
 *
 * Uses a stateful register-file mock so that mode transitions succeed
 * (CANSTAT mirrors CANCTRL), enabling the driver to reach normal operation
 * rather than timing out during init. This gives the property tests
 * meaningful line coverage across all driver paths.
 */

#include "test_harness.h"
#include "property_test.h"
#include "mcp2515.h"
#include <string.h>

/* ---- Embedded stateful SPI mock (register file) ---- */

#define PROP_REG_SIZE 256

static uint8_t  prop_regs[PROP_REG_SIZE];
static uint8_t  prop_spi_cmd;
static uint8_t  prop_spi_addr;
static int      prop_spi_phase;   /* 0=idle,1=cmd,2=addr,3=data,4=bm_mask,5=bm_data */
static uint8_t  prop_bm_mask;
static uint8_t  prop_status_byte;

static void prop_mock_reset_state(void)
{
    memset(prop_regs, 0, sizeof(prop_regs));
    prop_spi_cmd   = 0;
    prop_spi_addr  = 0;
    prop_spi_phase = 0;
    prop_bm_mask   = 0;
    prop_status_byte = 0;
    /* After hardware reset, chip starts in config mode */
    prop_regs[0x0E] = 0x80;  /* CANSTAT: config mode */
    prop_regs[0x0F] = 0x80;  /* CANCTRL: config mode */
}

static void prop_cs_low(void *ctx)
{
    (void)ctx;
    prop_spi_phase = 1;
}

static void prop_cs_high(void *ctx)
{
    (void)ctx;
    prop_spi_phase = 0;
}

static uint8_t prop_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    if (prop_spi_phase == 1) {
        prop_spi_cmd = tx;
        if (tx == 0xC0) {
            /* RESET: enter config mode */
            prop_regs[0x0E] = 0x80;
            prop_regs[0x0F] = 0x80;
            return 0xFF;
        }
        if (tx == 0xA0) {
            /* READ_STATUS: next byte is data phase */
            prop_spi_phase = 3;
            return 0xFF;
        }
        prop_spi_phase = 2;
        return 0xFF;
    }

    if (prop_spi_phase == 2) {
        prop_spi_addr = tx;
        if (prop_spi_cmd == 0x05) {
            prop_spi_phase = 4;
        } else {
            prop_spi_phase = 3;
        }
        return 0xFF;
    }

    if (prop_spi_phase == 3) {
        if (prop_spi_cmd == 0x03) {
            return prop_regs[prop_spi_addr];
        }
        if (prop_spi_cmd == 0x02) {
            prop_regs[prop_spi_addr] = tx;
            if (prop_spi_addr == 0x0F) {
                /* Mirror mode bits to CANSTAT */
                prop_regs[0x0E] = (uint8_t)((prop_regs[0x0E] & 0x1Fu) |
                                             (tx & 0xE0u));
            }
            return 0xFF;
        }
        if (prop_spi_cmd == 0xA0) {
            return prop_status_byte;
        }
        return 0xFF;
    }

    if (prop_spi_phase == 4) {
        prop_bm_mask = tx;
        prop_spi_phase = 5;
        return 0xFF;
    }

    if (prop_spi_phase == 5) {
        uint8_t old = prop_regs[prop_spi_addr];
        uint8_t nv  = (uint8_t)((old & ~prop_bm_mask) | (tx & prop_bm_mask));
        prop_regs[prop_spi_addr] = nv;
        if (prop_spi_addr == 0x0F) {
            prop_regs[0x0E] = (uint8_t)((prop_regs[0x0E] & 0x1Fu) |
                                         (nv & 0xE0u));
        }
        return 0xFF;
    }

    return 0xFF;
}

static void prop_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    (void)us;
}

static const mcp2515_spi_t prop_spi = {
    .cs_low   = prop_cs_low,
    .cs_high  = prop_cs_high,
    .xfer     = prop_xfer,
    .ctx      = NULL,
    .delay_us = prop_delay_us,
};

/* ---- Helper: inject standard frame into RXB0 ---- */

static void prop_inject_rx_std(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    prop_regs[0x61] = (uint8_t)(id >> 3);
    prop_regs[0x62] = (uint8_t)((id << 5) & 0xE0);
    prop_regs[0x63] = 0;
    prop_regs[0x64] = 0;
    prop_regs[0x65] = dlc;
    for (uint8_t i = 0; i < dlc && i < 8; i++)
        prop_regs[0x66u + i] = data[i];
    prop_regs[0x2C] |= 0x01;
}

/* ---- Property tests ---- */

/*
 * init_roundtrip: init with any valid speed must succeed and leave device
 * in normal mode.
 */
PROPERTY(init_roundtrip, 200)
{
    prop_mock_reset_state();
    mcp2515_t dev;
    uint8_t speed_idx = (uint8_t)(ARBI_UINT8(seed) % 3u);
    mcp2515_err_t err = mcp2515_init(&dev, &prop_spi,
                                     (mcp2515_speed_t)speed_idx);
    PROP_ASSERT(err == MCP2515_OK);
    PROP_ASSERT(dev.mode == MCP2515_MODE_NORMAL);
}

/*
 * mode_transition: after init, any valid mode change must succeed.
 */
PROPERTY(mode_transition, 200)
{
    prop_mock_reset_state();
    mcp2515_t dev;
    mcp2515_err_t err = mcp2515_init(&dev, &prop_spi, MCP2515_SPEED_250KBPS);
    PROP_ASSERT(err == MCP2515_OK);

    uint8_t mode_idx = (uint8_t)(ARBI_UINT8(seed) % 5u);
    mcp2515_mode_t target = (mcp2515_mode_t)mode_idx;
    err = mcp2515_set_mode(&dev, target);
    PROP_ASSERT(err == MCP2515_OK);
    PROP_ASSERT(dev.mode == target);
}

/*
 * send_receive_roundtrip: inject a standard frame and receive it back.
 * Verifies ID, DLC, and first data byte survive the round trip.
 */
PROPERTY(send_receive_roundtrip, 200)
{
    prop_mock_reset_state();
    mcp2515_t dev;
    mcp2515_err_t err = mcp2515_init(&dev, &prop_spi, MCP2515_SPEED_500KBPS);
    PROP_ASSERT(err == MCP2515_OK);

    uint32_t id  = ARBI_UINT32(seed) & 0x7FFu;
    uint8_t  dlc = (uint8_t)(ARBI_UINT8(seed) % 9u);
    uint8_t  data[8];
    ARBI_FILL(seed, data, 8);

    uint8_t tx_data[8];
    memcpy(tx_data, data, sizeof(tx_data));

    /* Send */
    mcp2515_msg_t tx_msg = {
        .id = id,
        .dlc = dlc,
        .extended = false,
        .rtr = false,
    };
    memcpy(tx_msg.data, tx_data, sizeof(tx_msg.data));
    err = mcp2515_send(&dev, &tx_msg);
    PROP_ASSERT(err == MCP2515_OK);

    /* Inject the same frame into RXB0 for receive */
    prop_inject_rx_std(id, tx_data, dlc);

    /* Receive */
    mcp2515_msg_t rx_msg;
    memset(&rx_msg, 0, sizeof(rx_msg));
    err = mcp2515_receive(&dev, &rx_msg);
    PROP_ASSERT(err == MCP2515_OK);
    PROP_ASSERT(rx_msg.id == id);
    PROP_ASSERT(rx_msg.dlc == dlc);
    if (dlc > 0) {
        PROP_ASSERT(rx_msg.data[0] == tx_data[0]);
    }
}

/*
 * status_errors_no_crash: read_status and read_errors must not crash.
 */
PROPERTY(status_errors_no_crash, 200)
{
    prop_mock_reset_state();
    mcp2515_t dev;
    mcp2515_err_t err = mcp2515_init(&dev, &prop_spi, MCP2515_SPEED_500KBPS);
    PROP_ASSERT(err == MCP2515_OK);

    prop_status_byte = ARBI_UINT8(seed);
    uint8_t st = mcp2515_read_status(&dev);
    PROP_ASSERT(st == prop_status_byte);

    uint8_t eflg = ARBI_UINT8(seed);
    prop_regs[0x2D] = eflg;
    uint8_t ef = mcp2515_read_errors(&dev);
    PROP_ASSERT(ef == eflg);
}

/*
 * filter_mask_config: filter and mask writes in config mode must succeed.
 */
PROPERTY(filter_mask_config, 200)
{
    prop_mock_reset_state();
    mcp2515_t dev;
    mcp2515_err_t err = mcp2515_init(&dev, &prop_spi, MCP2515_SPEED_500KBPS);
    PROP_ASSERT(err == MCP2515_OK);

    err = mcp2515_set_mode(&dev, MCP2515_MODE_CONFIG);
    PROP_ASSERT(err == MCP2515_OK);

    uint8_t filter_num = (uint8_t)(ARBI_UINT8(seed) % 6u);
    uint32_t id = ARBI_UINT32(seed) & 0x7FFu;
    err = mcp2515_set_filter(&dev, filter_num, id, false);
    PROP_ASSERT(err == MCP2515_OK);

    uint8_t mask_num = (uint8_t)(ARBI_UINT8(seed) % 2u);
    uint32_t mask = ARBI_UINT32(seed) & 0x7FFu;
    err = mcp2515_set_mask(&dev, mask_num, mask, false);
    PROP_ASSERT(err == MCP2515_OK);
}

/*
 * invalid_speed_rejected: init with speed > 2 must return ERR_PARAM.
 */
PROPERTY(invalid_speed_rejected, 50)
{
    prop_mock_reset_state();
    mcp2515_t dev;
    uint8_t bad = (uint8_t)(4u + (ARBI_UINT8(seed) % 252u));
    mcp2515_err_t err = mcp2515_init(&dev, &prop_spi, (mcp2515_speed_t)bad);
    PROP_ASSERT(err == MCP2515_ERR_PARAM);
}

TEST_MAIN("prop_mcp2515")
    RUN(init_roundtrip);
    RUN(mode_transition);
    RUN(send_receive_roundtrip);
    RUN(status_errors_no_crash);
    RUN(filter_mask_config);
    RUN(invalid_speed_rejected);
TEST_MAIN_END()
