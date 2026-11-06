// SPDX-License-Identifier: MIT
/*
 * test_hw_model_ccs811.c — driver against the hardware model
 *
 * Checks what hw_model_ccs811.h takes from ams CCS811 datasheet v1-00: the
 * change from boot to application mode needs the single byte 0xF4, ENV_DATA
 * takes its 4 bytes in one write, registers are mailboxes with no address
 * increment, and a sample comes at the measurement interval.
 */

#include "test_harness.h"
#include "hw_model_ccs811.h"
#include "../ccs811.h"

static ccs811_model_t model;

static const ccs811_i2c_t model_i2c = {
    .write_reg = ccs811_model_write_reg,
    .read_regs = ccs811_model_read_regs,
    .delay_ms  = ccs811_model_delay_ms,
    .ctx       = &model.hw,
    .write_raw = ccs811_model_write_raw,
};

static const ccs811_cfg_t cfg = {
    .addr = CCS811_ADDR_PRIMARY,
    .mode = CCS811_MODE_1SEC,
};

#define ADDR CCS811_ADDR_PRIMARY

static uint8_t rd(uint8_t reg)
{
    uint8_t val = 0;
    (void)ccs811_model_read_regs(&model.hw, ADDR, reg, &val, 1);
    return val;
}

static void wr(uint8_t reg, uint8_t val)
{
    (void)ccs811_model_write_reg(&model.hw, ADDR, reg, val);
}

/* The model in application mode, by the bus */
static void app_mode(void)
{
    static const uint8_t app_start = CCS811_REG_APP_START;

    ccs811_model_init(&model);
    (void)ccs811_model_write_raw(&model.hw, ADDR, &app_start, 1);
}

static uint8_t status(void) { return rd(0x00); }
static uint8_t error_id(void) { return rd(0xE0); }

TEST(test_init_starts_the_application)
{
    ccs811_t d;

    ccs811_model_init(&model);
    ASSERT_FALSE(model.app_mode);
    ASSERT_EQ(ccs811_init(&d, &model_i2c, &cfg), CCS811_OK);
    ASSERT_TRUE(model.app_mode);
    ASSERT_EQ_HEX(model.hw.regs[CCS811_REG_STATUS] & 0x90u, 0x90u);
}

TEST(test_a_write_with_data_does_not_start_the_application)
{
    /* p24: "a write with no data is required" */
    static const uint8_t with_data[] = { CCS811_REG_APP_START, 0x00 };

    ccs811_model_init(&model);
    ASSERT_TRUE(ccs811_model_write_raw(&model.hw, CCS811_ADDR_PRIMARY,
                                       with_data, sizeof(with_data)));
    ASSERT_TRUE(ccs811_model_write_reg(&model.hw, CCS811_ADDR_PRIMARY,
                                       CCS811_REG_APP_START, 0x00));
    ASSERT_FALSE(model.app_mode);
}

TEST(test_env_data_reaches_the_mailbox_whole)
{
    static const uint8_t defaults[4] = { 0x64, 0x00, 0x64, 0x00 };
    static const uint8_t want[4]     = { 0x50, 0x00, 0x5A, 0x00 };
    ccs811_t d;

    ccs811_model_init(&model);
    ASSERT_MEMEQ_N(model.env_data, defaults, 4);
    ASSERT_EQ(ccs811_init(&d, &model_i2c, &cfg), CCS811_OK);
    ASSERT_EQ(ccs811_set_env_data(&d, 80, 90), CCS811_OK);
    ASSERT_EQ(model.env_write_count, 1);
    ASSERT_MEMEQ_N(model.env_data, want, 4);
}

/* ---------- mailboxes, p13: "the address does not increment" ---------- */

/* p18: eCO2 high and low, TVOC high and low, STATUS, ERROR_ID, RAW_DATA */
TEST(test_alg_result_streams_its_own_eight_bytes)
{
    uint8_t buf[8];

    app_mode();
    ccs811_model_set_air_quality(&model, 0x1234, 0x0056);
    ccs811_model_set_raw(&model, 0xABCD);
    wr(0x01, 0x10);
    ccs811_model_advance_ms(&model, 1000);

    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x02, buf, 8));
    ASSERT_EQ_HEX(buf[0], 0x12);
    ASSERT_EQ_HEX(buf[1], 0x34);
    ASSERT_EQ_HEX(buf[2], 0x00);
    ASSERT_EQ_HEX(buf[3], 0x56);
    ASSERT_EQ_HEX(buf[4], 0x98);  /* FW_MODE, APP_VALID, DATA_READY */
    ASSERT_EQ_HEX(buf[5], 0x00);
    ASSERT_EQ_HEX(buf[6], 0xAB);
    ASSERT_EQ_HEX(buf[7], 0xCD);
}

/* p15: 0x03 is RAW_DATA, not the low byte of eCO2 */
TEST(test_raw_data_is_its_own_mailbox)
{
    uint8_t buf[2];

    app_mode();
    ccs811_model_set_air_quality(&model, 0x1234, 0x0056);
    ccs811_model_set_raw(&model, 0xABCD);
    wr(0x01, 0x10);
    ccs811_model_advance_ms(&model, 1000);

    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x03, buf, 2));
    ASSERT_EQ_HEX(buf[0], 0xAB);
    ASSERT_EQ_HEX(buf[1], 0xCD);
}

/* p15: ENV_DATA is type W, and p22: "an I2C read request to a mailbox ID that
 * is invalid" is READ_REG_INVALID */
TEST(test_env_data_cannot_be_read)
{
    app_mode();
    ASSERT_EQ_HEX(status() & 0x01, 0x00);
    ASSERT_EQ(rd(0x05), 0);
    ASSERT_EQ_HEX(status() & 0x01, 0x01);
    ASSERT_EQ_HEX(error_id(), 0x02);
}

/* p15: the map has no 0x07. p22: WRITE_REG_INVALID. */
TEST(test_there_is_no_register_0x07)
{
    app_mode();
    wr(0x07, 0x64);
    ASSERT_EQ_HEX(status() & 0x01, 0x01);
    ASSERT_EQ_HEX(error_id(), 0x01);
    (void)rd(0x07);
    ASSERT_EQ_HEX(error_id(), 0x03);
}

/* p15 note 1: "The number of bytes read from a register must not exceed the
 * size in this table." */
TEST(test_a_read_past_the_size_is_counted)
{
    uint8_t buf[9];

    app_mode();
    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x02, buf, 8));
    ASSERT_EQ(model.over_reads, 0);
    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x02, buf, 9));
    ASSERT_EQ(model.over_reads, 1);
    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x20, buf, 2));
    ASSERT_EQ(model.over_reads, 2);
}

/* p15: STATUS, HW_ID and ALG_RESULT_DATA are type R */
TEST(test_read_only_mailboxes_refuse_writes)
{
    app_mode();
    wr(0x00, 0xFF);
    ASSERT_EQ_HEX(error_id(), 0x01);

    app_mode();
    wr(0x20, 0x00);
    ASSERT_EQ_HEX(rd(0x20), 0x81);
    ASSERT_EQ_HEX(error_id(), 0x01);

    app_mode();
    wr(0x02, 0x00);
    ASSERT_EQ_HEX(error_id(), 0x01);
}

/* p24: the bootloader map has none of the measurement registers */
TEST(test_boot_mode_has_no_measurement_registers)
{
    ccs811_model_init(&model);
    wr(0x01, 0x10);
    ASSERT_EQ_HEX(error_id(), 0x01);
    ccs811_model_advance_ms(&model, 2000);
    ASSERT_EQ_HEX(status() & 0x08, 0x00);

    ccs811_model_init(&model);
    (void)rd(0x02);
    ASSERT_EQ_HEX(error_id(), 0x02);
}

/* p15: 0xF4 is only in the bootloader map of p24 */
TEST(test_application_mode_has_no_app_start)
{
    static const uint8_t app_start = CCS811_REG_APP_START;

    app_mode();
    ASSERT_EQ_HEX(error_id(), 0x00);
    ASSERT_TRUE(ccs811_model_write_raw(&model.hw, ADDR, &app_start, 1));
    ASSERT_EQ_HEX(error_id(), 0x01);
}

/* p16: APP_VALID 0 is "No application firmware loaded" */
TEST(test_no_application_means_no_start)
{
    static const uint8_t app_start = CCS811_REG_APP_START;

    ccs811_model_init(&model);
    ccs811_model_set_app_valid(&model, false);
    ASSERT_EQ_HEX(status() & 0x10, 0x00);
    ASSERT_TRUE(ccs811_model_write_raw(&model.hw, ADDR, &app_start, 1));
    ASSERT_EQ_HEX(status() & 0x80, 0x00);
}

/* ---------- samples, p16 and p17 ---------- */

/* p17: "the DATA_READY bit in the STATUS register is set at the defined
 * measurement interval" */
TEST(test_a_sample_waits_for_the_interval)
{
    static const struct { uint8_t meas_mode; uint32_t ms; } modes[] = {
        {0x10, 1000}, {0x20, 10000}, {0x30, 60000}, {0x40, 250},
    };
    for (size_t i = 0; i < 4; i++) {
        app_mode();
        wr(0x01, modes[i].meas_mode);
        ASSERT_EQ_HEX(status() & 0x08, 0x00);
        ccs811_model_advance_ms(&model, modes[i].ms - 1);
        ASSERT_EQ_HEX(status() & 0x08, 0x00);
        ccs811_model_advance_ms(&model, 1);
        ASSERT_EQ_HEX(status() & 0x08, 0x08);
    }
}

/* p16: "this bit is cleared when ALG_RESULT_DATA is read" */
TEST(test_reading_the_result_clears_data_ready)
{
    uint8_t buf[4];

    app_mode();
    wr(0x01, 0x10);
    ccs811_model_advance_ms(&model, 1000);
    ASSERT_EQ_HEX(status() & 0x08, 0x08);
    ASSERT_EQ_HEX(status() & 0x08, 0x08);  /* STATUS alone does not clear it */

    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x02, buf, 4));
    ASSERT_EQ_HEX(status() & 0x08, 0x00);

    ccs811_model_advance_ms(&model, 1000);
    ASSERT_EQ_HEX(status() & 0x08, 0x08);
}

/* p17: "A new sample is placed in ALG_RESULT_DATA and RAW_DATA registers" */
TEST(test_the_result_changes_only_with_a_sample)
{
    uint8_t buf[2];

    app_mode();
    ccs811_model_set_air_quality(&model, 500, 0);
    wr(0x01, 0x10);
    ccs811_model_advance_ms(&model, 1000);
    ccs811_model_set_air_quality(&model, 900, 0);
    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x02, buf, 2));
    ASSERT_EQ((buf[0] << 8) | buf[1], 500);

    ccs811_model_advance_ms(&model, 1000);
    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x02, buf, 2));
    ASSERT_EQ((buf[0] << 8) | buf[1], 900);
}

/* p17: "In mode 4, the ALG_RESULT_DATA is not updated, only RAW_DATA" */
TEST(test_mode_4_updates_only_raw_data)
{
    uint8_t buf[2];

    app_mode();
    ccs811_model_set_air_quality(&model, 500, 0);
    ccs811_model_set_raw(&model, 0x0123);
    wr(0x01, 0x10);
    ccs811_model_advance_ms(&model, 1000);

    ccs811_model_set_air_quality(&model, 900, 0);
    ccs811_model_set_raw(&model, 0x0456);
    wr(0x01, 0x40);
    ccs811_model_advance_ms(&model, 250);

    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x02, buf, 2));
    ASSERT_EQ((buf[0] << 8) | buf[1], 500);
    ASSERT_TRUE(ccs811_model_read_regs(&model.hw, ADDR, 0x03, buf, 2));
    ASSERT_EQ_HEX((buf[0] << 8) | buf[1], 0x0456);
}

/* p17: Mode 0, "Measurements are disabled in this mode" */
TEST(test_idle_takes_no_samples)
{
    app_mode();
    wr(0x01, 0x00);
    ccs811_model_advance_ms(&model, 120000);
    ASSERT_EQ_HEX(status() & 0x08, 0x00);
}

/* p17: 1xx are reserved modes, and p22: MEASMODE_INVALID */
TEST(test_a_reserved_mode_is_refused)
{
    app_mode();
    wr(0x01, 0x10);
    wr(0x01, 0x50);
    ASSERT_EQ_HEX(rd(0x01), 0x10);
    ASSERT_EQ_HEX(error_id(), 0x04);
}

/* p17: bit 7 and bits 1:0 are reserved */
TEST(test_meas_mode_keeps_reserved_bits_zero)
{
    app_mode();
    wr(0x01, 0x9F);
    ASSERT_EQ_HEX(rd(0x01), 0x1C);
}

/* ---------- the driver on the model ---------- */

static ccs811_t dev;

TEST(test_read_gets_the_sample)
{
    ccs811_data_t data;

    ccs811_model_init(&model);
    ccs811_model_set_air_quality(&model, 450, 10);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &cfg), CCS811_OK);
    ccs811_model_advance_ms(&model, 1000);
    ASSERT_EQ(ccs811_read(&dev, &data), CCS811_OK);
    ASSERT_EQ(data.eco2_ppm, 450);
    ASSERT_EQ(data.tvoc_ppb, 10);
}

TEST(test_read_before_the_first_sample_is_not_ready)
{
    ccs811_data_t data;

    ccs811_model_init(&model);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &cfg), CCS811_OK);
    ASSERT_EQ(ccs811_read(&dev, &data), CCS811_ERR_NOT_READY);
}

TEST(test_one_sample_reads_once)
{
    ccs811_data_t data;

    ccs811_model_init(&model);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &cfg), CCS811_OK);
    ccs811_model_advance_ms(&model, 1000);
    ASSERT_EQ(ccs811_read(&dev, &data), CCS811_OK);
    ASSERT_EQ(ccs811_read(&dev, &data), CCS811_ERR_NOT_READY);
    ccs811_model_advance_ms(&model, 1000);
    ASSERT_EQ(ccs811_read(&dev, &data), CCS811_OK);
}

/* p17: mode 4 has no result to read */
TEST(test_read_refuses_mode_4)
{
    ccs811_data_t data;
    ccs811_cfg_t raw = cfg;

    raw.mode = CCS811_MODE_250MS;
    ccs811_model_init(&model);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &raw), CCS811_OK);
    ccs811_model_advance_ms(&model, 250);
    ASSERT_EQ(ccs811_read(&dev, &data), CCS811_ERR_MODE);
}

TEST(test_set_mode_reaches_the_chip)
{
    ccs811_model_init(&model);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &cfg), CCS811_OK);
    ASSERT_EQ_HEX(rd(0x01), 0x10);
    ASSERT_EQ(ccs811_set_mode(&dev, CCS811_MODE_60SEC), CCS811_OK);
    ASSERT_EQ_HEX(rd(0x01), 0x30);
}

/* Everything the driver does uses only addresses in the map, within their size */
TEST(test_normal_use_raises_no_error)
{
    ccs811_data_t data;

    ccs811_model_init(&model);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &cfg), CCS811_OK);
    ccs811_model_advance_ms(&model, 1000);
    ASSERT_EQ(ccs811_read(&dev, &data), CCS811_OK);
    ASSERT_EQ(ccs811_set_env_data(&dev, 100, 100), CCS811_OK);
    ASSERT_EQ(ccs811_set_mode(&dev, CCS811_MODE_10SEC), CCS811_OK);
    ASSERT_EQ_HEX(error_id(), 0x00);
    ASSERT_EQ_HEX(status() & 0x01, 0x00);
    ASSERT_EQ(model.over_reads, 0);
}

/* An MCU restart meets a chip still in application mode, where p15 has no 0xF4,
 * so init must not send it. p22: WRITE_REG_INVALID. */
TEST(test_init_of_a_running_chip_raises_no_error)
{
    ccs811_model_init(&model);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &cfg), CCS811_OK);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &cfg), CCS811_OK);
    ASSERT_EQ_HEX(error_id(), 0x00);
    ASSERT_EQ_HEX(status() & 0x01, 0x00);
}

/* p16: with no application there is nothing to start */
TEST(test_init_refuses_a_chip_with_no_application)
{
    ccs811_model_init(&model);
    ccs811_model_set_app_valid(&model, false);
    ASSERT_EQ(ccs811_init(&dev, &model_i2c, &cfg), CCS811_ERR_BOOT);
}

TEST_MAIN("hw_model_ccs811")
    RUN(test_init_starts_the_application);
    RUN(test_a_write_with_data_does_not_start_the_application);
    RUN(test_env_data_reaches_the_mailbox_whole);
    RUN(test_alg_result_streams_its_own_eight_bytes);
    RUN(test_raw_data_is_its_own_mailbox);
    RUN(test_env_data_cannot_be_read);
    RUN(test_there_is_no_register_0x07);
    RUN(test_a_read_past_the_size_is_counted);
    RUN(test_read_only_mailboxes_refuse_writes);
    RUN(test_boot_mode_has_no_measurement_registers);
    RUN(test_application_mode_has_no_app_start);
    RUN(test_no_application_means_no_start);
    RUN(test_a_sample_waits_for_the_interval);
    RUN(test_reading_the_result_clears_data_ready);
    RUN(test_the_result_changes_only_with_a_sample);
    RUN(test_mode_4_updates_only_raw_data);
    RUN(test_idle_takes_no_samples);
    RUN(test_a_reserved_mode_is_refused);
    RUN(test_meas_mode_keeps_reserved_bits_zero);
    RUN(test_read_gets_the_sample);
    RUN(test_read_before_the_first_sample_is_not_ready);
    RUN(test_one_sample_reads_once);
    RUN(test_read_refuses_mode_4);
    RUN(test_set_mode_reaches_the_chip);
    RUN(test_normal_use_raises_no_error);
    RUN(test_init_of_a_running_chip_raises_no_error);
    RUN(test_init_refuses_a_chip_with_no_application);
TEST_MAIN_END()
