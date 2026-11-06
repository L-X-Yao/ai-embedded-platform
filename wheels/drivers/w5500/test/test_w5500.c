// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "w5500.h"

/* ---------- mock SPI ---------- */

/*
 * W5500 SPI frame: [addr_hi][addr_lo][control][data...]
 * Control byte: BSB[7:3] | RWB[2] | OM[1:0]
 *   RWB=0 read, RWB=1 write; OM=00 variable-length
 */

#define CTRL_READ   0x00U
#define CTRL_WRITE  0x04U

#define BSB_COMMON         (0x00U << 3)
#define BSB_SOCK_REG(n)    ((uint8_t)(((n) * 4U + 1U) << 3))
#define BSB_SOCK_TX(n)     ((uint8_t)(((n) * 4U + 2U) << 3))
#define BSB_SOCK_RX(n)     ((uint8_t)(((n) * 4U + 3U) << 3))

/* Common register addresses */
#define REG_MR         0x0000U
#define REG_GAR0       0x0001U
#define REG_SUBR0      0x0005U
#define REG_SHAR0      0x0009U
#define REG_SIPR0      0x000FU
#define REG_VERSIONR   0x0039U

/* Socket register offsets */
#define SN_MR          0x0000U
#define SN_CR          0x0001U
#define SN_IR          0x0002U
#define SN_SR          0x0003U
#define SN_PORT0       0x0004U
#define SN_PORT1       0x0005U
#define SN_TXBUF_SIZE  0x001FU
#define SN_RXBUF_SIZE  0x001EU
#define SN_TX_FSR0     0x0020U
#define SN_TX_FSR1     0x0021U
#define SN_TX_WR0      0x0024U
#define SN_TX_WR1      0x0025U
#define SN_RX_RSR0     0x0026U
#define SN_RX_RSR1     0x0027U
#define SN_RX_RD0      0x0028U
#define SN_RX_RD1      0x0029U

#define MR_RST         0x80U

/* Sn_IR bits, W5500 Datasheet 1.1.0 p48 */
#define IR_SEND_OK     0x10U
#define IR_TIMEOUT     0x08U
#define IR_RECV        0x04U

/* Mock register storage */
#define COMMON_REG_SIZE 0x0040U
#define SOCK_REG_SIZE   0x0030U
#define SOCK_BUF_SIZE   2048U

static uint8_t mock_common_regs[COMMON_REG_SIZE];
static uint8_t mock_sock_regs[8][SOCK_REG_SIZE];
static uint8_t mock_sock_tx[8][SOCK_BUF_SIZE];
static uint8_t mock_sock_rx[8][SOCK_BUF_SIZE];

/* SPI transaction state machine */
static enum {
    SPI_IDLE,
    SPI_GOT_ADDR_HI,
    SPI_GOT_ADDR_LO,
    SPI_DATA_PHASE,
} mock_spi_state;

static uint16_t mock_spi_addr;
static uint8_t  mock_spi_ctrl;
static uint16_t mock_spi_data_idx;

/* Tracking */
static int mock_delay_calls;
static int mock_cs_low_calls;
static int mock_cs_high_calls;

/* Post-command hooks: simulate Sn_CR clearing and status transitions */
static int mock_cmd_clear_delay;  /* how many reads of Sn_CR before it clears */
static int mock_cmd_read_count[8];

/* For simulating open: what status to set after OPEN command */
static uint8_t mock_open_status[8];

/* Sn_TX_FSR and Sn_RX_RSR can change during a 16-bit read (W5500 Datasheet
 * 1.1.0 p54, p55): the next mock_tear_left[n] reads of either one see a high
 * byte that is not the register's */
static int     mock_tear_left[8];
static uint8_t mock_tear_hi;

/* Sn_SR moves after Sn_CR has cleared (W5500 Datasheet 1.1.0 p46: "the command
 * is still being processed"): the first mock_sr_lag reads of Sn_SR after a
 * command still show the old status */
static int     mock_sr_lag;
static int     mock_sr_left[8];
static uint8_t mock_sr_next[8];
static uint8_t mock_sr_at_open[8];   /* Sn_SR when the OPEN command arrived */

/* How a SEND ends: the Sn_IR bits it sets, on read number mock_ir_lag of Sn_IR
 * after Sn_CR has cleared (0: at once) */
static uint8_t mock_send_ir[8];
static int     mock_ir_lag;
static int     mock_ir_left[8];
static int     mock_ir_reads[8];

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
    mock_delay_calls = 0;
    mock_cs_low_calls = 0;
    mock_cs_high_calls = 0;
    mock_cmd_clear_delay = 0;
    memset(mock_cmd_read_count, 0, sizeof(mock_cmd_read_count));
    memset(mock_open_status, 0, sizeof(mock_open_status));
    memset(mock_tear_left, 0, sizeof(mock_tear_left));
    mock_tear_hi = 0;
    mock_sr_lag = 0;
    memset(mock_sr_left, 0, sizeof(mock_sr_left));
    memset(mock_sr_next, 0, sizeof(mock_sr_next));
    memset(mock_sr_at_open, 0xEE, sizeof(mock_sr_at_open));
    memset(mock_send_ir, IR_SEND_OK, sizeof(mock_send_ir));
    mock_ir_lag = 0;
    memset(mock_ir_left, 0, sizeof(mock_ir_left));
    memset(mock_ir_reads, 0, sizeof(mock_ir_reads));

    /* Set version register */
    mock_common_regs[REG_VERSIONR] = 0x04;
}

static void mock_set_ir(int sock, uint8_t bits)
{
    mock_sock_regs[sock][SN_IR] = (uint8_t)(mock_sock_regs[sock][SN_IR] | bits);
}

/* Sn_CR has cleared: apply what the command does to Sn_SR and Sn_IR, after
 * the lag */
static void mock_cmd_done(int sock, uint8_t cmd)
{
    uint8_t next;

    if (cmd == 0x20) {          /* SEND */
        if (mock_ir_lag == 0) {
            mock_set_ir(sock, mock_send_ir[sock]);
        } else {
            mock_ir_left[sock] = mock_ir_lag;
        }
        return;
    }

    if (cmd == 0x01) {          /* OPEN */
        mock_sr_at_open[sock] = mock_sock_regs[sock][SN_SR];
        if (!mock_open_status[sock]) return;
        next = mock_open_status[sock];
    } else if (cmd == 0x10) {   /* CLOSE */
        next = 0x00;
    } else {
        return;
    }

    if (mock_sr_lag == 0) {
        mock_sock_regs[sock][SN_SR] = next;
    } else {
        mock_sr_next[sock] = next;
        mock_sr_left[sock] = mock_sr_lag;
    }
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_spi_state = SPI_IDLE;
    mock_spi_data_idx = 0;
    mock_cs_low_calls++;
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_spi_state = SPI_IDLE;
    mock_cs_high_calls++;
}

/* Decode which block/buffer a BSB refers to */
static void decode_bsb(uint8_t ctrl, int *block_type, int *sock_num)
{
    uint8_t bsb = (ctrl >> 3) & 0x1FU;
    if (bsb == 0) {
        *block_type = 0;  /* common */
        *sock_num = -1;
    } else {
        /* bsb = sock*4 + offset: offset 1=reg, 2=tx, 3=rx */
        int idx = (int)(bsb - 1);
        *sock_num = idx / 4;
        int sub = idx % 4;
        if (sub == 0)      *block_type = 1; /* socket register */
        else if (sub == 1) *block_type = 2; /* socket TX buffer */
        else               *block_type = 3; /* socket RX buffer */
    }
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    switch (mock_spi_state) {
    case SPI_IDLE:
        mock_spi_addr = (uint16_t)((uint16_t)tx << 8);
        mock_spi_state = SPI_GOT_ADDR_HI;
        return 0x00U;

    case SPI_GOT_ADDR_HI:
        mock_spi_addr = (uint16_t)(mock_spi_addr | tx);
        mock_spi_state = SPI_GOT_ADDR_LO;
        return 0x00U;

    case SPI_GOT_ADDR_LO:
        mock_spi_ctrl = tx;
        mock_spi_state = SPI_DATA_PHASE;
        mock_spi_data_idx = 0;
        return 0x00U;

    case SPI_DATA_PHASE: {
        int block_type, sock_num;
        decode_bsb(mock_spi_ctrl, &block_type, &sock_num);
        uint16_t offset = (uint16_t)(mock_spi_addr + mock_spi_data_idx);
        mock_spi_data_idx++;

        uint8_t is_write = (mock_spi_ctrl & CTRL_WRITE) ? 1 : 0;

        if (block_type == 0) {
            /* Common registers */
            if (offset < COMMON_REG_SIZE) {
                if (is_write) {
                    /* Handle MR reset bit: clear after write */
                    if (offset == REG_MR && (tx & MR_RST)) {
                        mock_common_regs[offset] = 0; /* RST auto-clears */
                    } else {
                        mock_common_regs[offset] = tx;
                    }
                    return 0x00U;
                }
                return mock_common_regs[offset];
            }
            return is_write ? 0x00U : 0x00U;
        }

        if (block_type == 1 && sock_num >= 0 && sock_num < 8) {
            /* Socket registers */
            if (offset < SOCK_REG_SIZE) {
                if (is_write) {
                    if (offset == SN_IR) {
                        /* RCW1, p48: "the host should write the bit to '1'" */
                        mock_sock_regs[sock_num][SN_IR] =
                            (uint8_t)(mock_sock_regs[sock_num][SN_IR] & ~tx);
                        return 0x00U;
                    }
                    mock_sock_regs[sock_num][offset] = tx;
                    /* Simulate command completion */
                    if (offset == SN_CR && tx != 0) {
                        if (mock_cmd_clear_delay == 0) {
                            /* Immediate clear */
                            mock_sock_regs[sock_num][SN_CR] = 0x00;
                            mock_cmd_done(sock_num, tx);
                        } else {
                            /* Delayed clear: increment counter */
                            mock_cmd_read_count[sock_num] = 0;
                        }
                    }
                    return 0x00U;
                }
                /* Reading Sn_CR with delayed clear */
                if (offset == SN_CR && mock_cmd_clear_delay > 0 &&
                    mock_sock_regs[sock_num][SN_CR] != 0) {
                    mock_cmd_read_count[sock_num]++;
                    if (mock_cmd_read_count[sock_num] >= mock_cmd_clear_delay) {
                        uint8_t saved_cmd = mock_sock_regs[sock_num][SN_CR];
                        mock_sock_regs[sock_num][SN_CR] = 0x00;
                        mock_cmd_done(sock_num, saved_cmd);
                        return 0x00U;
                    }
                    return mock_sock_regs[sock_num][SN_CR];
                }
                if (offset == SN_IR) {
                    mock_ir_reads[sock_num]++;
                    if (mock_ir_left[sock_num] > 0 &&
                        --mock_ir_left[sock_num] == 0) {
                        mock_set_ir(sock_num, mock_send_ir[sock_num]);
                    }
                }
                if (offset == SN_SR && mock_sr_left[sock_num] > 0) {
                    uint8_t shown = mock_sock_regs[sock_num][SN_SR];
                    if (--mock_sr_left[sock_num] == 0) {
                        mock_sock_regs[sock_num][SN_SR] = mock_sr_next[sock_num];
                    }
                    return shown;
                }
                if ((offset == SN_TX_FSR0 || offset == SN_RX_RSR0) &&
                    mock_tear_left[sock_num] > 0) {
                    mock_tear_left[sock_num]--;
                    /* changes on every read, so a long run never settles */
                    return (uint8_t)(mock_tear_hi ^
                                     (uint8_t)mock_tear_left[sock_num]);
                }
                return mock_sock_regs[sock_num][offset];
            }
            return is_write ? 0x00U : 0x00U;
        }

        if (block_type == 2 && sock_num >= 0 && sock_num < 8) {
            /* Socket TX buffer */
            uint16_t buf_offset = offset % SOCK_BUF_SIZE;
            if (is_write) {
                mock_sock_tx[sock_num][buf_offset] = tx;
                return 0x00U;
            }
            return mock_sock_tx[sock_num][buf_offset];
        }

        if (block_type == 3 && sock_num >= 0 && sock_num < 8) {
            /* Socket RX buffer */
            uint16_t buf_offset = offset % SOCK_BUF_SIZE;
            if (is_write) {
                mock_sock_rx[sock_num][buf_offset] = tx;
                return 0x00U;
            }
            return mock_sock_rx[sock_num][buf_offset];
        }

        return 0x00U;
    }

    default:
        return 0x00U;
    }
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
    mock_delay_calls++;
}

static const w5500_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

static w5500_t dev;

static void init_dev(void)
{
    mock_reset();
    memset(&dev, 0, sizeof(dev));
    w5500_init(&dev, &mock_spi);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    memset(&dev, 0, sizeof(dev));
    w5500_err_t e = w5500_init(&dev, &mock_spi);
    ASSERT_EQ(e, W5500_OK);
    /* delay_ms should have been called after reset */
    ASSERT_TRUE(mock_delay_calls > 0);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(w5500_init(NULL, &mock_spi), W5500_ERR_PARAM);
}

TEST(test_init_null_spi)
{
    w5500_t d;
    ASSERT_EQ(w5500_init(&d, NULL), W5500_ERR_PARAM);
}

TEST(test_init_spi_missing_cs_low)
{
    w5500_spi_t bad = mock_spi;
    bad.cs_low = NULL;
    w5500_t d;
    ASSERT_EQ(w5500_init(&d, &bad), W5500_ERR_PARAM);
}

TEST(test_init_spi_missing_cs_high)
{
    w5500_spi_t bad = mock_spi;
    bad.cs_high = NULL;
    w5500_t d;
    ASSERT_EQ(w5500_init(&d, &bad), W5500_ERR_PARAM);
}

TEST(test_init_spi_missing_xfer)
{
    w5500_spi_t bad = mock_spi;
    bad.xfer = NULL;
    w5500_t d;
    ASSERT_EQ(w5500_init(&d, &bad), W5500_ERR_PARAM);
}

TEST(test_init_bad_version)
{
    mock_reset();
    mock_common_regs[REG_VERSIONR] = 0xFF; /* wrong version */
    memset(&dev, 0, sizeof(dev));
    w5500_err_t e = w5500_init(&dev, &mock_spi);
    ASSERT_EQ(e, W5500_ERR_SPI);
}

TEST(test_init_configures_buffer_sizes)
{
    init_dev();
    /* Each socket should have TXBUF=2 and RXBUF=2 */
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(mock_sock_regs[i][SN_TXBUF_SIZE], 2);
        ASSERT_EQ(mock_sock_regs[i][SN_RXBUF_SIZE], 2);
    }
}

/* ---------- version test ---------- */

TEST(test_read_version)
{
    init_dev();
    uint8_t ver = w5500_read_version(&dev);
    ASSERT_EQ(ver, 0x04);
}

TEST(test_read_version_null)
{
    ASSERT_EQ(w5500_read_version(NULL), 0);
}

/* ---------- MAC tests ---------- */

TEST(test_set_mac)
{
    init_dev();
    const uint8_t mac[6] = {0x02, 0x00, 0xDE, 0xAD, 0xBE, 0xEF};
    w5500_err_t e = w5500_set_mac(&dev, mac);
    ASSERT_EQ(e, W5500_OK);
    /* Check stored in device struct */
    ASSERT_EQ(dev.mac_addr[0], 0x02);
    ASSERT_EQ(dev.mac_addr[5], 0xEF);
    /* Check written to common registers */
    ASSERT_EQ(mock_common_regs[REG_SHAR0],     0x02);
    ASSERT_EQ(mock_common_regs[REG_SHAR0 + 1], 0x00);
    ASSERT_EQ(mock_common_regs[REG_SHAR0 + 2], 0xDE);
    ASSERT_EQ(mock_common_regs[REG_SHAR0 + 3], 0xAD);
    ASSERT_EQ(mock_common_regs[REG_SHAR0 + 4], 0xBE);
    ASSERT_EQ(mock_common_regs[REG_SHAR0 + 5], 0xEF);
}

TEST(test_set_mac_null_dev)
{
    const uint8_t mac[6] = {0};
    ASSERT_EQ(w5500_set_mac(NULL, mac), W5500_ERR_PARAM);
}

TEST(test_set_mac_null_mac)
{
    init_dev();
    ASSERT_EQ(w5500_set_mac(&dev, NULL), W5500_ERR_PARAM);
}

/* ---------- IP tests ---------- */

TEST(test_set_ip)
{
    init_dev();
    const uint8_t ip[4]      = {192, 168, 1, 100};
    const uint8_t subnet[4]  = {255, 255, 255, 0};
    const uint8_t gateway[4] = {192, 168, 1, 1};

    w5500_err_t e = w5500_set_ip(&dev, ip, subnet, gateway);
    ASSERT_EQ(e, W5500_OK);

    /* Gateway */
    ASSERT_EQ(mock_common_regs[REG_GAR0],     192);
    ASSERT_EQ(mock_common_regs[REG_GAR0 + 1], 168);
    ASSERT_EQ(mock_common_regs[REG_GAR0 + 2], 1);
    ASSERT_EQ(mock_common_regs[REG_GAR0 + 3], 1);

    /* Subnet */
    ASSERT_EQ(mock_common_regs[REG_SUBR0],     255);
    ASSERT_EQ(mock_common_regs[REG_SUBR0 + 1], 255);
    ASSERT_EQ(mock_common_regs[REG_SUBR0 + 2], 255);
    ASSERT_EQ(mock_common_regs[REG_SUBR0 + 3], 0);

    /* IP */
    ASSERT_EQ(mock_common_regs[REG_SIPR0],     192);
    ASSERT_EQ(mock_common_regs[REG_SIPR0 + 1], 168);
    ASSERT_EQ(mock_common_regs[REG_SIPR0 + 2], 1);
    ASSERT_EQ(mock_common_regs[REG_SIPR0 + 3], 100);
}

TEST(test_set_ip_null_dev)
{
    const uint8_t ip[4] = {0}, sub[4] = {0}, gw[4] = {0};
    ASSERT_EQ(w5500_set_ip(NULL, ip, sub, gw), W5500_ERR_PARAM);
}

TEST(test_set_ip_null_ip)
{
    init_dev();
    const uint8_t sub[4] = {0}, gw[4] = {0};
    ASSERT_EQ(w5500_set_ip(&dev, NULL, sub, gw), W5500_ERR_PARAM);
}

TEST(test_set_ip_null_subnet)
{
    init_dev();
    const uint8_t ip[4] = {0}, gw[4] = {0};
    ASSERT_EQ(w5500_set_ip(&dev, ip, NULL, gw), W5500_ERR_PARAM);
}

TEST(test_set_ip_null_gateway)
{
    init_dev();
    const uint8_t ip[4] = {0}, sub[4] = {0};
    ASSERT_EQ(w5500_set_ip(&dev, ip, sub, NULL), W5500_ERR_PARAM);
}

/* ---------- socket open tests ---------- */

TEST(test_socket_open_tcp)
{
    init_dev();
    mock_open_status[0] = W5500_SOCK_INIT;

    w5500_err_t e = w5500_socket_open(&dev, 0, W5500_PROTO_TCP, 5000);
    ASSERT_EQ(e, W5500_OK);
    ASSERT_EQ(mock_sock_regs[0][SN_MR], W5500_PROTO_TCP);
    /* Port: big-endian 5000 = 0x1388 */
    ASSERT_EQ(mock_sock_regs[0][SN_PORT0], 0x13);
    ASSERT_EQ(mock_sock_regs[0][SN_PORT1], 0x88);
}

TEST(test_socket_open_udp)
{
    init_dev();
    mock_open_status[2] = W5500_SOCK_UDP;

    w5500_err_t e = w5500_socket_open(&dev, 2, W5500_PROTO_UDP, 8080);
    ASSERT_EQ(e, W5500_OK);
    ASSERT_EQ(mock_sock_regs[2][SN_MR], W5500_PROTO_UDP);
}

TEST(test_socket_open_macraw)
{
    init_dev();
    mock_open_status[0] = W5500_SOCK_MACRAW;

    w5500_err_t e = w5500_socket_open(&dev, 0, W5500_PROTO_MACRAW, 0);
    ASSERT_EQ(e, W5500_OK);
}

TEST(test_socket_open_macraw_only_socket_0)
{
    /* W5500 Datasheet 1.1.0 p45: "MACRAW mode should be only used in Socket 0." */
    for (uint8_t sock = 1; sock < 8; sock++) {
        init_dev();
        mock_open_status[sock] = W5500_SOCK_MACRAW;
        ASSERT_EQ(w5500_socket_open(&dev, sock, W5500_PROTO_MACRAW, 0),
                  W5500_ERR_SOCK);
        ASSERT_EQ(mock_sock_regs[sock][SN_MR], 0);
        ASSERT_EQ(mock_sock_regs[sock][SN_SR], W5500_SOCK_CLOSED);
    }
}

TEST(test_socket_open_invalid_sock)
{
    init_dev();
    ASSERT_EQ(w5500_socket_open(&dev, 8, W5500_PROTO_TCP, 80), W5500_ERR_SOCK);
}

TEST(test_socket_open_invalid_protocol)
{
    init_dev();
    ASSERT_EQ(w5500_socket_open(&dev, 0, 0xFF, 80), W5500_ERR_PARAM);
}

TEST(test_socket_open_null_dev)
{
    ASSERT_EQ(w5500_socket_open(NULL, 0, W5500_PROTO_TCP, 80), W5500_ERR_PARAM);
}

TEST(test_socket_open_wrong_status)
{
    init_dev();
    /* After OPEN with TCP, status should be SOCK_INIT.
     * Set mock to return SOCK_CLOSED instead → ERR_SOCK */
    mock_open_status[0] = W5500_SOCK_CLOSED;

    w5500_err_t e = w5500_socket_open(&dev, 0, W5500_PROTO_TCP, 80);
    ASSERT_EQ(e, W5500_ERR_SOCK);
}

TEST(test_socket_open_waits_for_status)
{
    init_dev();
    /* W5500 Datasheet 1.1.0 p46: "Even though Sn_CR is cleared to 0x00, the
     * command is still being processed. ... please check the Sn_IR or Sn_SR." */
    mock_open_status[0] = W5500_SOCK_INIT;
    mock_sr_lag = 3;

    ASSERT_EQ(w5500_socket_open(&dev, 0, W5500_PROTO_TCP, 5000), W5500_OK);
    ASSERT_EQ(mock_sock_regs[0][SN_SR], W5500_SOCK_INIT);
}

TEST(test_socket_open_finishes_close_before_open)
{
    init_dev();
    mock_sock_regs[0][SN_SR] = W5500_SOCK_UDP;    /* already open */
    mock_open_status[0] = W5500_SOCK_INIT;
    mock_sr_lag = 2;

    ASSERT_EQ(w5500_socket_open(&dev, 0, W5500_PROTO_TCP, 5000), W5500_OK);
    ASSERT_EQ(mock_sr_at_open[0], W5500_SOCK_CLOSED);
}

TEST(test_socket_open_close_never_finishes)
{
    init_dev();
    mock_sock_regs[0][SN_SR] = W5500_SOCK_UDP;
    mock_sr_lag = 999999;

    ASSERT_EQ(w5500_socket_open(&dev, 0, W5500_PROTO_TCP, 5000),
              W5500_ERR_TIMEOUT);
    ASSERT_EQ(mock_sr_at_open[0], 0xEE);          /* OPEN was never sent */
}

/* ---------- socket close tests ---------- */

TEST(test_socket_close_waits_for_closed)
{
    init_dev();
    mock_sock_regs[0][SN_SR] = W5500_SOCK_UDP;
    mock_sr_lag = 2;

    ASSERT_EQ(w5500_socket_close(&dev, 0), W5500_OK);
    ASSERT_EQ(mock_sock_regs[0][SN_SR], W5500_SOCK_CLOSED);
}

TEST(test_socket_close_status_never_changes)
{
    init_dev();
    mock_sock_regs[0][SN_SR] = W5500_SOCK_UDP;
    mock_sr_lag = 999999;

    ASSERT_EQ(w5500_socket_close(&dev, 0), W5500_ERR_TIMEOUT);
}

TEST(test_socket_close)
{
    init_dev();
    mock_open_status[0] = W5500_SOCK_INIT;
    w5500_socket_open(&dev, 0, W5500_PROTO_TCP, 80);

    w5500_err_t e = w5500_socket_close(&dev, 0);
    ASSERT_EQ(e, W5500_OK);
    ASSERT_EQ(mock_sock_regs[0][SN_SR], W5500_SOCK_CLOSED);
}

TEST(test_socket_close_invalid_sock)
{
    init_dev();
    ASSERT_EQ(w5500_socket_close(&dev, 8), W5500_ERR_SOCK);
}

TEST(test_socket_close_null_dev)
{
    ASSERT_EQ(w5500_socket_close(NULL, 0), W5500_ERR_PARAM);
}

/* ---------- socket send tests ---------- */

TEST(test_socket_send)
{
    init_dev();
    /* Set TX free size to 2048 (sufficient) */
    mock_sock_regs[0][SN_TX_FSR0] = 0x08; /* 2048 = 0x0800 */
    mock_sock_regs[0][SN_TX_FSR1] = 0x00;
    /* TX write pointer at 0 */
    mock_sock_regs[0][SN_TX_WR0] = 0x00;
    mock_sock_regs[0][SN_TX_WR1] = 0x00;

    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    w5500_err_t e = w5500_socket_send(&dev, 0, data, 4);
    ASSERT_EQ(e, W5500_OK);
    /* Data should be in TX buffer */
    ASSERT_EQ(mock_sock_tx[0][0], 0xDE);
    ASSERT_EQ(mock_sock_tx[0][1], 0xAD);
    ASSERT_EQ(mock_sock_tx[0][2], 0xBE);
    ASSERT_EQ(mock_sock_tx[0][3], 0xEF);
    /* TX write pointer should be advanced by 4 */
    ASSERT_EQ(mock_sock_regs[0][SN_TX_WR0], 0x00);
    ASSERT_EQ(mock_sock_regs[0][SN_TX_WR1], 0x04);
}

TEST(test_socket_send_null_data)
{
    init_dev();
    ASSERT_EQ(w5500_socket_send(&dev, 0, NULL, 4), W5500_ERR_PARAM);
}

TEST(test_socket_send_zero_len)
{
    init_dev();
    uint8_t d = 0;
    ASSERT_EQ(w5500_socket_send(&dev, 0, &d, 0), W5500_ERR_SIZE);
}

TEST(test_socket_send_oversized)
{
    init_dev();
    uint8_t d = 0;
    ASSERT_EQ(w5500_socket_send(&dev, 0, &d, (uint16_t)(W5500_SOCK_BUF_SIZE + 1)), W5500_ERR_SIZE);
}

TEST(test_socket_send_insufficient_free)
{
    init_dev();
    /* Set TX free size to 2 (too small for 100 bytes) */
    mock_sock_regs[0][SN_TX_FSR0] = 0x00;
    mock_sock_regs[0][SN_TX_FSR1] = 0x02;

    uint8_t data[100];
    memset(data, 0xAA, sizeof(data));
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 100), W5500_ERR_SIZE);
}

static void set_free_size_2048(uint8_t sock)
{
    mock_sock_regs[sock][SN_TX_FSR0] = 0x08;
    mock_sock_regs[sock][SN_TX_FSR1] = 0x00;
}

TEST(test_socket_send_reports_chip_timeout)
{
    init_dev();
    set_free_size_2048(0);
    /* W5500 Datasheet 1.1.0 p48: TIMEOUT "is issued when ARPTO or TCPTO
     * occurs".  The host clears it by writing 1. */
    mock_send_ir[0] = IR_TIMEOUT;

    uint8_t data[4] = {1, 2, 3, 4};
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_ERR_TIMEOUT);
    ASSERT_EQ(mock_sock_regs[0][SN_IR] & IR_TIMEOUT, 0);
}

TEST(test_socket_send_waits_for_send_ok)
{
    init_dev();
    set_free_size_2048(0);
    /* p46: Sn_CR clearing does not end the command.  p48: SEND_OK "is issued
     * when SEND command is completed". */
    mock_ir_lag = 3;

    uint8_t data[4] = {1, 2, 3, 4};
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_OK);
    ASSERT_EQ(mock_ir_reads[0], 3);
    ASSERT_EQ(mock_sock_regs[0][SN_IR] & IR_SEND_OK, 0);
}

TEST(test_socket_send_no_result_times_out)
{
    init_dev();
    set_free_size_2048(0);
    mock_send_ir[0] = 0;   /* neither SEND_OK nor TIMEOUT ever comes */

    uint8_t data[4] = {1, 2, 3, 4};
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_ERR_TIMEOUT);
}

TEST(test_socket_send_ignores_stale_send_ok)
{
    init_dev();
    set_free_size_2048(0);
    /* a SEND_OK left from an earlier SEND must not pass for this one */
    mock_sock_regs[0][SN_IR] = IR_SEND_OK;
    mock_send_ir[0] = IR_TIMEOUT;

    uint8_t data[4] = {1, 2, 3, 4};
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_ERR_TIMEOUT);
}

TEST(test_socket_send_keeps_other_ir_bits)
{
    init_dev();
    set_free_size_2048(0);
    mock_sock_regs[0][SN_IR] = IR_RECV;

    uint8_t data[4] = {1, 2, 3, 4};
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_OK);
    ASSERT_EQ(mock_sock_regs[0][SN_IR], IR_RECV);
}

TEST(test_socket_send_rereads_torn_free_size)
{
    init_dev();
    /* W5500 Datasheet 1.1.0 p54: "read all 16-bits twice or more until
     * getting the same value".  Really 2 bytes free, first read shows 0x0702. */
    mock_sock_regs[0][SN_TX_FSR0] = 0x00;
    mock_sock_regs[0][SN_TX_FSR1] = 0x02;
    mock_tear_hi = 0x07;
    mock_tear_left[0] = 1;

    uint8_t data[100];
    memset(data, 0xAA, sizeof(data));
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 100), W5500_ERR_SIZE);
    ASSERT_EQ(mock_sock_tx[0][0], 0);
}

TEST(test_socket_send_free_size_never_settles)
{
    init_dev();
    mock_sock_regs[0][SN_TX_FSR0] = 0x08;
    mock_tear_left[0] = 999999;

    uint8_t data[4] = {1, 2, 3, 4};
    ASSERT_EQ(w5500_socket_send(&dev, 0, data, 4), W5500_ERR_TIMEOUT);
    ASSERT_EQ(mock_sock_tx[0][0], 0);
}

TEST(test_socket_send_invalid_sock)
{
    init_dev();
    uint8_t d = 0;
    ASSERT_EQ(w5500_socket_send(&dev, 8, &d, 1), W5500_ERR_SOCK);
}

TEST(test_socket_send_null_dev)
{
    uint8_t d = 0;
    ASSERT_EQ(w5500_socket_send(NULL, 0, &d, 1), W5500_ERR_PARAM);
}

/* ---------- socket recv tests ---------- */

TEST(test_socket_recv)
{
    init_dev();
    /* Place data in RX buffer */
    mock_sock_rx[0][0] = 0x11;
    mock_sock_rx[0][1] = 0x22;
    mock_sock_rx[0][2] = 0x33;
    mock_sock_rx[0][3] = 0x44;

    /* Set RX received size = 4 */
    mock_sock_regs[0][SN_RX_RSR0] = 0x00;
    mock_sock_regs[0][SN_RX_RSR1] = 0x04;
    /* RX read pointer at 0 */
    mock_sock_regs[0][SN_RX_RD0] = 0x00;
    mock_sock_regs[0][SN_RX_RD1] = 0x00;

    uint8_t buf[64];
    uint16_t out_len = 0;
    w5500_err_t e = w5500_socket_recv(&dev, 0, buf, sizeof(buf), &out_len);
    ASSERT_EQ(e, W5500_OK);
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ(buf[0], 0x11);
    ASSERT_EQ(buf[1], 0x22);
    ASSERT_EQ(buf[2], 0x33);
    ASSERT_EQ(buf[3], 0x44);
}

TEST(test_socket_recv_clamped)
{
    init_dev();
    /* RX has 100 bytes but buffer is only 10 */
    mock_sock_regs[0][SN_RX_RSR0] = 0x00;
    mock_sock_regs[0][SN_RX_RSR1] = 100;
    mock_sock_regs[0][SN_RX_RD0] = 0x00;
    mock_sock_regs[0][SN_RX_RD1] = 0x00;
    for (int i = 0; i < 100; i++) mock_sock_rx[0][i] = (uint8_t)i;

    uint8_t buf[10];
    uint16_t out_len = 0;
    w5500_err_t e = w5500_socket_recv(&dev, 0, buf, 10, &out_len);
    ASSERT_EQ(e, W5500_OK);
    ASSERT_EQ(out_len, 10);
    ASSERT_EQ(buf[0], 0);
    ASSERT_EQ(buf[9], 9);
}

TEST(test_socket_recv_rereads_torn_size)
{
    init_dev();
    /* W5500 Datasheet 1.1.0 p55: same note as p54.  4 bytes are really
     * there, and the first read shows 0x0104. */
    mock_sock_regs[0][SN_RX_RSR0] = 0x00;
    mock_sock_regs[0][SN_RX_RSR1] = 0x04;
    mock_tear_hi = 0x01;
    mock_tear_left[0] = 1;

    uint8_t buf[512];
    uint16_t out_len = 0;
    ASSERT_EQ(w5500_socket_recv(&dev, 0, buf, sizeof(buf), &out_len), W5500_OK);
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ(mock_sock_regs[0][SN_RX_RD1], 4);
}

TEST(test_socket_recv_size_never_settles)
{
    init_dev();
    mock_sock_regs[0][SN_RX_RSR1] = 0x04;
    mock_tear_left[0] = 999999;

    uint8_t buf[8];
    uint16_t out_len = 99;
    ASSERT_EQ(w5500_socket_recv(&dev, 0, buf, sizeof(buf), &out_len),
              W5500_ERR_TIMEOUT);
    ASSERT_EQ(out_len, 99);
    ASSERT_EQ(mock_sock_regs[0][SN_RX_RD1], 0);
}

TEST(test_socket_recv_zero_buf_len)
{
    init_dev();
    /* W5500 Datasheet 1.1.0 p17: variable length mode has "N-Bytes Data
     * Phase (1 <= N)", so a read of 0 bytes is not a legal frame */
    mock_sock_regs[0][SN_RX_RSR0] = 0x00;
    mock_sock_regs[0][SN_RX_RSR1] = 0x04;

    uint8_t buf[4];
    uint16_t out_len = 99;
    int cs_before = mock_cs_low_calls;
    ASSERT_EQ(w5500_socket_recv(&dev, 0, buf, 0, &out_len), W5500_ERR_PARAM);
    ASSERT_EQ(mock_cs_low_calls, cs_before);
    ASSERT_EQ(out_len, 99);
}

TEST(test_socket_recv_no_data)
{
    init_dev();
    mock_sock_regs[0][SN_RX_RSR0] = 0x00;
    mock_sock_regs[0][SN_RX_RSR1] = 0x00;

    uint8_t buf[64];
    uint16_t out_len = 0;
    ASSERT_EQ(w5500_socket_recv(&dev, 0, buf, sizeof(buf), &out_len), W5500_ERR_NO_DATA);
}

TEST(test_socket_recv_null_buf)
{
    init_dev();
    uint16_t out_len;
    ASSERT_EQ(w5500_socket_recv(&dev, 0, NULL, 64, &out_len), W5500_ERR_PARAM);
}

TEST(test_socket_recv_null_out_len)
{
    init_dev();
    uint8_t buf[64];
    ASSERT_EQ(w5500_socket_recv(&dev, 0, buf, sizeof(buf), NULL), W5500_ERR_PARAM);
}

TEST(test_socket_recv_invalid_sock)
{
    init_dev();
    uint8_t buf[64];
    uint16_t out_len;
    ASSERT_EQ(w5500_socket_recv(&dev, 8, buf, sizeof(buf), &out_len), W5500_ERR_SOCK);
}

TEST(test_socket_recv_null_dev)
{
    uint8_t buf[64];
    uint16_t out_len;
    ASSERT_EQ(w5500_socket_recv(NULL, 0, buf, sizeof(buf), &out_len), W5500_ERR_PARAM);
}

/* ---------- socket status tests ---------- */

TEST(test_socket_status)
{
    init_dev();
    mock_sock_regs[0][SN_SR] = W5500_SOCK_ESTABLISHED;

    uint8_t st;
    w5500_err_t e = w5500_socket_status(&dev, 0, &st);
    ASSERT_EQ(e, W5500_OK);
    ASSERT_EQ(st, W5500_SOCK_ESTABLISHED);
}

TEST(test_socket_status_null_status)
{
    init_dev();
    ASSERT_EQ(w5500_socket_status(&dev, 0, NULL), W5500_ERR_PARAM);
}

TEST(test_socket_status_invalid_sock)
{
    init_dev();
    uint8_t st;
    ASSERT_EQ(w5500_socket_status(&dev, 8, &st), W5500_ERR_SOCK);
}

TEST(test_socket_status_null_dev)
{
    uint8_t st;
    ASSERT_EQ(w5500_socket_status(NULL, 0, &st), W5500_ERR_PARAM);
}

/* ---------- multi-socket tests ---------- */

TEST(test_multi_socket_open)
{
    init_dev();
    mock_open_status[0] = W5500_SOCK_INIT;
    mock_open_status[7] = W5500_SOCK_UDP;

    ASSERT_EQ(w5500_socket_open(&dev, 0, W5500_PROTO_TCP, 80), W5500_OK);
    ASSERT_EQ(w5500_socket_open(&dev, 7, W5500_PROTO_UDP, 5000), W5500_OK);

    ASSERT_EQ(mock_sock_regs[0][SN_MR], W5500_PROTO_TCP);
    ASSERT_EQ(mock_sock_regs[7][SN_MR], W5500_PROTO_UDP);
}

/* ---------- error code value tests ---------- */

TEST(test_err_values)
{
    ASSERT_EQ((int)W5500_OK,          0);
    ASSERT_EQ((int)W5500_ERR_SPI,    -1);
    ASSERT_EQ((int)W5500_ERR_PARAM,  -2);
    ASSERT_EQ((int)W5500_ERR_TIMEOUT,-3);
    ASSERT_EQ((int)W5500_ERR_SIZE,   -4);
    ASSERT_EQ((int)W5500_ERR_NO_DATA,-5);
    ASSERT_EQ((int)W5500_ERR_SOCK,   -6);
}

/* ---------- command timeout test ---------- */

TEST(test_socket_close_timeout)
{
    init_dev();
    /* Set command clear delay very high so it never clears in time */
    mock_cmd_clear_delay = 999999;

    /* Force CR to non-zero so it never clears */
    mock_sock_regs[0][SN_CR] = 0x10;

    w5500_err_t e = w5500_socket_close(&dev, 0);
    ASSERT_EQ(e, W5500_ERR_TIMEOUT);
}

/* ---------- edge cases ---------- */

TEST(test_socket_send_max_sock_7)
{
    init_dev();
    mock_sock_regs[7][SN_TX_FSR0] = 0x08;
    mock_sock_regs[7][SN_TX_FSR1] = 0x00;
    mock_sock_regs[7][SN_TX_WR0] = 0x00;
    mock_sock_regs[7][SN_TX_WR1] = 0x00;

    uint8_t data[] = {0xAA, 0xBB};
    w5500_err_t e = w5500_socket_send(&dev, 7, data, 2);
    ASSERT_EQ(e, W5500_OK);
    ASSERT_EQ(mock_sock_tx[7][0], 0xAA);
    ASSERT_EQ(mock_sock_tx[7][1], 0xBB);
}

TEST(test_init_no_delay_fn)
{
    mock_reset();
    memset(&dev, 0, sizeof(dev));
    w5500_spi_t spi_no_delay = mock_spi;
    spi_no_delay.delay_ms = NULL;
    w5500_err_t e = w5500_init(&dev, &spi_no_delay);
    ASSERT_EQ(e, W5500_OK);
}

TEST(test_read_version_uninit_dev)
{
    /* dev with spi == NULL */
    w5500_t d;
    memset(&d, 0, sizeof(d));
    ASSERT_EQ(w5500_read_version(&d), 0);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(w5500_t *d)
{
    const uint8_t mac[6] = { 0x02, 0, 0, 0, 0, 1 };
    const uint8_t ip[4] = { 192, 168, 1, 2 };
    const uint8_t mask[4] = { 255, 255, 255, 0 };
    const uint8_t gateway[4] = { 192, 168, 1, 1 };
    uint8_t buf[4] = { 1, 2, 3, 4 };
    uint16_t out_len = 0;
    uint8_t status = 0;
    ASSERT_EQ(w5500_set_mac(d, mac), W5500_ERR_PARAM);
    ASSERT_EQ(w5500_set_ip(d, ip, mask, gateway), W5500_ERR_PARAM);
    ASSERT_EQ(w5500_socket_open(d, 0, W5500_PROTO_TCP, 5000), W5500_ERR_PARAM);
    ASSERT_EQ(w5500_socket_close(d, 0), W5500_ERR_PARAM);
    ASSERT_EQ(w5500_socket_send(d, 0, buf, 4), W5500_ERR_PARAM);
    ASSERT_EQ(w5500_socket_recv(d, 0, buf, 4, &out_len), W5500_ERR_PARAM);
    ASSERT_EQ(w5500_socket_status(d, 0, &status), W5500_ERR_PARAM);
    ASSERT_EQ(w5500_read_version(d), 0);
}

TEST(test_device_refused_after_wrong_version)
{
    w5500_t d;
    mock_reset();
    mock_common_regs[REG_VERSIONR] = 0x51;
    ASSERT_EQ(w5500_init(&d, &mock_spi), W5500_ERR_SPI);
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const w5500_spi_t zeroed;
    w5500_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(w5500_init(&d, NULL), W5500_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(w5500_init(&d, &zeroed), W5500_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("w5500")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_init_spi_missing_cs_low);
    RUN(test_init_spi_missing_cs_high);
    RUN(test_init_spi_missing_xfer);
    RUN(test_init_bad_version);
    RUN(test_init_configures_buffer_sizes);
    RUN(test_read_version);
    RUN(test_read_version_null);
    RUN(test_set_mac);
    RUN(test_set_mac_null_dev);
    RUN(test_set_mac_null_mac);
    RUN(test_set_ip);
    RUN(test_set_ip_null_dev);
    RUN(test_set_ip_null_ip);
    RUN(test_set_ip_null_subnet);
    RUN(test_set_ip_null_gateway);
    RUN(test_socket_open_tcp);
    RUN(test_socket_open_udp);
    RUN(test_socket_open_macraw);
    RUN(test_socket_open_macraw_only_socket_0);
    RUN(test_socket_open_invalid_sock);
    RUN(test_socket_open_invalid_protocol);
    RUN(test_socket_open_null_dev);
    RUN(test_socket_open_wrong_status);
    RUN(test_socket_open_waits_for_status);
    RUN(test_socket_open_finishes_close_before_open);
    RUN(test_socket_open_close_never_finishes);
    RUN(test_socket_close_waits_for_closed);
    RUN(test_socket_close_status_never_changes);
    RUN(test_socket_close);
    RUN(test_socket_close_invalid_sock);
    RUN(test_socket_close_null_dev);
    RUN(test_socket_send);
    RUN(test_socket_send_null_data);
    RUN(test_socket_send_zero_len);
    RUN(test_socket_send_oversized);
    RUN(test_socket_send_insufficient_free);
    RUN(test_socket_send_reports_chip_timeout);
    RUN(test_socket_send_waits_for_send_ok);
    RUN(test_socket_send_no_result_times_out);
    RUN(test_socket_send_ignores_stale_send_ok);
    RUN(test_socket_send_keeps_other_ir_bits);
    RUN(test_socket_send_rereads_torn_free_size);
    RUN(test_socket_send_free_size_never_settles);
    RUN(test_socket_send_invalid_sock);
    RUN(test_socket_send_null_dev);
    RUN(test_socket_recv);
    RUN(test_socket_recv_clamped);
    RUN(test_socket_recv_rereads_torn_size);
    RUN(test_socket_recv_size_never_settles);
    RUN(test_socket_recv_zero_buf_len);
    RUN(test_socket_recv_no_data);
    RUN(test_socket_recv_null_buf);
    RUN(test_socket_recv_null_out_len);
    RUN(test_socket_recv_invalid_sock);
    RUN(test_socket_recv_null_dev);
    RUN(test_socket_status);
    RUN(test_socket_status_null_status);
    RUN(test_socket_status_invalid_sock);
    RUN(test_socket_status_null_dev);
    RUN(test_multi_socket_open);
    RUN(test_err_values);
    RUN(test_socket_close_timeout);
    RUN(test_socket_send_max_sock_7);
    RUN(test_init_no_delay_fn);
    RUN(test_read_version_uninit_dev);
    RUN(test_device_refused_after_wrong_version);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
