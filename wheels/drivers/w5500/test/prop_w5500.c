// SPDX-License-Identifier: MIT
/*
 * prop_w5500.c -- property-based tests for w5500
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_w5500.c ../w5500.c -o prop_w5500
 */

#include "test_harness.h"
#include "property_test.h"
#include <string.h>
#include "../w5500.h"

/* --- W5500 SPI mock (same structure as test_w5500.c) --- */

#define CTRL_READ   0x00U
#define CTRL_WRITE  0x04U
#define REG_MR       0x0000U
#define REG_SHAR0    0x0009U
#define REG_SIPR0    0x000FU
#define REG_VERSIONR 0x0039U
#define MR_RST       0x80U

#define COMMON_REG_SIZE 0x0040U
#define SOCK_REG_SIZE   0x0030U
#define MOCK_BUF_SIZE   2048U

#define SN_MR          0x0000U
#define SN_CR          0x0001U
#define SN_SR          0x0003U
#define SN_PORT0       0x0004U
#define SN_TXBUF_SIZE  0x001FU
#define SN_RXBUF_SIZE  0x001EU

static uint8_t mock_common_regs[COMMON_REG_SIZE];
static uint8_t mock_sock_regs[8][SOCK_REG_SIZE];
static uint8_t mock_sock_tx[8][MOCK_BUF_SIZE];
static uint8_t mock_sock_rx[8][MOCK_BUF_SIZE];

static enum {
    SPI_IDLE, SPI_GOT_ADDR_HI, SPI_GOT_ADDR_LO, SPI_DATA_PHASE,
} mock_spi_state;

static uint16_t mock_spi_addr;
static uint8_t  mock_spi_ctrl;
static uint16_t mock_spi_data_idx;
static uint8_t  mock_open_status[8];

static void mock_reset(void)
{
    memset(mock_common_regs, 0, sizeof(mock_common_regs));
    memset(mock_sock_regs, 0, sizeof(mock_sock_regs));
    memset(mock_sock_tx, 0, sizeof(mock_sock_tx));
    memset(mock_sock_rx, 0, sizeof(mock_sock_rx));
    mock_spi_state = SPI_IDLE;
    mock_spi_addr = 0;
    mock_spi_ctrl = 0;
    mock_spi_data_idx = 0;
    memset(mock_open_status, 0, sizeof(mock_open_status));
    mock_common_regs[REG_VERSIONR] = 0x04;
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_spi_state = SPI_IDLE;
    mock_spi_data_idx = 0;
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_spi_state = SPI_IDLE;
}

static void decode_bsb(uint8_t ctrl, int *block_type, int *sock_num)
{
    uint8_t bsb = (ctrl >> 3) & 0x1FU;
    if (bsb == 0) { *block_type = 0; *sock_num = -1; }
    else {
        int idx = (int)(bsb - 1);
        *sock_num = idx / 4;
        int sub = idx % 4;
        *block_type = (sub == 0) ? 1 : (sub == 1) ? 2 : 3;
    }
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    switch (mock_spi_state) {
    case SPI_IDLE:
        mock_spi_addr = (uint16_t)((uint16_t)tx << 8);
        mock_spi_state = SPI_GOT_ADDR_HI;
        return 0;
    case SPI_GOT_ADDR_HI:
        mock_spi_addr |= tx;
        mock_spi_state = SPI_GOT_ADDR_LO;
        return 0;
    case SPI_GOT_ADDR_LO:
        mock_spi_ctrl = tx;
        mock_spi_state = SPI_DATA_PHASE;
        mock_spi_data_idx = 0;
        return 0;
    case SPI_DATA_PHASE: {
        int bt, sn;
        decode_bsb(mock_spi_ctrl, &bt, &sn);
        uint16_t off = (uint16_t)(mock_spi_addr + mock_spi_data_idx);
        mock_spi_data_idx++;
        uint8_t wr = (mock_spi_ctrl & CTRL_WRITE) ? 1 : 0;
        if (bt == 0) {
            if (off < COMMON_REG_SIZE) {
                if (wr) {
                    if (off == REG_MR && (tx & MR_RST)) mock_common_regs[off] = 0;
                    else mock_common_regs[off] = tx;
                    return 0;
                }
                return mock_common_regs[off];
            }
            return 0;
        }
        if (bt == 1 && sn >= 0 && sn < 8) {
            if (off < SOCK_REG_SIZE) {
                if (wr) {
                    mock_sock_regs[sn][off] = tx;
                    if (off == SN_CR && tx != 0) {
                        mock_sock_regs[sn][SN_CR] = 0;
                        if (tx == 0x01 && mock_open_status[sn])
                            mock_sock_regs[sn][SN_SR] = mock_open_status[sn];
                        if (tx == 0x10)
                            mock_sock_regs[sn][SN_SR] = 0;
                    }
                    return 0;
                }
                return mock_sock_regs[sn][off];
            }
            return 0;
        }
        if (bt == 2 && sn >= 0 && sn < 8) {
            uint16_t bo = off % MOCK_BUF_SIZE;
            if (wr) { mock_sock_tx[sn][bo] = tx; return 0; }
            return mock_sock_tx[sn][bo];
        }
        if (bt == 3 && sn >= 0 && sn < 8) {
            uint16_t bo = off % MOCK_BUF_SIZE;
            if (wr) { mock_sock_rx[sn][bo] = tx; return 0; }
            return mock_sock_rx[sn][bo];
        }
        return 0;
    }
    default: return 0;
    }
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

static const w5500_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

static w5500_t g_dev;

static void init_dev(void)
{
    mock_reset();
    memset(&g_dev, 0, sizeof(g_dev));
    w5500_init(&g_dev, &mock_spi);
}

/* --- init_no_crash: init with any register state never crashes --- */
PROPERTY(init_no_crash, 5000) {
    mock_reset();
    /* Randomize version register to test both paths */
    mock_common_regs[REG_VERSIONR] = ARBI_UINT8(seed);
    w5500_t dev;
    memset(&dev, 0, sizeof(dev));
    (void)w5500_init(&dev, &mock_spi);
    PROP_ASSERT(1);
}

/* --- double_init_safe: calling init twice is safe --- */
PROPERTY(double_init_safe, 5000) {
    mock_reset();
    mock_common_regs[REG_VERSIONR] = 0x04;
    w5500_t dev;
    memset(&dev, 0, sizeof(dev));
    w5500_err_t e1 = w5500_init(&dev, &mock_spi);
    w5500_err_t e2 = w5500_init(&dev, &mock_spi);
    PROP_ASSERT_EQ(e1, e2);
    PROP_ASSERT_EQ(e1, W5500_OK);
}

/* --- set_mac_roundtrip: any MAC stored correctly in common regs --- */
PROPERTY(set_mac_roundtrip, 10000) {
    init_dev();
    uint8_t mac[6];
    ARBI_FILL(seed, mac, 6);

    w5500_err_t err = w5500_set_mac(&g_dev, mac);
    PROP_ASSERT(err == W5500_OK);

    /* Verify written to common registers */
    for (int i = 0; i < 6; i++) {
        PROP_ASSERT_EQ(mock_common_regs[REG_SHAR0 + i], mac[i]);
    }
    /* Verify stored in device struct */
    PROP_ASSERT_MEM_EQ(g_dev.mac_addr, mac, 6);
}

/* --- socket_invalid_always_rejected: socket >= 8 always fails --- */
PROPERTY(socket_invalid_rejected, 10000) {
    init_dev();
    uint8_t sock = (uint8_t)ARBI_RANGE(seed, 8, 255);
    PROP_ASSERT(w5500_socket_open(&g_dev, sock, W5500_PROTO_TCP, 80) == W5500_ERR_SOCK);
    PROP_ASSERT(w5500_socket_close(&g_dev, sock) == W5500_ERR_SOCK);

    uint8_t st;
    PROP_ASSERT(w5500_socket_status(&g_dev, sock, &st) == W5500_ERR_SOCK);
}

TEST_MAIN("w5500 properties") {
    RUN(init_no_crash);
    RUN(double_init_safe);
    RUN(set_mac_roundtrip);
    RUN(socket_invalid_rejected);
}
TEST_MAIN_END()
