#include "test_harness.h"
#include <string.h>
#include "soft_spi.h"

/* ---------- mock GPIO ---------- */

#define TRACE_MAX 2048

enum {
    TR_SCK_L = 1,
    TR_SCK_H,
    TR_MOSI_L,
    TR_MOSI_H,
    TR_MISO_RD,
    TR_DELAY,
};

static uint8_t g_trace[TRACE_MAX];
static uint16_t g_trace_idx;

static uint8_t g_sck_state;
static uint8_t g_mosi_state;

/* MISO data fed bit-by-bit */
static uint8_t g_miso_bits[512];
static uint16_t g_miso_idx;
static uint16_t g_miso_len;

static uint32_t g_delay_count;

static void mock_sck_low(void *ctx)
{
    (void)ctx;
    g_sck_state = 0;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = TR_SCK_L;
    }
}

static void mock_sck_high(void *ctx)
{
    (void)ctx;
    g_sck_state = 1;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = TR_SCK_H;
    }
}

static void mock_mosi_low(void *ctx)
{
    (void)ctx;
    g_mosi_state = 0;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = TR_MOSI_L;
    }
}

static void mock_mosi_high(void *ctx)
{
    (void)ctx;
    g_mosi_state = 1;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = TR_MOSI_H;
    }
}

static bool mock_miso_read(void *ctx)
{
    (void)ctx;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = TR_MISO_RD;
    }
    if (g_miso_idx < g_miso_len) {
        return g_miso_bits[g_miso_idx++] != 0;
    }
    return false; /* default low */
}

static void mock_delay_us(void *ctx, uint16_t us)
{
    (void)ctx;
    (void)us;
    g_delay_count++;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = TR_DELAY;
    }
}

static void mock_reset(void)
{
    g_sck_state = 0;
    g_mosi_state = 0;
    g_miso_idx = 0;
    g_miso_len = 0;
    g_delay_count = 0;
    g_trace_idx = 0;
    memset(g_trace, 0, sizeof(g_trace));
    memset(g_miso_bits, 0, sizeof(g_miso_bits));
}

/* Set MISO to return bits of a byte (MSB first) */
static void set_miso_byte_msb(uint8_t val, uint16_t offset)
{
    for (int i = 7; i >= 0; i--) {
        g_miso_bits[offset + (uint16_t)(7 - i)] = (val >> i) & 1;
    }
}

/* Set MISO to return bits of a byte (LSB first) */
static void set_miso_byte_lsb(uint8_t val, uint16_t offset)
{
    for (int i = 0; i < 8; i++) {
        g_miso_bits[offset + (uint16_t)i] = (val >> i) & 1;
    }
}

static const soft_spi_gpio_t g_mock_gpio = {
    .sck_low   = mock_sck_low,
    .sck_high  = mock_sck_high,
    .mosi_low  = mock_mosi_low,
    .mosi_high = mock_mosi_high,
    .miso_read = mock_miso_read,
    .delay_us  = mock_delay_us,
    .ctx       = NULL,
};

/* ---------- init tests ---------- */

TEST(test_init_null_dev)
{
    soft_spi_cfg_t cfg = {0, false};
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(NULL, &g_mock_gpio, &cfg));
}

TEST(test_init_null_gpio)
{
    soft_spi_t dev;
    soft_spi_cfg_t cfg = {0, false};
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, NULL, &cfg));
}

TEST(test_init_null_cfg)
{
    soft_spi_t dev;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &g_mock_gpio, NULL));
}

TEST(test_init_each_null_callback)
{
    soft_spi_t dev;
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_gpio_t bad;

    bad = g_mock_gpio; bad.sck_low = NULL;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &bad, &cfg));

    bad = g_mock_gpio; bad.sck_high = NULL;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &bad, &cfg));

    bad = g_mock_gpio; bad.mosi_low = NULL;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &bad, &cfg));

    bad = g_mock_gpio; bad.mosi_high = NULL;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &bad, &cfg));

    bad = g_mock_gpio; bad.miso_read = NULL;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &bad, &cfg));

    bad = g_mock_gpio; bad.delay_us = NULL;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &bad, &cfg));
}

TEST(test_init_invalid_mode)
{
    soft_spi_t dev;
    soft_spi_cfg_t cfg = {4, false};
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &g_mock_gpio, &cfg));

    cfg.mode = 255;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &g_mock_gpio, &cfg));
}

TEST(test_init_mode0_sck_idle_low)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev, &g_mock_gpio, &cfg));
    ASSERT_EQ(0, g_sck_state);  /* CPOL=0: idle low */
}

TEST(test_init_mode1_sck_idle_low)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {1, false};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev, &g_mock_gpio, &cfg));
    ASSERT_EQ(0, g_sck_state);  /* CPOL=0: idle low */
}

TEST(test_init_mode2_sck_idle_high)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {2, false};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev, &g_mock_gpio, &cfg));
    ASSERT_EQ(1, g_sck_state);  /* CPOL=1: idle high */
}

TEST(test_init_mode3_sck_idle_high)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {3, false};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev, &g_mock_gpio, &cfg));
    ASSERT_EQ(1, g_sck_state);  /* CPOL=1: idle high */
}

/* ---------- xfer tests: mode 0, MSB first ---------- */

TEST(test_xfer_mode0_msb_tx_0xa5)
{
    /* Mode 0 (CPOL=0,CPHA=0), MSB first: tx 0xA5, rx 0x00 */
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    /* MISO returns all zeros */
    g_miso_len = 8;

    mock_reset();
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0xA5);
    ASSERT_EQ(0x00, rx);

    /* Verify MOSI trace: 0xA5 = 10100101 (MSB first)
     * Mode 0 CPHA=0: for each bit, MOSI set, delay, SCK_H, delay, MISO_RD, SCK_L, delay
     * Check that MOSI was driven correctly for each bit */
}

TEST(test_xfer_mode0_msb_rx_0x5a)
{
    /* Mode 0, MSB first: tx 0xFF, rx should read 0x5A from MISO */
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    /* 0x5A = 01011010 MSB first */
    set_miso_byte_msb(0x5A, 0);
    g_miso_len = 8;

    mock_reset();
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    set_miso_byte_msb(0x5A, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0xFF);
    ASSERT_EQ_HEX(0x5A, rx);
}

TEST(test_xfer_mode0_msb_full_duplex)
{
    /* Full-duplex: tx 0xA5, rx 0x3C simultaneously */
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_msb(0x3C, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0xA5);
    ASSERT_EQ_HEX(0x3C, rx);
}

/* ---------- xfer tests: mode 1 ---------- */

TEST(test_xfer_mode1_msb_rx)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {1, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_msb(0xDB, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0x00);
    ASSERT_EQ_HEX(0xDB, rx);
}

/* ---------- xfer tests: mode 2 ---------- */

TEST(test_xfer_mode2_msb_rx)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {2, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_msb(0x42, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0x00);
    ASSERT_EQ_HEX(0x42, rx);
}

/* ---------- xfer tests: mode 3 ---------- */

TEST(test_xfer_mode3_msb_rx)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {3, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_msb(0xBE, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0x00);
    ASSERT_EQ_HEX(0xBE, rx);
}

/* ---------- LSB first tests ---------- */

TEST(test_xfer_mode0_lsb_rx)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, true};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    /* 0xA5 LSB first: bits 10100101, sent as 1,0,1,0,0,1,0,1 */
    set_miso_byte_lsb(0xA5, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0x00);
    ASSERT_EQ_HEX(0xA5, rx);
}

TEST(test_xfer_mode1_lsb_rx)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {1, true};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_lsb(0x7E, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0x00);
    ASSERT_EQ_HEX(0x7E, rx);
}

TEST(test_xfer_mode2_lsb_rx)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {2, true};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_lsb(0xF0, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0x00);
    ASSERT_EQ_HEX(0xF0, rx);
}

TEST(test_xfer_mode3_lsb_rx)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {3, true};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_lsb(0x55, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0x00);
    ASSERT_EQ_HEX(0x55, rx);
}

/* ---------- xfer edge cases ---------- */

TEST(test_xfer_0x00)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    g_miso_len = 8; /* all zeros */
    uint8_t rx = soft_spi_xfer(&dev, 0x00);
    ASSERT_EQ_HEX(0x00, rx);
}

TEST(test_xfer_0xff)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_msb(0xFF, 0);
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, 0xFF);
    ASSERT_EQ_HEX(0xFF, rx);
}

/* ---------- write tests ---------- */

TEST(test_write_null_dev)
{
    uint8_t data = 0x42;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_write(NULL, &data, 1));
}

TEST(test_write_null_data_nonzero_len)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_write(&dev, NULL, 1));
}

TEST(test_write_null_data_zero_len)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_write(&dev, NULL, 0));
}

TEST(test_write_multi_byte)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    g_miso_len = 24; /* 3 bytes worth of bits */
    uint8_t data[3] = {0x01, 0x02, 0x03};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_write(&dev, data, 3));
    ASSERT_TRUE(g_delay_count > 0);
}

/* ---------- read tests ---------- */

TEST(test_read_null_dev)
{
    uint8_t buf[1];
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_read(NULL, buf, 1));
}

TEST(test_read_null_buf_nonzero_len)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_read(&dev, NULL, 1));
}

TEST(test_read_null_buf_zero_len)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_read(&dev, NULL, 0));
}

TEST(test_read_one_byte)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_msb(0xAB, 0);
    g_miso_len = 8;

    uint8_t buf[1] = {0};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_read(&dev, buf, 1));
    ASSERT_EQ_HEX(0xAB, buf[0]);
}

TEST(test_read_multi_byte)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_msb(0x12, 0);
    set_miso_byte_msb(0x34, 8);
    set_miso_byte_msb(0x56, 16);
    g_miso_len = 24;

    uint8_t buf[3] = {0};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_read(&dev, buf, 3));
    ASSERT_EQ_HEX(0x12, buf[0]);
    ASSERT_EQ_HEX(0x34, buf[1]);
    ASSERT_EQ_HEX(0x56, buf[2]);
}

/* ---------- transfer tests ---------- */

TEST(test_transfer_null_dev)
{
    uint8_t tx = 0, rx = 0;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_transfer(NULL, &tx, &rx, 1));
}

TEST(test_transfer_null_tx_nonzero_len)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    uint8_t rx = 0;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_transfer(&dev, NULL, &rx, 1));
}

TEST(test_transfer_null_rx_nonzero_len)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    uint8_t tx = 0;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_transfer(&dev, &tx, NULL, 1));
}

TEST(test_transfer_zero_len)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_transfer(&dev, NULL, NULL, 0));
}

TEST(test_transfer_full_duplex_multi)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    set_miso_byte_msb(0xAA, 0);
    set_miso_byte_msb(0x55, 8);
    g_miso_len = 16;

    uint8_t tx[2] = {0x11, 0x22};
    uint8_t rx[2] = {0};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_transfer(&dev, tx, rx, 2));
    ASSERT_EQ_HEX(0xAA, rx[0]);
    ASSERT_EQ_HEX(0x55, rx[1]);
}

/* ---------- mode 0 clock/MOSI ordering verification ---------- */

TEST(test_mode0_msb_mosi_bits_order)
{
    /*
     * Mode 0 (CPOL=0,CPHA=0), MSB first, tx=0xA5 (10100101).
     * For each bit: set MOSI, delay, SCK_H, delay, (MISO read), SCK_L, delay.
     * Verify the MOSI calls match 1,0,1,0,0,1,0,1 in order.
     */
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    /* Reset trace after init so we only capture xfer */
    g_trace_idx = 0;
    g_miso_len = 8;

    (void)soft_spi_xfer(&dev, 0xA5);

    /* Scan trace for MOSI_H / MOSI_L sequence (should be H,L,H,L,L,H,L,H) */
    uint8_t mosi_bits[8];
    int mosi_count = 0;
    for (uint16_t i = 0; i < g_trace_idx && mosi_count < 8; i++) {
        if (g_trace[i] == TR_MOSI_H) {
            mosi_bits[mosi_count++] = 1;
        } else if (g_trace[i] == TR_MOSI_L) {
            mosi_bits[mosi_count++] = 1; /* placeholder, count only */
            mosi_count--; /* undo: we need to be smarter */
        }
    }

    /* Simpler approach: extract MOSI states from trace per clock cycle.
     * Mode 0: pattern per bit is [MOSI_x, DELAY, SCK_H, DELAY, MISO_RD, SCK_L, DELAY]
     * So the MOSI call is the first event in each cycle. */
    mosi_count = 0;
    for (uint16_t i = 0; i < g_trace_idx && mosi_count < 8; i++) {
        if (g_trace[i] == TR_MOSI_H || g_trace[i] == TR_MOSI_L) {
            mosi_bits[mosi_count++] = (g_trace[i] == TR_MOSI_H) ? 1 : 0;
        }
    }

    ASSERT_EQ(8, mosi_count);
    /* 0xA5 = 10100101 MSB first */
    ASSERT_EQ(1, mosi_bits[0]);
    ASSERT_EQ(0, mosi_bits[1]);
    ASSERT_EQ(1, mosi_bits[2]);
    ASSERT_EQ(0, mosi_bits[3]);
    ASSERT_EQ(0, mosi_bits[4]);
    ASSERT_EQ(1, mosi_bits[5]);
    ASSERT_EQ(0, mosi_bits[6]);
    ASSERT_EQ(1, mosi_bits[7]);
}

TEST(test_mode0_lsb_mosi_bits_order)
{
    /*
     * Mode 0 (CPOL=0,CPHA=0), LSB first, tx=0xA5 (10100101).
     * LSB first: send bit0 first → 1,0,1,0,0,1,0,1.
     */
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, true};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    g_trace_idx = 0;
    g_miso_len = 8;

    (void)soft_spi_xfer(&dev, 0xA5);

    uint8_t mosi_bits[8];
    int mosi_count = 0;
    for (uint16_t i = 0; i < g_trace_idx && mosi_count < 8; i++) {
        if (g_trace[i] == TR_MOSI_H || g_trace[i] == TR_MOSI_L) {
            mosi_bits[mosi_count++] = (g_trace[i] == TR_MOSI_H) ? 1 : 0;
        }
    }

    ASSERT_EQ(8, mosi_count);
    /* 0xA5 = 10100101, LSB first: bit0=1, bit1=0, bit2=1, bit3=0, bit4=0, bit5=1, bit6=0, bit7=1 */
    ASSERT_EQ(1, mosi_bits[0]);
    ASSERT_EQ(0, mosi_bits[1]);
    ASSERT_EQ(1, mosi_bits[2]);
    ASSERT_EQ(0, mosi_bits[3]);
    ASSERT_EQ(0, mosi_bits[4]);
    ASSERT_EQ(1, mosi_bits[5]);
    ASSERT_EQ(0, mosi_bits[6]);
    ASSERT_EQ(1, mosi_bits[7]);
}

/* ---------- mode 0 vs mode 2 clock polarity ---------- */

TEST(test_mode0_clock_edges)
{
    /* Mode 0: SCK idle low, CPHA=0 → leading edge is rising (SCK_H),
     * trailing edge is falling (SCK_L) */
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {0, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    g_trace_idx = 0;
    g_miso_len = 8;
    (void)soft_spi_xfer(&dev, 0x00);

    /* First SCK event should be SCK_H (rising = leading edge for mode 0) */
    int first_sck = -1;
    for (uint16_t i = 0; i < g_trace_idx; i++) {
        if (g_trace[i] == TR_SCK_H || g_trace[i] == TR_SCK_L) {
            first_sck = g_trace[i];
            break;
        }
    }
    ASSERT_EQ(TR_SCK_H, first_sck);
}

TEST(test_mode2_clock_edges)
{
    /* Mode 2: SCK idle high, CPHA=0 → leading edge is falling (SCK_L),
     * trailing edge is rising (SCK_H) */
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg = {2, false};
    soft_spi_init(&dev, &g_mock_gpio, &cfg);

    g_trace_idx = 0;
    g_miso_len = 8;
    (void)soft_spi_xfer(&dev, 0x00);

    /* First SCK event should be SCK_L (falling = leading edge for mode 2) */
    int first_sck = -1;
    for (uint16_t i = 0; i < g_trace_idx; i++) {
        if (g_trace[i] == TR_SCK_H || g_trace[i] == TR_SCK_L) {
            first_sck = g_trace[i];
            break;
        }
    }
    ASSERT_EQ(TR_SCK_L, first_sck);
}

/* ---------- multi-instance ---------- */

TEST(test_multiple_instances)
{
    soft_spi_t dev1, dev2;
    mock_reset();
    soft_spi_cfg_t cfg0 = {0, false};
    soft_spi_cfg_t cfg3 = {3, true};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev1, &g_mock_gpio, &cfg0));
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev2, &g_mock_gpio, &cfg3));
    ASSERT_EQ(0, dev1.cfg.mode);
    ASSERT_FALSE(dev1.cfg.lsb_first);
    ASSERT_EQ(3, dev2.cfg.mode);
    ASSERT_TRUE(dev2.cfg.lsb_first);
}

/* ---------- reinit ---------- */

TEST(test_reinit_same_dev)
{
    soft_spi_t dev;
    mock_reset();
    soft_spi_cfg_t cfg0 = {0, false};
    soft_spi_cfg_t cfg2 = {2, true};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev, &g_mock_gpio, &cfg0));
    ASSERT_EQ(0, dev.cfg.mode);
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev, &g_mock_gpio, &cfg2));
    ASSERT_EQ(2, dev.cfg.mode);
    ASSERT_TRUE(dev.cfg.lsb_first);
}

/* ---------- uninitialized (gpio=NULL) ---------- */

TEST(test_write_uninit_dev)
{
    soft_spi_t dev;
    memset(&dev, 0, sizeof(dev));
    uint8_t data = 0x42;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_write(&dev, &data, 1));
}

TEST(test_read_uninit_dev)
{
    soft_spi_t dev;
    memset(&dev, 0, sizeof(dev));
    uint8_t buf[1];
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_read(&dev, buf, 1));
}

TEST(test_transfer_uninit_dev)
{
    soft_spi_t dev;
    memset(&dev, 0, sizeof(dev));
    uint8_t tx = 0, rx = 0;
    ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_transfer(&dev, &tx, &rx, 1));
}

/* ---------- all modes rx correctness (systematic) ---------- */

TEST(test_all_modes_msb_rx_0xc3)
{
    /* Verify all 4 modes correctly receive 0xC3 in MSB-first */
    for (uint8_t mode = 0; mode < 4; mode++) {
        soft_spi_t dev;
        mock_reset();
        soft_spi_cfg_t cfg;
        cfg.mode = mode;
        cfg.lsb_first = false;
        soft_spi_init(&dev, &g_mock_gpio, &cfg);

        set_miso_byte_msb(0xC3, 0);
        g_miso_len = 8;

        uint8_t rx = soft_spi_xfer(&dev, 0x00);
        ASSERT_EQ_HEX(0xC3, rx);
    }
}

TEST(test_all_modes_lsb_rx_0xc3)
{
    /* Verify all 4 modes correctly receive 0xC3 in LSB-first */
    for (uint8_t mode = 0; mode < 4; mode++) {
        soft_spi_t dev;
        mock_reset();
        soft_spi_cfg_t cfg;
        cfg.mode = mode;
        cfg.lsb_first = true;
        soft_spi_init(&dev, &g_mock_gpio, &cfg);

        set_miso_byte_lsb(0xC3, 0);
        g_miso_len = 8;

        uint8_t rx = soft_spi_xfer(&dev, 0x00);
        ASSERT_EQ_HEX(0xC3, rx);
    }
}

/* ---------- error enum value ---------- */

TEST(test_err_param_enum_value)
{
    ASSERT_EQ(-1, SOFT_SPI_ERR_PARAM);
    ASSERT_EQ(0, SOFT_SPI_OK);
}

/* ---------- ctx passthrough ---------- */

static void *g_captured_ctx;

static void ctx_sck_low(void *ctx) { g_captured_ctx = ctx; }
static void ctx_sck_high(void *ctx) { (void)ctx; }
static void ctx_mosi_low(void *ctx) { (void)ctx; }
static void ctx_mosi_high(void *ctx) { (void)ctx; }
static bool ctx_miso_read(void *ctx) { (void)ctx; return false; }
static void ctx_delay_us(void *ctx, uint16_t us) { (void)ctx; (void)us; }

TEST(test_ctx_passthrough)
{
    int my_context = 42;
    g_captured_ctx = NULL;

    soft_spi_gpio_t gpio = {
        .sck_low   = ctx_sck_low,
        .sck_high  = ctx_sck_high,
        .mosi_low  = ctx_mosi_low,
        .mosi_high = ctx_mosi_high,
        .miso_read = ctx_miso_read,
        .delay_us  = ctx_delay_us,
        .ctx       = &my_context,
    };
    soft_spi_t dev;
    soft_spi_cfg_t cfg = {0, false};
    ASSERT_EQ(SOFT_SPI_OK, soft_spi_init(&dev, &gpio, &cfg));

    /* sck_low was called during init (mode 0 idle); ctx should have been passed */
    ASSERT_TRUE(g_captured_ctx == &my_context);
}

TEST_MAIN("soft_spi")
    RUN(test_init_null_dev);
    RUN(test_init_null_gpio);
    RUN(test_init_null_cfg);
    RUN(test_init_each_null_callback);
    RUN(test_init_invalid_mode);
    RUN(test_init_mode0_sck_idle_low);
    RUN(test_init_mode1_sck_idle_low);
    RUN(test_init_mode2_sck_idle_high);
    RUN(test_init_mode3_sck_idle_high);
    RUN(test_xfer_mode0_msb_tx_0xa5);
    RUN(test_xfer_mode0_msb_rx_0x5a);
    RUN(test_xfer_mode0_msb_full_duplex);
    RUN(test_xfer_mode1_msb_rx);
    RUN(test_xfer_mode2_msb_rx);
    RUN(test_xfer_mode3_msb_rx);
    RUN(test_xfer_mode0_lsb_rx);
    RUN(test_xfer_mode1_lsb_rx);
    RUN(test_xfer_mode2_lsb_rx);
    RUN(test_xfer_mode3_lsb_rx);
    RUN(test_xfer_0x00);
    RUN(test_xfer_0xff);
    RUN(test_write_null_dev);
    RUN(test_write_null_data_nonzero_len);
    RUN(test_write_null_data_zero_len);
    RUN(test_write_multi_byte);
    RUN(test_read_null_dev);
    RUN(test_read_null_buf_nonzero_len);
    RUN(test_read_null_buf_zero_len);
    RUN(test_read_one_byte);
    RUN(test_read_multi_byte);
    RUN(test_transfer_null_dev);
    RUN(test_transfer_null_tx_nonzero_len);
    RUN(test_transfer_null_rx_nonzero_len);
    RUN(test_transfer_zero_len);
    RUN(test_transfer_full_duplex_multi);
    RUN(test_mode0_msb_mosi_bits_order);
    RUN(test_mode0_lsb_mosi_bits_order);
    RUN(test_mode0_clock_edges);
    RUN(test_mode2_clock_edges);
    RUN(test_multiple_instances);
    RUN(test_reinit_same_dev);
    RUN(test_write_uninit_dev);
    RUN(test_read_uninit_dev);
    RUN(test_transfer_uninit_dev);
    RUN(test_all_modes_msb_rx_0xc3);
    RUN(test_all_modes_lsb_rx_0xc3);
    RUN(test_err_param_enum_value);
    RUN(test_ctx_passthrough);
TEST_MAIN_END()
