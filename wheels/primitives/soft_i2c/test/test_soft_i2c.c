// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "soft_i2c.h"

/* ---------- mock GPIO ---------- */

#define TRACE_MAX 512

static uint8_t g_scl_state;
static uint8_t g_sda_state;
static uint8_t g_sda_dir;  /* 0=in, 1=out */
static uint8_t g_slave_response[32];
static uint16_t g_slave_resp_idx;
static uint16_t g_slave_resp_len;
static uint32_t g_delay_count;
static uint8_t g_scl_fell;   /* SCL fell since the last STOP: mid-transfer */
static uint8_t g_idle_level; /* SDA as read with the bus idle */

static uint8_t g_trace[TRACE_MAX];
static uint16_t g_trace_idx;

enum { TR_SCL_H = 1, TR_SCL_L, TR_SDA_H, TR_SDA_L, TR_SDA_IN, TR_SDA_OUT, TR_DELAY };

static void mock_scl_set(void *ctx, bool high)
{
    (void)ctx;
    if (!high) {
        g_scl_fell = 1;
    }
    g_scl_state = high ? 1 : 0;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = high ? TR_SCL_H : TR_SCL_L;
    }
}

static void mock_sda_set(void *ctx, bool high)
{
    (void)ctx;
    if (high && g_scl_state && !g_sda_state) {
        g_scl_fell = 0; /* SDA rising with SCL high is a STOP */
    }
    g_sda_state = high ? 1 : 0;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = high ? TR_SDA_H : TR_SDA_L;
    }
}

static void mock_sda_dir_out(void *ctx)
{
    (void)ctx;
    g_sda_dir = 1;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = TR_SDA_OUT;
    }
}

static void mock_sda_dir_in(void *ctx)
{
    (void)ctx;
    g_sda_dir = 0;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = TR_SDA_IN;
    }
}

static bool mock_sda_get(void *ctx)
{
    (void)ctx;
    if (!g_scl_fell) {
        return g_idle_level != 0; /* idle bus: the script is for transfers */
    }
    if (g_slave_resp_idx < g_slave_resp_len) {
        return g_slave_response[g_slave_resp_idx++] != 0;
    }
    return true; /* default: NACK / high */
}

static bool mock_sda_held_low(void *ctx)
{
    (void)ctx;
    return false;
}

static void mock_delay(void *ctx)
{
    (void)ctx;
    g_delay_count++;
}

static void mock_reset(void)
{
    g_scl_state = 1;
    g_sda_state = 1;
    g_sda_dir = 1;
    g_slave_resp_idx = 0;
    g_slave_resp_len = 0;
    g_delay_count = 0;
    g_scl_fell = 0;
    g_idle_level = 1;
    g_trace_idx = 0;
    memset(g_trace, 0, sizeof(g_trace));
    memset(g_slave_response, 1, sizeof(g_slave_response));
}

static void set_slave_acks(uint16_t n)
{
    for (uint16_t i = 0; i < n && i < sizeof(g_slave_response); i++) {
        g_slave_response[i] = 0; /* ACK = SDA low */
    }
    g_slave_resp_len = n;
    g_slave_resp_idx = 0;
}

static const soft_i2c_gpio_t g_mock_gpio = {
    .scl_set    = mock_scl_set,
    .sda_set    = mock_sda_set,
    .sda_dir_out = mock_sda_dir_out,
    .sda_dir_in  = mock_sda_dir_in,
    .sda_get    = mock_sda_get,
    .delay      = mock_delay,
    .ctx        = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_null_bus)
{
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_init(NULL, &g_mock_gpio));
}

TEST(test_init_null_gpio)
{
    soft_i2c_t bus;
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_init(&bus, NULL));
}

TEST(test_init_missing_callback)
{
    soft_i2c_t bus;
    soft_i2c_gpio_t bad = g_mock_gpio;
    bad.scl_set = NULL;
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_init(&bus, &bad));
}

TEST(test_init_ok)
{
    soft_i2c_t bus;
    mock_reset();
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_init(&bus, &g_mock_gpio));
    ASSERT_EQ(1, g_scl_state);
    ASSERT_EQ(1, g_sda_state);
}

TEST(test_write_null_bus)
{
    uint8_t data = 0x42;
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_write(NULL, 0x50, &data, 1));
}

TEST(test_write_null_data_nonzero_len)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_write(&bus, 0x50, NULL, 1));
}

TEST(test_write_addr_nack)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    /* No ACKs set → slave NACKs address */
    g_slave_resp_len = 1;
    g_slave_response[0] = 1; /* NACK */
    ASSERT_EQ(SOFT_I2C_ERR_NACK, soft_i2c_write(&bus, 0x50, (uint8_t[]){0xAB}, 1));
}

TEST(test_write_one_byte_ok)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    set_slave_acks(2); /* addr ACK + data ACK */
    uint8_t data = 0xCD;
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_write(&bus, 0x50, &data, 1));
    ASSERT_TRUE(g_delay_count > 0);
}

TEST(test_write_multi_byte)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    set_slave_acks(5); /* addr + 4 data bytes */
    uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_write(&bus, 0x68, data, 4));
}

TEST(test_write_data_nack)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    /* ACK address, NACK first data byte */
    g_slave_response[0] = 0; /* addr ACK */
    g_slave_response[1] = 1; /* data NACK */
    g_slave_resp_len = 2;
    uint8_t data[2] = {0xAA, 0xBB};
    ASSERT_EQ(SOFT_I2C_ERR_NACK, soft_i2c_write(&bus, 0x50, data, 2));
}

TEST(test_read_null)
{
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_read(NULL, 0x50, (uint8_t[]){0}, 1));
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_read(&bus, 0x50, NULL, 1));
    uint8_t buf[1];
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_read(&bus, 0x50, buf, 0));
}

TEST(test_read_addr_nack)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 1; /* NACK */
    g_slave_resp_len = 1;
    uint8_t buf[1];
    ASSERT_EQ(SOFT_I2C_ERR_NACK, soft_i2c_read(&bus, 0x50, buf, 1));
}

TEST(test_read_one_byte)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    /* ACK for address, then 8 data bits (all 1 = 0xFF), then post-read */
    g_slave_response[0] = 0; /* addr ACK */
    for (int i = 1; i <= 8; i++) g_slave_response[i] = 1; /* 0xFF data */
    g_slave_resp_len = 9;
    uint8_t buf[1] = {0};
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_read(&bus, 0x50, buf, 1));
    ASSERT_EQ(0xFF, buf[0]);
}

TEST(test_read_two_bytes)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    /* addr ACK + 8 bits (0xA5) + 8 bits (0x5A) */
    g_slave_response[0] = 0; /* addr ACK */
    /* 0xA5 = 10100101 */
    g_slave_response[1] = 1; g_slave_response[2] = 0;
    g_slave_response[3] = 1; g_slave_response[4] = 0;
    g_slave_response[5] = 0; g_slave_response[6] = 1;
    g_slave_response[7] = 0; g_slave_response[8] = 1;
    /* 0x5A = 01011010 */
    g_slave_response[9] = 0; g_slave_response[10] = 1;
    g_slave_response[11] = 0; g_slave_response[12] = 1;
    g_slave_response[13] = 1; g_slave_response[14] = 0;
    g_slave_response[15] = 1; g_slave_response[16] = 0;
    g_slave_resp_len = 17;
    uint8_t buf[2] = {0};
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_read(&bus, 0x50, buf, 2));
    ASSERT_EQ(0xA5, buf[0]);
    ASSERT_EQ(0x5A, buf[1]);
}

TEST(test_write_reg_ok)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    set_slave_acks(3); /* addr + reg + val */
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_write_reg(&bus, 0x68, 0x0D, 0x40));
}

TEST(test_write_reg_null)
{
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_write_reg(NULL, 0x68, 0x0D, 0x40));
}

TEST(test_read_regs_null)
{
    uint8_t buf[2];
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_read_regs(NULL, 0x68, 0x00, buf, 2));
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_read_regs(&bus, 0x68, 0x00, NULL, 2));
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_read_regs(&bus, 0x68, 0x00, buf, 0));
}

TEST(test_read_regs_ok)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    /* addr-write ACK + reg ACK + SDA high before the repeated START +
     * addr-read ACK + 8 data bits */
    g_slave_response[0] = 0; /* addr-write ACK */
    g_slave_response[1] = 0; /* reg ACK */
    g_slave_response[2] = 1; /* SDA high before the repeated START */
    g_slave_response[3] = 0; /* addr-read ACK */
    /* byte = 0x42 = 01000010 */
    g_slave_response[4] = 0; g_slave_response[5] = 1;
    g_slave_response[6] = 0; g_slave_response[7] = 0;
    g_slave_response[8] = 0; g_slave_response[9] = 0;
    g_slave_response[10] = 1; g_slave_response[11] = 0;
    g_slave_resp_len = 12;
    uint8_t buf[1] = {0};
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_read_regs(&bus, 0x68, 0x0F, buf, 1));
    ASSERT_EQ(0x42, buf[0]);
}

TEST(test_bus_recover_null)
{
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_bus_recover(NULL));
}

TEST(test_bus_recover_ok)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    /* SDA stuck low initially — respond high after a few clocks */
    for (int i = 0; i < 4; i++) g_slave_response[i] = 0; /* stuck low */
    g_slave_response[4] = 1; /* released */
    g_slave_resp_len = 5;
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_bus_recover(&bus));
}

TEST(test_write_zero_len)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    set_slave_acks(1); /* addr ACK only */
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_write(&bus, 0x50, NULL, 0));
}

TEST(test_multiple_instances)
{
    soft_i2c_t bus1, bus2;
    mock_reset();
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_init(&bus1, &g_mock_gpio));
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_init(&bus2, &g_mock_gpio));
    ASSERT_TRUE(bus1.gpio == bus2.gpio);
}

/* ---------- new tests ---------- */

/*
 * Test that every individual GPIO function pointer being NULL causes ERR_PARAM.
 * The existing test_init_missing_callback only covers scl_set=NULL.
 */
TEST(test_init_each_null_callback)
{
    soft_i2c_t bus;
    soft_i2c_gpio_t bad;

    bad = g_mock_gpio; bad.sda_set = NULL;
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_init(&bus, &bad));

    bad = g_mock_gpio; bad.sda_dir_out = NULL;
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_init(&bus, &bad));

    bad = g_mock_gpio; bad.sda_dir_in = NULL;
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_init(&bus, &bad));

    bad = g_mock_gpio; bad.sda_get = NULL;
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_init(&bus, &bad));

    bad = g_mock_gpio; bad.delay = NULL;
    ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_init(&bus, &bad));
}

/*
 * write_reg sends addr (ACK[0]), reg (ACK[1]), val (ACK[2]).
 * NACK on the 3rd byte (value) must return ERR_NACK.
 */
TEST(test_write_reg_data_nack)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 0; /* addr ACK */
    g_slave_response[1] = 0; /* reg ACK  */
    g_slave_response[2] = 1; /* val NACK */
    g_slave_resp_len = 3;
    ASSERT_EQ(SOFT_I2C_ERR_NACK, soft_i2c_write_reg(&bus, 0x68, 0x1A, 0x55));
}

/*
 * read_regs protocol:
 *   write-addr (ACK[0]), reg byte (ACK[1]),
 *   repeated-start read-addr (ACK[2]), then 8 data bits.
 * NACK on the repeated-start read-address byte must return ERR_NACK.
 */
TEST(test_read_regs_repeated_start_nack)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 0; /* addr-write ACK */
    g_slave_response[1] = 0; /* reg ACK         */
    g_slave_response[2] = 1; /* SDA high before the repeated START */
    g_slave_response[3] = 1; /* addr-read NACK  */
    g_slave_resp_len = 4;
    uint8_t buf[1] = {0};
    ASSERT_EQ(SOFT_I2C_ERR_NACK, soft_i2c_read_regs(&bus, 0x68, 0x0F, buf, 1));
}

/*
 * NACK on the register byte (2nd byte of write phase) must return ERR_NACK.
 */
TEST(test_read_regs_reg_nack)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 0; /* addr-write ACK */
    g_slave_response[1] = 1; /* reg NACK       */
    g_slave_resp_len = 2;
    uint8_t buf[1] = {0};
    ASSERT_EQ(SOFT_I2C_ERR_NACK, soft_i2c_read_regs(&bus, 0x68, 0x0F, buf, 1));
}

/*
 * SDA low for all 18 read slots and after the STOP: the loop must not
 * break early, and the line never came back, so recovery failed.
 */
TEST(test_bus_recover_line_never_released)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    /* All slave reads return 0 (SDA stuck low for all 18 pulses) */
    memset(g_slave_response, 0, sizeof(g_slave_response));
    g_slave_resp_len = 32;
    g_idle_level = 0;
    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_bus_recover(&bus));
    ASSERT_EQ(18, g_slave_resp_idx);
}

/* The verdict comes from the line after the STOP, not from the loop. */
TEST(test_bus_recover_low_after_stop)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 1; /* released on the first pulse */
    g_slave_resp_len = 1;
    g_idle_level = 0;        /* but low again after the STOP */
    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_bus_recover(&bus));
}

/*
 * SDA held low reads as ACK on every byte, so without a look at the line
 * before START every transfer would report success with zeros.
 */
TEST(test_sda_held_low_refused)
{
    soft_i2c_gpio_t held = g_mock_gpio;
    soft_i2c_t bus;
    uint8_t data = 0x42;
    uint8_t buf[2] = {0xA5, 0xA5};

    held.sda_get = mock_sda_held_low;
    mock_reset();
    soft_i2c_init(&bus, &held);
    g_trace_idx = 0;

    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_write(&bus, 0x50, &data, 1));
    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_write(&bus, 0x50, NULL, 0));
    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_read(&bus, 0x50, buf, 2));
    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_write_reg(&bus, 0x68, 0x0D, 0x40));
    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_read_regs(&bus, 0x68, 0x0F, buf, 2));
    ASSERT_EQ(0xA5, buf[0]);
    ASSERT_EQ(0xA5, buf[1]);
    for (uint16_t i = 0; i < g_trace_idx; i++) {
        ASSERT_TRUE(g_trace[i] != TR_SCL_L); /* the bus never clocked */
    }
    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_bus_recover(&bus));
}

/* A device can also take the line mid-transfer, after the register byte. */
TEST(test_sda_low_before_repeated_start)
{
    soft_i2c_t bus;
    uint8_t buf[1] = {0xA5};

    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 0; /* addr-write ACK */
    g_slave_response[1] = 0; /* reg ACK */
    g_slave_response[2] = 0; /* SDA low before the repeated START */
    g_slave_resp_len = 3;
    ASSERT_EQ(SOFT_I2C_ERR_BUS, soft_i2c_read_regs(&bus, 0x68, 0x0F, buf, 1));
    ASSERT_EQ(0xA5, buf[0]);
    ASSERT_EQ(0, g_scl_fell); /* closed with a STOP */
}

/*
 * The flybox barometer NACKs its command byte. That stays a plain ERR_NACK
 * closed by a STOP, and the next transfer starts normally.
 */
TEST(test_data_nack_then_next_transfer)
{
    soft_i2c_t bus;
    uint8_t cmd = 0xAC;
    uint8_t buf[1] = {0};

    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 0; /* addr ACK */
    g_slave_response[1] = 1; /* command byte NACK */
    g_slave_response[2] = 0; /* next transfer: addr ACK, then 0xFF */
    g_slave_resp_len = 11;
    ASSERT_EQ(SOFT_I2C_ERR_NACK, soft_i2c_write(&bus, 0x78, &cmd, 1));
    ASSERT_EQ(0, g_scl_fell); /* closed with a STOP */
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_read(&bus, 0x78, buf, 1));
    ASSERT_EQ(0xFF, buf[0]);
}

/*
 * Zero-length write where the slave NACKs the address byte.
 * Even with len=0 the address byte is still transmitted and checked.
 */
TEST(test_write_zero_len_addr_nack)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 1; /* addr NACK */
    g_slave_resp_len = 1;
    ASSERT_EQ(SOFT_I2C_ERR_NACK, soft_i2c_write(&bus, 0x50, NULL, 0));
}

/*
 * Read 1 byte where all 8 data bits are 0 → result must be 0x00.
 * ACK response[0], then 8 × 0 data bits.
 */
TEST(test_read_byte_zero)
{
    soft_i2c_t bus;
    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_slave_response[0] = 0; /* addr ACK */
    for (int i = 1; i <= 8; i++) {
        g_slave_response[i] = 0; /* bit = 0 */
    }
    g_slave_resp_len = 9;
    uint8_t buf[1] = {0xFF};
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_read(&bus, 0x50, buf, 1));
    ASSERT_EQ(0x00, buf[0]);
}

/*
 * SOFT_I2C_ERR_BUS (-3) reports SDA held low: before a START, or still low
 * after soft_i2c_bus_recover. Callers branch on this constant.
 */
TEST(test_err_bus_enum_value)
{
    ASSERT_EQ(-3, SOFT_I2C_ERR_BUS);
}

/*
 * Calling init a second time on the same bus variable with a different
 * gpio struct must make the second struct take effect.
 */
TEST(test_reinit_same_bus)
{
    soft_i2c_t bus;
    soft_i2c_gpio_t gpio2 = g_mock_gpio;

    mock_reset();
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_init(&bus, &g_mock_gpio));
    ASSERT_TRUE(bus.gpio == &g_mock_gpio);

    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_init(&bus, &gpio2));
    ASSERT_TRUE(bus.gpio == &gpio2);
}

/*
 * I2C addresses are 7 bits. 0xD0 is the 8-bit form of 0x68: shifted left
 * it would leave 0xA0 on the wire, which selects 0x50, another device.
 * It must be refused before the bus moves at all.
 */
TEST(test_addr_above_7_bits_refused)
{
    static const uint8_t bad[] = {0x80, 0xD0, 0xFF};
    uint8_t data = 0x42;
    uint8_t buf[1] = {0};
    soft_i2c_t bus;

    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    for (size_t i = 0; i < sizeof(bad); i++) {
        g_trace_idx = 0;
        ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_write(&bus, bad[i], &data, 1));
        ASSERT_EQ(SOFT_I2C_ERR_PARAM, soft_i2c_read(&bus, bad[i], buf, 1));
        ASSERT_EQ(SOFT_I2C_ERR_PARAM,
                  soft_i2c_write_reg(&bus, bad[i], 0x0D, 0x40));
        ASSERT_EQ(SOFT_I2C_ERR_PARAM,
                  soft_i2c_read_regs(&bus, bad[i], 0x0F, buf, 1));
        ASSERT_EQ(0, g_trace_idx);
    }
}

/* 0x7F is the top legal address: it must reach the wire as 0xFE. */
TEST(test_addr_7f_still_sent)
{
    soft_i2c_t bus;
    uint8_t wire = 0;
    int bits = 0;

    mock_reset();
    soft_i2c_init(&bus, &g_mock_gpio);
    g_trace_idx = 0;
    set_slave_acks(1);
    ASSERT_EQ(SOFT_I2C_OK, soft_i2c_write(&bus, 0x7F, NULL, 0));

    /* START ends with SCL falling. After that each bit is one SDA level
     * clocked by one rising SCL edge. */
    uint16_t i = 0;
    while (i < g_trace_idx && g_trace[i] != TR_SCL_L) i++;
    uint8_t level = 0;
    for (; i < g_trace_idx && bits < 8; i++) {
        if (g_trace[i] == TR_SDA_H) level = 1;
        if (g_trace[i] == TR_SDA_L) level = 0;
        if (g_trace[i] == TR_SCL_H) {
            wire = (uint8_t)((wire << 1) | level);
            bits++;
        }
    }
    ASSERT_EQ(8, bits);
    ASSERT_EQ(0xFE, wire);
}

TEST_MAIN("soft_i2c")
    RUN(test_addr_above_7_bits_refused);
    RUN(test_addr_7f_still_sent);
    RUN(test_init_null_bus);
    RUN(test_init_null_gpio);
    RUN(test_init_missing_callback);
    RUN(test_init_ok);
    RUN(test_write_null_bus);
    RUN(test_write_null_data_nonzero_len);
    RUN(test_write_addr_nack);
    RUN(test_write_one_byte_ok);
    RUN(test_write_multi_byte);
    RUN(test_write_data_nack);
    RUN(test_read_null);
    RUN(test_read_addr_nack);
    RUN(test_read_one_byte);
    RUN(test_read_two_bytes);
    RUN(test_write_reg_ok);
    RUN(test_write_reg_null);
    RUN(test_read_regs_null);
    RUN(test_read_regs_ok);
    RUN(test_bus_recover_null);
    RUN(test_bus_recover_ok);
    RUN(test_write_zero_len);
    RUN(test_multiple_instances);
    RUN(test_init_each_null_callback);
    RUN(test_write_reg_data_nack);
    RUN(test_read_regs_repeated_start_nack);
    RUN(test_read_regs_reg_nack);
    RUN(test_bus_recover_line_never_released);
    RUN(test_bus_recover_low_after_stop);
    RUN(test_sda_held_low_refused);
    RUN(test_sda_low_before_repeated_start);
    RUN(test_data_nack_then_next_transfer);
    RUN(test_write_zero_len_addr_nack);
    RUN(test_read_byte_zero);
    RUN(test_err_bus_enum_value);
    RUN(test_reinit_same_bus);
TEST_MAIN_END()
