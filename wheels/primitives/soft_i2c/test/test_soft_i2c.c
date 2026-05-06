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

static uint8_t g_trace[TRACE_MAX];
static uint16_t g_trace_idx;

enum { TR_SCL_H = 1, TR_SCL_L, TR_SDA_H, TR_SDA_L, TR_SDA_IN, TR_SDA_OUT, TR_DELAY };

static void mock_scl_set(void *ctx, bool high)
{
    (void)ctx;
    g_scl_state = high ? 1 : 0;
    if (g_trace_idx < TRACE_MAX) {
        g_trace[g_trace_idx++] = high ? TR_SCL_H : TR_SCL_L;
    }
}

static void mock_sda_set(void *ctx, bool high)
{
    (void)ctx;
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
    if (g_slave_resp_idx < g_slave_resp_len) {
        return g_slave_response[g_slave_resp_idx++] != 0;
    }
    return true; /* default: NACK / high */
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
    /* addr-write ACK + reg ACK + addr-read ACK + 8 data bits */
    g_slave_response[0] = 0; /* addr-write ACK */
    g_slave_response[1] = 0; /* reg ACK */
    g_slave_response[2] = 0; /* addr-read ACK */
    /* byte = 0x42 = 01000010 */
    g_slave_response[3] = 0; g_slave_response[4] = 1;
    g_slave_response[5] = 0; g_slave_response[6] = 0;
    g_slave_response[7] = 0; g_slave_response[8] = 0;
    g_slave_response[9] = 1; g_slave_response[10] = 0;
    g_slave_resp_len = 11;
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

TEST_MAIN("soft_i2c")
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
TEST_MAIN_END()
