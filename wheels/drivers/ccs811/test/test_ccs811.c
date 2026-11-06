// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "ccs811.h"

/* ---------- chip-specific mock setup ---------- */

/*
 * The mock chip. Every I2C write lands in bus_log as the exact bytes after
 * the address, whichever HAL callback carried them, so a test sees what the
 * chip would see (datasheet v1-00 p13: register address, then any data).
 *
 * STATUS starts in boot mode. Only the single byte 0xF4 moves it to
 * application mode (p24, p25); a write that carries data does not.
 */
#define BUS_LOG_MAX 8
#define BUS_TX_MAX  8

typedef struct {
    uint8_t  addr;
    uint16_t len;
    uint8_t  data[BUS_TX_MAX];
} bus_tx_t;

static bus_tx_t bus_log[BUS_LOG_MAX];
static int      bus_count;
static bool     chip_refuses_app_start;

static void bus_clear(void)
{
    memset(bus_log, 0, sizeof(bus_log));
    bus_count = 0;
}

static bool bus_write(uint8_t dev_addr, const uint8_t *data, uint16_t len)
{
    if (mock_i2c_should_fail()) return false;

    if (bus_count < BUS_LOG_MAX) {
        bus_tx_t *tx = &bus_log[bus_count];
        tx->addr = dev_addr;
        tx->len  = len;
        memcpy(tx->data, data, len < BUS_TX_MAX ? len : BUS_TX_MAX);
    }
    bus_count++;

    if (len == 1 && data[0] == 0xF4 && !chip_refuses_app_start) {
        mock_regs[0x00] |= 0x80;      /* FW_MODE */
    } else if (len == 2 && data[0] == 0x01) {
        mock_regs[0x01] = data[1];    /* MEAS_MODE */
    }
    return true;
}

static bool bus_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    const uint8_t tx[2] = { reg, val };

    (void)ctx;
    return bus_write(dev_addr, tx, 2);
}

static bool bus_write_raw(void *ctx, uint8_t dev_addr, const uint8_t *data,
                          uint16_t len)
{
    (void)ctx;
    return bus_write(dev_addr, data, len);
}

/* Assert that write transaction i was exactly these bytes */
#define ASSERT_TX(i, ...) do { \
    static const uint8_t want_[] = { __VA_ARGS__ }; \
    ASSERT_TRUE((i) < bus_count); \
    ASSERT_EQ(bus_log[i].len, sizeof(want_)); \
    ASSERT_MEMEQ_N(bus_log[i].data, want_, sizeof(want_)); \
} while (0)

static void mock_reset(void)
{
    mock_i2c_reset();
    bus_clear();
    chip_refuses_app_start = false;
    mock_regs[0x20] = 0x81;  /* HW_ID */
    mock_regs[0x00] = 0x10;  /* STATUS: APP_VALID(0x10), still in boot mode */
}

static const ccs811_i2c_t mock_i2c = {
    .write_reg = bus_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
    .write_raw = bus_write_raw,
};

static ccs811_t dev;
static ccs811_cfg_t default_cfg = {
    .addr = CCS811_ADDR_PRIMARY,
    .mode = CCS811_MODE_1SEC,
};

static void setup(void)
{
    mock_reset();
    ccs811_init(&dev, &mock_i2c, &default_cfg);
}

/* Plant measurement data in ALG_RESULT registers */
static void plant_data(uint16_t eco2, uint16_t tvoc)
{
    mock_regs[0x02] = (uint8_t)(eco2 >> 8);
    mock_regs[0x03] = (uint8_t)(eco2 & 0xFF);
    mock_regs[0x04] = (uint8_t)(tvoc >> 8);
    mock_regs[0x05] = (uint8_t)(tvoc & 0xFF);
    /* Set DATA_READY bit in STATUS */
    mock_regs[0x00] |= 0x08;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x20] = 0x00;
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_I2C);
}

TEST(test_init_app_not_valid)
{
    mock_reset();
    mock_regs[0x00] = 0x80;  /* FW_MODE but no APP_VALID */
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_BOOT);
}

TEST(test_init_fw_mode_not_set)
{
    mock_reset();
    chip_refuses_app_start = true;  /* FW_MODE stays 0 after APP_START */
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, CCS811_ERR_BOOT);
}

TEST(test_app_start_is_one_byte_with_no_data)
{
    ccs811_t d;

    mock_reset();
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &default_cfg);
    /* p25: "a single byte write of 0xF4 is required" */
    ASSERT_TX(0, 0xF4);
    ASSERT_EQ(bus_log[0].addr, CCS811_ADDR_PRIMARY);
    ASSERT_EQ(e, CCS811_OK);
    /* p13: a single-byte register takes its address and one data byte */
    ASSERT_TX(1, 0x01, 0x10);
    ASSERT_EQ(bus_count, 2);
}

TEST(test_app_start_goes_to_the_configured_address)
{
    ccs811_cfg_t cfg = { .addr = CCS811_ADDR_ALT, .mode = CCS811_MODE_1SEC };
    ccs811_t d;

    mock_reset();
    ASSERT_EQ(ccs811_init(&d, &mock_i2c, &cfg), CCS811_OK);
    ASSERT_TX(0, 0xF4);
    ASSERT_EQ(bus_log[0].addr, CCS811_ADDR_ALT);
}

TEST(test_init_refuses_a_hal_without_write_raw)
{
    ccs811_i2c_t hal = mock_i2c;
    ccs811_t d;

    hal.write_raw = NULL;
    mock_reset();
    ASSERT_EQ(ccs811_init(&d, &hal, &default_cfg), CCS811_ERR_PARAM);
    /* nothing on the bus at all, reads included */
    ASSERT_EQ(mock_op_count, 0);
    ASSERT_EQ(bus_count, 0);
}

TEST(test_init_app_start_i2c_fail)
{
    ccs811_t d;

    mock_reset();
    mock_fail_at = 2;  /* HW_ID read, STATUS read, then APP_START */
    ASSERT_EQ(ccs811_init(&d, &mock_i2c, &default_cfg), CCS811_ERR_I2C);
    ASSERT_EQ(bus_count, 0);
}

TEST(test_null_safety)
{
    ASSERT_EQ(ccs811_init(NULL, &mock_i2c, &default_cfg), CCS811_ERR_PARAM);
    ccs811_t d;
    ASSERT_EQ(ccs811_init(&d, NULL, &default_cfg), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_init(&d, &mock_i2c, NULL), CCS811_ERR_PARAM);
    ccs811_data_t data;
    ASSERT_EQ(ccs811_read(NULL, &data), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_read(&d, NULL), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_set_mode(NULL, CCS811_MODE_1SEC), CCS811_ERR_PARAM);
    ASSERT_EQ(ccs811_set_env_data(NULL, 0, 0), CCS811_ERR_PARAM);
}

TEST(test_read_ok)
{
    setup();
    plant_data(450, 10);
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_OK);
    ASSERT_EQ(data.eco2_ppm, 450);
    ASSERT_EQ(data.tvoc_ppb, 10);
}

TEST(test_read_not_ready)
{
    setup();
    /* DATA_READY bit is NOT set */
    mock_regs[0x00] = 0x90;  /* FW_MODE | APP_VALID, but no DATA_READY */
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_ERR_NOT_READY);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_ERR_I2C);
}

TEST(test_set_mode)
{
    setup();
    ccs811_err_t e = ccs811_set_mode(&dev, CCS811_MODE_10SEC);
    ASSERT_EQ(e, CCS811_OK);
    /* MEAS_MODE: mode 2 shifted left 4 = 0x20 */
    ASSERT_EQ(mock_regs[0x01], 0x20);
}

TEST(test_set_mode_invalid)
{
    setup();
    ccs811_err_t e = ccs811_set_mode(&dev, (ccs811_drive_mode_t)99);
    ASSERT_EQ(e, CCS811_ERR_PARAM);
}

TEST(test_set_mode_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ccs811_err_t e = ccs811_set_mode(&dev, CCS811_MODE_60SEC);
    ASSERT_EQ(e, CCS811_ERR_I2C);
}

TEST(test_meas_mode_register)
{
    setup();
    /* After init with MODE_1SEC, MEAS_MODE should be (1 << 4) = 0x10 */
    ASSERT_EQ(mock_regs[0x01], 0x10);
}

TEST(test_addr_alt)
{
    mock_reset();
    ccs811_cfg_t cfg = {
        .addr = CCS811_ADDR_ALT,
        .mode = CCS811_MODE_1SEC,
    };
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, CCS811_OK);
}

TEST(test_init_invalid_mode)
{
    mock_reset();
    ccs811_cfg_t cfg = {
        .addr = CCS811_ADDR_PRIMARY,
        .mode = (ccs811_drive_mode_t)99,
    };
    ccs811_t d;
    ccs811_err_t e = ccs811_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, CCS811_ERR_PARAM);
}

TEST(test_read_high_values)
{
    setup();
    plant_data(8192, 1187);
    ccs811_data_t data;
    ccs811_err_t e = ccs811_read(&dev, &data);
    ASSERT_EQ(e, CCS811_OK);
    ASSERT_EQ(data.eco2_ppm, 8192);
    ASSERT_EQ(data.tvoc_ppb, 1187);
}

TEST(test_read_refuses_the_raw_data_mode)
{
    ccs811_cfg_t cfg = { .addr = CCS811_ADDR_PRIMARY, .mode = CCS811_MODE_250MS };
    ccs811_t d;
    ccs811_data_t data;

    mock_reset();
    ASSERT_EQ(ccs811_init(&d, &mock_i2c, &cfg), CCS811_OK);
    /* mode 4 updates only RAW_DATA, so ALG_RESULT_DATA would be stale */
    ASSERT_EQ(ccs811_read(&d, &data), CCS811_ERR_MODE);
}

TEST_MAIN("ccs811")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_init_app_not_valid);
    RUN(test_init_fw_mode_not_set);
    RUN(test_app_start_is_one_byte_with_no_data);
    RUN(test_app_start_goes_to_the_configured_address);
    RUN(test_init_refuses_a_hal_without_write_raw);
    RUN(test_init_app_start_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_ok);
    RUN(test_read_not_ready);
    RUN(test_read_i2c_fail);
    RUN(test_set_mode);
    RUN(test_set_mode_invalid);
    RUN(test_set_mode_i2c_fail);
    RUN(test_meas_mode_register);
    RUN(test_addr_alt);
    RUN(test_init_invalid_mode);
    RUN(test_read_high_values);
    RUN(test_read_refuses_the_raw_data_mode);
TEST_MAIN_END()
