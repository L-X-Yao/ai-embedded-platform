// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "enc28j60.h"

/* ---------- mock SPI ---------- */

/* SPI opcodes (mirrored from enc28j60.c for verification) */
#define OP_RCR  0x00U
#define OP_RBM  0x3AU
#define OP_WCR  0x40U
#define OP_WBM  0x7AU
#define OP_BFS  0x80U
#define OP_BFC  0xA0U
#define OP_SRC  0xFFU

/* Register addresses for test verification */
#define ECON1     0x1FU
#define ECON2     0x1EU
#define ESTAT     0x1DU
#define EPKTCNT   0x19U
#define EREVID    0x12U
#define ERXSTL    0x08U
#define MAADR1    0x04U
#define MACON1    0x00U

#define ECON1_BSEL0  0x01U
#define ECON1_BSEL1  0x02U
#define ECON1_RXEN   0x04U
#define ECON1_TXRTS  0x08U
#define ECON2_PKTDEC 0x40U
#define ESTAT_CLKRDY 0x01U

/* Table 7-3 bit 23, Received Ok: bit 7 of the fifth header byte (Figure 7-3) */
#define RSV_RECEIVED_OK 0x80U

static uint8_t xfer_log[1024];
static int     xfer_log_idx;
static int     xfer_call_count;

/* Mock register state: 4 banks x 32 registers.
 * Registers 0x1B-0x1F (EIE, EIR, ESTAT, ECON2, ECON1) are common across
 * all banks on real hardware.  We store them in a dedicated array. */
static uint8_t mock_regs[4][32];
static uint8_t mock_common[32];   /* only indices 0x1B..0x1F used */
static uint8_t mock_current_bank;

#define IS_COMMON_REG(a) ((a) >= 0x1BU)

/* MAC and MII registers (Table 3-1) read with a dummy byte first: all of bank
 * 2, and MAADR and MISTAT in bank 3 */
#define IS_MAC_MII_REG(b, a) \
    (!IS_COMMON_REG(a) && \
     ((b) == 2U || ((b) == 3U && ((a) <= 0x05U || (a) == 0x0AU))))

/* Mock RX buffer for receive testing */
static uint8_t mock_rx_buf[2048];
static int     mock_rx_buf_idx;
static int     mock_rx_buf_len;

/* Mock TX tracking */
static uint8_t mock_tx_buf[2048];
static int     mock_tx_buf_idx;

/* Control flags */
static int     mock_reset_seen;
static int     mock_delay_calls;
static uint32_t mock_last_delay_ms;

/* SPI transaction state machine */
static enum {
    SPI_IDLE,
    SPI_READ_CMD,      /* got opcode, waiting for data/dummy */
    SPI_READ_DUMMY,    /* MAC/MII dummy byte phase */
    SPI_WRITE_CMD,     /* got opcode, waiting for data byte */
    SPI_READ_BUF,      /* reading from RX buffer */
    SPI_WRITE_BUF,     /* writing to TX buffer */
} mock_spi_state;

static uint8_t mock_spi_opcode;
static uint8_t mock_spi_addr;

/* TX completion: how many reads of ECON1 before TXRTS clears */
static int mock_tx_complete_after;
static int mock_tx_read_count;

static void mock_reset(void)
{
    xfer_log_idx = 0;
    xfer_call_count = 0;
    memset(xfer_log, 0, sizeof(xfer_log));
    memset(mock_regs, 0, sizeof(mock_regs));
    memset(mock_common, 0, sizeof(mock_common));
    mock_current_bank = 0;
    mock_rx_buf_idx = 0;
    mock_rx_buf_len = 0;
    memset(mock_rx_buf, 0, sizeof(mock_rx_buf));
    mock_tx_buf_idx = 0;
    memset(mock_tx_buf, 0, sizeof(mock_tx_buf));
    mock_reset_seen = 0;
    mock_delay_calls = 0;
    mock_last_delay_ms = 0;
    mock_spi_state = SPI_IDLE;
    mock_spi_opcode = 0;
    mock_spi_addr = 0;
    mock_tx_complete_after = 0;
    mock_tx_read_count = 0;

    /* Set EREVID to a known value */
    mock_regs[3][EREVID] = 0x06;

    /* The oscillator has started: init polls this bit */
    mock_common[ESTAT] = ESTAT_CLKRDY;
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_spi_state = SPI_IDLE;
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_spi_state = SPI_IDLE;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (xfer_log_idx < (int)sizeof(xfer_log))
        xfer_log[xfer_log_idx++] = tx;
    xfer_call_count++;

    switch (mock_spi_state) {
    case SPI_IDLE: {
        /* First byte after CS low: opcode + register address */
        uint8_t op = tx & 0xE0U;
        uint8_t addr = tx & 0x1FU;

        if (tx == OP_SRC) {
            mock_reset_seen++;
            mock_current_bank = 0;
            return 0xFFU;
        }
        if (tx == OP_RBM) {
            mock_spi_state = SPI_READ_BUF;
            return 0xFFU;
        }
        if (tx == OP_WBM) {
            mock_spi_state = SPI_WRITE_BUF;
            return 0xFFU;
        }

        mock_spi_opcode = op;
        mock_spi_addr = addr;

        if (op == OP_RCR) {
            mock_spi_state = IS_MAC_MII_REG(mock_current_bank, addr)
                           ? SPI_READ_DUMMY : SPI_READ_CMD;
        } else if (op == OP_WCR) {
            mock_spi_state = SPI_WRITE_CMD;
        } else if (op == OP_BFS) {
            mock_spi_state = SPI_WRITE_CMD;
        } else if (op == OP_BFC) {
            mock_spi_state = SPI_WRITE_CMD;
        }
        return 0xFFU;
    }

    case SPI_READ_CMD: {
        /* Common registers (0x1B-0x1F) are shared across all banks */
        uint8_t *reg_ptr = IS_COMMON_REG(mock_spi_addr)
            ? &mock_common[mock_spi_addr]
            : &mock_regs[mock_current_bank][mock_spi_addr];
        if (mock_spi_addr == ECON1) {
            /* For TX completion test: simulate TXRTS clearing */
            if (*reg_ptr & ECON1_TXRTS) {
                mock_tx_read_count++;
                if (mock_tx_read_count >= mock_tx_complete_after) {
                    *reg_ptr &= (uint8_t)~ECON1_TXRTS;
                }
            }
        }
        return *reg_ptr;
    }

    case SPI_WRITE_CMD: {
        /* Common registers (0x1B-0x1F) are shared across all banks */
        uint8_t *reg_ptr = IS_COMMON_REG(mock_spi_addr)
            ? &mock_common[mock_spi_addr]
            : &mock_regs[mock_current_bank][mock_spi_addr];

        if (mock_spi_opcode == OP_WCR) {
            *reg_ptr = tx;
        } else if (IS_MAC_MII_REG(mock_current_bank, mock_spi_addr)) {
            /* DS39662C p.31: BFS and BFC have no effect on these */
        } else if (mock_spi_opcode == OP_BFS) {
            *reg_ptr |= tx;
        } else if (mock_spi_opcode == OP_BFC) {
            *reg_ptr &= (uint8_t)~tx;
        }
        /* Track bank select changes */
        if (mock_spi_addr == ECON1) {
            mock_current_bank = mock_common[ECON1] & 0x03U;
        }
        mock_spi_state = SPI_IDLE;
        return 0xFFU;
    }

    case SPI_READ_BUF:
        if (mock_rx_buf_idx < mock_rx_buf_len) {
            return mock_rx_buf[mock_rx_buf_idx++];
        }
        return 0x00U;

    case SPI_WRITE_BUF:
        if (mock_tx_buf_idx < (int)sizeof(mock_tx_buf)) {
            mock_tx_buf[mock_tx_buf_idx++] = tx;
        }
        return 0xFFU;

    case SPI_READ_DUMMY:
        mock_spi_state = SPI_READ_CMD;
        return 0xFFU;

    default:
        return 0xFFU;
    }
}

static void mock_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_delay_calls++;
    mock_last_delay_ms = ms;
}

static const enc28j60_spi_t mock_spi = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

static const uint8_t test_mac[6] = {0x02, 0x00, 0x00, 0x11, 0x22, 0x33};

/* Helper: initialize a device for tests that need it */
static enc28j60_t dev;

static void init_dev(void)
{
    mock_reset();
    memset(&dev, 0, sizeof(dev));
    enc28j60_init(&dev, &mock_spi, test_mac);
    /* Reset xfer log after init so tests see only their own ops */
    xfer_log_idx = 0;
    xfer_call_count = 0;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    memset(&dev, 0, sizeof(dev));
    enc28j60_err_t e = enc28j60_init(&dev, &mock_spi, test_mac);
    ASSERT_EQ(e, ENC28J60_OK);
    /* Soft reset must have been issued */
    ASSERT_TRUE(mock_reset_seen > 0);
    /* delay_ms must have been called after reset */
    ASSERT_TRUE(mock_delay_calls > 0);
}

TEST(test_init_sets_mac)
{
    mock_reset();
    memset(&dev, 0, sizeof(dev));
    enc28j60_init(&dev, &mock_spi, test_mac);
    /* MAC address should be stored in device struct */
    ASSERT_EQ(dev.mac_addr[0], 0x02);
    ASSERT_EQ(dev.mac_addr[1], 0x00);
    ASSERT_EQ(dev.mac_addr[2], 0x00);
    ASSERT_EQ(dev.mac_addr[3], 0x11);
    ASSERT_EQ(dev.mac_addr[4], 0x22);
    ASSERT_EQ(dev.mac_addr[5], 0x33);
    /* MAC regs should have been written (bank 3) */
    ASSERT_EQ(mock_regs[3][MAADR1], 0x02);
}

TEST(test_init_configures_rx_buffer)
{
    mock_reset();
    memset(&dev, 0, sizeof(dev));
    enc28j60_init(&dev, &mock_spi, test_mac);
    /* ERXSTL/H should be 0x0000 (bank 0) */
    ASSERT_EQ(mock_regs[0][0x08], 0x00); /* ERXSTL */
    ASSERT_EQ(mock_regs[0][0x09], 0x00); /* ERXSTH */
    /* ERXNDL/H should be 0x19FF */
    ASSERT_EQ(mock_regs[0][0x0A], 0xFF); /* ERXNDL */
    ASSERT_EQ(mock_regs[0][0x0B], 0x19); /* ERXNDH */
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(enc28j60_init(NULL, &mock_spi, test_mac), ENC28J60_ERR_PARAM);
}

TEST(test_init_null_spi)
{
    enc28j60_t d;
    ASSERT_EQ(enc28j60_init(&d, NULL, test_mac), ENC28J60_ERR_PARAM);
}

TEST(test_init_null_mac)
{
    enc28j60_t d;
    ASSERT_EQ(enc28j60_init(&d, &mock_spi, NULL), ENC28J60_ERR_PARAM);
}

TEST(test_get_revision)
{
    init_dev();
    /* mock_regs[3][EREVID] was set to 0x06 in mock_reset() */
    uint8_t rev = enc28j60_get_revision(&dev);
    ASSERT_EQ(rev, 0x06);
}

TEST(test_send_frame)
{
    init_dev();
    uint8_t frame[64];
    memset(frame, 0xAA, sizeof(frame));

    /* TX completes immediately (TXRTS clears on first read) */
    mock_tx_complete_after = 1;
    mock_tx_read_count = 0;

    enc28j60_err_t e = enc28j60_send(&dev, frame, 64);
    ASSERT_EQ(e, ENC28J60_OK);
    /* TX buffer should have control byte (0x00) + frame data */
    ASSERT_EQ(mock_tx_buf[0], 0x00); /* per-packet control byte */
    ASSERT_EQ(mock_tx_buf[1], 0xAA); /* first frame byte */
}

TEST(test_send_oversized)
{
    init_dev();
    uint8_t frame[1];
    enc28j60_err_t e = enc28j60_send(&dev, frame, 1519);
    ASSERT_EQ(e, ENC28J60_ERR_SIZE);
}

TEST(test_send_empty)
{
    init_dev();
    uint8_t frame[1];
    enc28j60_err_t e = enc28j60_send(&dev, frame, 0);
    ASSERT_EQ(e, ENC28J60_ERR_SIZE);
}

TEST(test_send_null_frame)
{
    init_dev();
    enc28j60_err_t e = enc28j60_send(&dev, NULL, 64);
    ASSERT_EQ(e, ENC28J60_ERR_PARAM);
}

TEST(test_receive_packet)
{
    init_dev();

    /* Prepare mock RX buffer with a packet:
     * [next_ptr_lo, next_ptr_hi, len_lo, len_hi, status_lo, status_hi, data...]
     */
    uint8_t test_frame[] = {0xDE, 0xAD, 0xBE, 0xEF};
    mock_rx_buf[0] = 0x00; /* next packet pointer low (back to start) */
    mock_rx_buf[1] = 0x00; /* next packet pointer high */
    mock_rx_buf[2] = 0x04; /* byte count low */
    mock_rx_buf[3] = 0x00; /* byte count high */
    mock_rx_buf[4] = RSV_RECEIVED_OK; /* status low */
    mock_rx_buf[5] = 0x00; /* status high */
    memcpy(&mock_rx_buf[6], test_frame, 4);
    mock_rx_buf_len = 10;
    mock_rx_buf_idx = 0;

    /* Set packet count = 1 */
    mock_regs[1][EPKTCNT] = 1;

    uint8_t out[64];
    uint16_t out_len = 0;
    enc28j60_err_t e = enc28j60_receive(&dev, out, sizeof(out), &out_len);
    ASSERT_EQ(e, ENC28J60_OK);
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ(out[0], 0xDE);
    ASSERT_EQ(out[1], 0xAD);
    ASSERT_EQ(out[2], 0xBE);
    ASSERT_EQ(out[3], 0xEF);
}

TEST(test_has_packet)
{
    init_dev();

    /* No packets */
    mock_regs[1][EPKTCNT] = 0;
    ASSERT_FALSE(enc28j60_has_packet(&dev));

    /* One packet */
    mock_regs[1][EPKTCNT] = 1;
    ASSERT_TRUE(enc28j60_has_packet(&dev));

    /* Multiple packets */
    mock_regs[1][EPKTCNT] = 5;
    ASSERT_TRUE(enc28j60_has_packet(&dev));
}

TEST(test_no_packet)
{
    init_dev();
    mock_regs[1][EPKTCNT] = 0;
    uint8_t out[64];
    uint16_t out_len = 0;
    enc28j60_err_t e = enc28j60_receive(&dev, out, sizeof(out), &out_len);
    ASSERT_EQ(e, ENC28J60_ERR_NO_PACKET);
}

TEST(test_bank_switching)
{
    init_dev();

    /* Reading EREVID is bank 3, EPKTCNT is bank 1.
     * Accessing them sequentially forces bank switches. */
    mock_regs[3][EREVID] = 0x06;
    mock_regs[1][EPKTCNT] = 3;

    uint8_t rev = enc28j60_get_revision(&dev);
    ASSERT_EQ(rev, 0x06);
    /* dev->bank should be 3 now */
    ASSERT_EQ(dev.bank, 3);

    /* Now access EPKTCNT (bank 1) */
    bool has = enc28j60_has_packet(&dev);
    ASSERT_TRUE(has);
    /* dev->bank should be 1 now */
    ASSERT_EQ(dev.bank, 1);
}

TEST(test_set_mac)
{
    init_dev();
    uint8_t new_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    enc28j60_err_t e = enc28j60_set_mac(&dev, new_mac);
    ASSERT_EQ(e, ENC28J60_OK);
    ASSERT_EQ(dev.mac_addr[0], 0xAA);
    ASSERT_EQ(dev.mac_addr[5], 0xFF);
    /* Check register was written */
    ASSERT_EQ(mock_regs[3][MAADR1], 0xAA);
}

TEST(test_set_mac_null)
{
    init_dev();
    ASSERT_EQ(enc28j60_set_mac(&dev, NULL), ENC28J60_ERR_PARAM);
}

TEST(test_enable_reception)
{
    init_dev();
    /* Clear RXEN first (ECON1 is a common register) */
    mock_common[ECON1] &= (uint8_t)~ECON1_RXEN;

    enc28j60_enable_reception(&dev);
    /* RXEN bit should be set via BFS */
    ASSERT_TRUE(mock_common[ECON1] & ECON1_RXEN);
}

TEST(test_receive_null_frame)
{
    init_dev();
    uint16_t len;
    ASSERT_EQ(enc28j60_receive(&dev, NULL, 64, &len), ENC28J60_ERR_PARAM);
}

TEST(test_receive_null_len_out)
{
    init_dev();
    uint8_t buf[64];
    ASSERT_EQ(enc28j60_receive(&dev, buf, 64, NULL), ENC28J60_ERR_PARAM);
}

TEST(test_has_packet_null_dev)
{
    ASSERT_FALSE(enc28j60_has_packet(NULL));
}

TEST(test_get_revision_null_dev)
{
    ASSERT_EQ(enc28j60_get_revision(NULL), 0);
}

/* ---------- ENC28J60_ERR_TIMEOUT: TXRTS never clears ---------- */

TEST(test_send_timeout)
{
    init_dev();
    uint8_t frame[64];
    memset(frame, 0xBB, sizeof(frame));

    /* TXRTS never clears → driver polls ENC28J60_TX_TIMEOUT times then
     * returns ENC28J60_ERR_TIMEOUT.  Set mock_tx_complete_after to a
     * value larger than ENC28J60_TX_TIMEOUT (50 000) so the bit stays
     * set throughout the polling loop. */
    mock_tx_complete_after = 999999;
    mock_tx_read_count = 0;
    /* Pre-set TXRTS in ECON1 so BFS during send keeps it set */
    mock_common[ECON1] |= ECON1_TXRTS;

    enc28j60_err_t e = enc28j60_send(&dev, frame, 64);
    ASSERT_EQ(e, ENC28J60_ERR_TIMEOUT);
}

/* ---------- ENC28J60_ERR_SPI: defined but currently unreachable --------
 * The enum value exists for future SPI-level error detection.  No current
 * API path returns it, so no behavioral test is possible.  This smoke
 * test simply verifies the constant's numeric value for ABI stability. */

TEST(test_err_spi_value)
{
    ASSERT_EQ((int)ENC28J60_ERR_SPI, -1);
}

/* ---------- receive: buffer too small (next_pkt wraps to RXSTART) ----------
 *
 * When rx_len > buf_size the driver must still free the packet slot and
 * advance next_packet_ptr, then return ENC28J60_ERR_SIZE.  The free-space
 * update has two branches:
 *   (a) next_pkt == RXSTART → ERXRDPT set to RXEND
 *   (b) next_pkt != RXSTART → ERXRDPT set to next_pkt - 1
 *
 * This test covers branch (a): next_pkt is 0x0000 (RXSTART). */

TEST(test_receive_buf_too_small_wrap)
{
    init_dev();

    /* Inject header: next_pkt=0x0000 (RXSTART), rx_len=40 */
    mock_rx_buf[0] = 0x00; /* next_pkt lo */
    mock_rx_buf[1] = 0x00; /* next_pkt hi */
    mock_rx_buf[2] = 40;   /* rx_len lo */
    mock_rx_buf[3] = 0x00; /* rx_len hi */
    mock_rx_buf[4] = RSV_RECEIVED_OK; /* status lo */
    mock_rx_buf[5] = 0x00; /* status hi */
    mock_rx_buf_len = 6;
    mock_rx_buf_idx = 0;

    mock_regs[1][EPKTCNT] = 1;

    uint8_t out[10]; /* smaller than rx_len (40) */
    uint16_t out_len = 0;
    enc28j60_err_t e = enc28j60_receive(&dev, out, sizeof(out), &out_len);
    ASSERT_EQ(e, ENC28J60_ERR_SIZE);
    /* frame_len should still be reported even on error */
    ASSERT_EQ(out_len, 40);
    /* next_packet_ptr must be advanced (to RXSTART = 0x0000) */
    ASSERT_EQ(dev.next_packet_ptr, 0x0000);
}

/* ---------- receive: buffer too small (next_pkt non-zero, no wrap) ---------
 *
 * Branch (b): next_pkt is non-zero, so ERXRDPT = next_pkt - 1. */

TEST(test_receive_buf_too_small_no_wrap)
{
    init_dev();

    /* Inject header: next_pkt=0x0020, rx_len=40 */
    mock_rx_buf[0] = 0x20; /* next_pkt lo */
    mock_rx_buf[1] = 0x00; /* next_pkt hi */
    mock_rx_buf[2] = 40;   /* rx_len lo */
    mock_rx_buf[3] = 0x00; /* rx_len hi */
    mock_rx_buf[4] = RSV_RECEIVED_OK;
    mock_rx_buf[5] = 0x00;
    mock_rx_buf_len = 6;
    mock_rx_buf_idx = 0;

    mock_regs[1][EPKTCNT] = 1;

    uint8_t out[10];
    uint16_t out_len = 0;
    enc28j60_err_t e = enc28j60_receive(&dev, out, sizeof(out), &out_len);
    ASSERT_EQ(e, ENC28J60_ERR_SIZE);
    ASSERT_EQ(out_len, 40);
    ASSERT_EQ(dev.next_packet_ptr, 0x0020);
}

/* ---------- receive: normal path with non-zero next_pkt -------------------
 *
 * When a packet is received successfully and next_pkt != RXSTART the
 * driver sets ERXRDPT = next_pkt - 1 (errata: must be odd value).
 * This exercises lines 367-368 in enc28j60.c. */

TEST(test_receive_next_pkt_nonzero)
{
    init_dev();

    uint8_t test_frame[] = {0x11, 0x22, 0x33, 0x44};
    /* next_pkt = 0x0010 (non-zero, non-RXSTART) */
    mock_rx_buf[0] = 0x10; /* next_pkt lo */
    mock_rx_buf[1] = 0x00; /* next_pkt hi */
    mock_rx_buf[2] = 0x04; /* rx_len lo */
    mock_rx_buf[3] = 0x00; /* rx_len hi */
    mock_rx_buf[4] = RSV_RECEIVED_OK;
    mock_rx_buf[5] = 0x00;
    memcpy(&mock_rx_buf[6], test_frame, 4);
    mock_rx_buf_len = 10;
    mock_rx_buf_idx = 0;

    mock_regs[1][EPKTCNT] = 1;

    uint8_t out[64];
    uint16_t out_len = 0;
    enc28j60_err_t e = enc28j60_receive(&dev, out, sizeof(out), &out_len);
    ASSERT_EQ(e, ENC28J60_OK);
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ(out[0], 0x11);
    ASSERT_EQ(out[3], 0x44);
    /* next_packet_ptr should be updated to 0x0010 */
    ASSERT_EQ(dev.next_packet_ptr, 0x0010);
}

/* DS39662C p.31: BFS and BFC "cannot be used on the MAC registers, MII
 * registers, PHY registers or buffer memory" */
TEST(test_mock_bfs_bfc_leave_mac_registers_alone)
{
    init_dev();
    enc28j60_get_revision(&dev);   /* bank 3, where MAADR1 is */
    ASSERT_EQ(mock_regs[3][MAADR1], test_mac[0]);

    mock_cs_low(NULL);
    mock_xfer(NULL, OP_BFS | MAADR1);
    mock_xfer(NULL, 0xFFU);
    mock_cs_high(NULL);
    ASSERT_EQ(mock_regs[3][MAADR1], test_mac[0]);

    mock_cs_low(NULL);
    mock_xfer(NULL, OP_BFC | MAADR1);
    mock_xfer(NULL, 0xFFU);
    mock_cs_high(NULL);
    ASSERT_EQ(mock_regs[3][MAADR1], test_mac[0]);
}

TEST_MAIN("enc28j60")
    RUN(test_init_ok);
    RUN(test_init_sets_mac);
    RUN(test_init_configures_rx_buffer);
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_init_null_mac);
    RUN(test_get_revision);
    RUN(test_send_frame);
    RUN(test_send_oversized);
    RUN(test_send_empty);
    RUN(test_send_null_frame);
    RUN(test_receive_packet);
    RUN(test_has_packet);
    RUN(test_no_packet);
    RUN(test_bank_switching);
    RUN(test_set_mac);
    RUN(test_set_mac_null);
    RUN(test_enable_reception);
    RUN(test_receive_null_frame);
    RUN(test_receive_null_len_out);
    RUN(test_has_packet_null_dev);
    RUN(test_get_revision_null_dev);
    RUN(test_send_timeout);
    RUN(test_err_spi_value);
    RUN(test_receive_buf_too_small_wrap);
    RUN(test_receive_buf_too_small_no_wrap);
    RUN(test_receive_next_pkt_nonzero);
    RUN(test_mock_bfs_bfc_leave_mac_registers_alone);
TEST_MAIN_END()
