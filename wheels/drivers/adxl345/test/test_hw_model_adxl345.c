// SPDX-License-Identifier: MIT
/*
 * test_hw_model_adxl345.c -- the driver against the data sheet model, and the
 * model against ADXL345 data sheet Rev. G
 */
#include "test_harness.h"
#include "hw_model_adxl345.h"
#include "adxl345.h"
#include <math.h>

static adxl345_model_t model;
static adxl345_t dev;

/* The HAL checks what the model cannot see for itself */
static int     reserved_access;  /* Table 19: 0x01 to 0x1C, "do not access" */
static int     six_byte_reads;   /* p.27: one multiple-byte read of all six */
static uint8_t last_addr;

static void check_access(uint8_t addr, uint8_t reg)
{
    last_addr = addr;
    if (reg >= 0x01 && reg <= 0x1C) reserved_access++;
}

static bool hal_write(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    check_access(addr, reg);
    return hw_model_i2c_write_reg(ctx, addr, reg, val);
}

static bool hal_read(void *ctx, uint8_t addr, uint8_t reg, uint8_t *buf,
                     uint16_t len)
{
    check_access(addr, reg);
    if (reg == ADXL345_REG_DATAX0 && len == 6) six_byte_reads++;
    return adxl345_model_read_regs(ctx, addr, reg, buf, len);
}

static const adxl345_i2c_t hal = {
    .write_reg = hal_write,
    .read_regs = hal_read,
    .delay_ms  = adxl345_model_delay_ms,
    .ctx       = &model.hw,
};

static void start(void)
{
    adxl345_model_init(&model);
    reserved_access = 0;
    six_byte_reads = 0;
    last_addr = 0;
}

static uint8_t reg_read(uint8_t reg)
{
    uint8_t val = 0;
    (void)hw_model_i2c_read_regs(&model.hw, 0x53, reg, &val, 1);
    return val;
}

static void reg_write(uint8_t reg, uint8_t val)
{
    (void)hw_model_i2c_write_reg(&model.hw, 0x53, reg, val);
}

/* An output pair, low byte first and twos complement, p.27 */
static int count(uint8_t reg)
{
    uint16_t raw = (uint16_t)((uint16_t)(reg_read((uint8_t)(reg + 1)) << 8) |
                              reg_read(reg));
    return (int)(int16_t)raw;
}

#define G(n) ((int16_t)((n) * 256))  /* the model's light is in 1/256 g */

/* Measurement mode at the given DATA_FORMAT, past the turn-on time */
static void measure(uint8_t format)
{
    reg_write(ADXL345_REG_DATA_FORMAT, format);
    reg_write(ADXL345_REG_POWER_CTL, 0x08);
    adxl345_model_advance_us(&model, 11100);
}

/* ---------- the model ---------- */

/* Table 19, p.23 */
TEST(test_hw_reset_values)
{
    start();
    ASSERT_EQ_HEX(reg_read(0x00), 0xE5);
    ASSERT_EQ_HEX(reg_read(0x2C), 0x0A);  /* p.25: "The default value is 0x0A" */
    ASSERT_EQ_HEX(reg_read(0x2D), 0x00);
    ASSERT_EQ_HEX(reg_read(0x30), 0x02);
    ASSERT_EQ_HEX(reg_read(0x31), 0x00);
    for (uint8_t reg = 0x32; reg <= 0x39; reg++) {
        ASSERT_EQ_HEX(reg_read(reg), 0x00);
    }
}

/* Table 19, p.23: type R */
TEST(test_hw_read_only_registers_ignore_writes)
{
    static const uint8_t ro[] = {0x00, 0x2B, 0x30, 0x32, 0x33, 0x34, 0x35,
                                 0x36, 0x37, 0x39};
    start();
    for (size_t i = 0; i < sizeof(ro); i++) {
        uint8_t before = reg_read(ro[i]);
        reg_write(ro[i], (uint8_t)~before);
        ASSERT_EQ_HEX(reg_read(ro[i]), before);
    }
}

/* Tables 25, 26 and 34: the bits printed as 0 */
TEST(test_hw_fixed_zero_bits_stay_zero)
{
    start();
    reg_write(0x2C, 0xFF);
    ASSERT_EQ_HEX(reg_read(0x2C), 0x1F);
    reg_write(0x2D, 0xF7);
    ASSERT_EQ_HEX(reg_read(0x2D), 0x37);
    reg_write(0x31, 0xFF);
    ASSERT_EQ_HEX(reg_read(0x31), 0xEF);
}

/* p.26: "The ADXL345 powers up in standby mode" */
TEST(test_hw_standby_produces_no_data)
{
    start();
    adxl345_model_advance_us(&model, 1000000);
    ASSERT_EQ(count(0x36), 0);
    ASSERT_EQ_HEX(reg_read(0x30) & 0x80, 0x00);
}

/* p.4: "At a 100 Hz data rate, the turn-on and wake-up times are each
 * approximately 11.1 ms." */
TEST(test_hw_turn_on_time_at_100_hz)
{
    start();
    reg_write(0x2D, 0x08);
    ASSERT_EQ(count(0x36), 0);
    adxl345_model_advance_us(&model, 11000);
    ASSERT_EQ(count(0x36), 0);
    ASSERT_EQ_HEX(reg_read(0x30) & 0x80, 0x00);

    adxl345_model_advance_us(&model, 100);
    ASSERT_EQ(count(0x36), 256);
    ASSERT_EQ_HEX(reg_read(0x30) & 0x80, 0x80);
}

/* p.4: at other rates "approximately tau + 1.1 in milliseconds, where
 * tau = 1/(data rate)". Rate code 1000b is 25 Hz, table 7. */
TEST(test_hw_turn_on_time_follows_the_rate)
{
    start();
    reg_write(0x2C, 0x08);
    reg_write(0x2D, 0x08);
    adxl345_model_advance_us(&model, 41000);
    ASSERT_EQ(count(0x36), 0);
    adxl345_model_advance_us(&model, 100);
    ASSERT_EQ(count(0x36), 256);
}

/* One sample per period of the output data rate, table 7 */
TEST(test_hw_samples_come_at_the_data_rate)
{
    start();
    measure(0x08);
    adxl345_model_set_accel(&model, 0, 0, G(-1));
    ASSERT_EQ(count(0x36), 256);
    adxl345_model_advance_us(&model, 9900);
    ASSERT_EQ(count(0x36), 256);
    adxl345_model_advance_us(&model, 100);
    ASSERT_EQ(count(0x36), -256);
}

/* p.26: 0 in the measure bit "places the part into standby mode" */
TEST(test_hw_standby_stops_the_samples)
{
    start();
    measure(0x08);
    reg_write(0x2D, 0x00);
    adxl345_model_set_accel(&model, 0, 0, G(2));
    adxl345_model_advance_us(&model, 100000);
    ASSERT_EQ(count(0x36), 256);
}

/* p.27: DATA_READY and the others "are cleared by reading data from the DATAX,
 * DATAY, and DATAZ registers" */
TEST(test_hw_reading_the_data_clears_data_ready)
{
    uint8_t buf[6];
    start();
    measure(0x08);
    ASSERT_EQ_HEX(reg_read(0x30) & 0x80, 0x80);
    ASSERT_TRUE(adxl345_model_read_regs(&model.hw, 0x53, 0x32, buf, 6));
    ASSERT_EQ_HEX(reg_read(0x30) & 0x80, 0x00);
}

/* p.3, Sensitivity: full resolution is 256 LSB/g at every range */
TEST(test_hw_full_resolution_scale)
{
    for (uint8_t range = 0; range < 4; range++) {
        start();
        measure((uint8_t)(0x08 | range));
        ASSERT_EQ(count(0x36), 256);
    }
}

/* p.3, Sensitivity: 10-bit mode gives 256, 128, 64 and 32 LSB/g. p.27: "When
 * the FULL_RES bit is set to 0, the device is in 10-bit mode, and the range
 * bits determine the maximum g range and scale factor." */
TEST(test_hw_10_bit_scale)
{
    static const int lsb_per_g[4] = {256, 128, 64, 32};
    for (uint8_t range = 0; range < 4; range++) {
        start();
        measure(range);
        ASSERT_EQ(count(0x36), lsb_per_g[range]);
    }
}

/* p.27: "All data, except that for the +/-16 g range, must be clipped to avoid
 * rollover." p.3 gives 10, 11 and 12 bits at 2, 4 and 8 g in full resolution. */
TEST(test_hw_clipping_at_each_range)
{
    static const int top[3] = {511, 1023, 2047};
    for (uint8_t range = 0; range < 3; range++) {
        start();
        adxl345_model_set_accel(&model, G(15), G(-15), 0);
        measure((uint8_t)(0x08 | range));
        ASSERT_EQ(count(0x32), top[range]);
        ASSERT_EQ(count(0x34), -top[range] - 1);
    }
    /* 10 bits at every range without FULL_RES */
    for (uint8_t range = 0; range < 3; range++) {
        start();
        adxl345_model_set_accel(&model, G(15), G(-15), 0);
        measure(range);
        ASSERT_EQ(count(0x32), 511);
        ASSERT_EQ(count(0x34), -512);
    }
}

/* p.27 again: the +/-16 g range is the exception, and p.3 gives it 13 bits */
TEST(test_hw_16g_is_not_clipped)
{
    start();
    adxl345_model_set_accel(&model, G(15), 0, 0);
    measure(0x0B);
    ASSERT_EQ(count(0x32), 15 * 256);

    start();
    adxl345_model_set_accel(&model, G(17), 0, 0);
    measure(0x0B);
    ASSERT_EQ(count(0x32), (17 - 32) * 256);  /* rolled over 13 bits */
}

/* p.27: Justify "selects left-justified (MSB) mode" */
TEST(test_hw_left_justified)
{
    start();
    measure(0x04);  /* 10-bit, +/-2 g: 256 is 01 0000 0000b */
    ASSERT_EQ_HEX((uint16_t)count(0x36), 0x4000);

    start();
    measure(0x0F);  /* full resolution, +/-16 g: 13 bits */
    ASSERT_EQ_HEX((uint16_t)count(0x36), 0x0800);
}

/* p.27: DATA_FORMAT "controls the presentation of data", so the next sample
 * follows a change */
TEST(test_hw_format_applies_to_the_next_sample)
{
    start();
    measure(0x08);
    ASSERT_EQ(count(0x36), 256);
    reg_write(0x31, 0x03);
    ASSERT_EQ(count(0x36), 256);
    adxl345_model_advance_us(&model, 10000);
    ASSERT_EQ(count(0x36), 32);
}

/* ---------- the driver on the model ---------- */

static adxl345_err_t init_at(adxl345_range_t range, adxl345_data_rate_t rate)
{
    adxl345_cfg_t cfg = {
        .addr = ADXL345_ADDR_PRIMARY, .range = range, .data_rate = rate,
    };
    return adxl345_init(&dev, &hal, &cfg);
}

TEST(test_hw_init_programs_the_part)
{
    start();
    ASSERT_EQ(init_at(ADXL345_RANGE_8G, ADXL345_RATE_200HZ), ADXL345_OK);
    ASSERT_EQ_HEX(reg_read(0x2C), 0x0B);  /* table 7: 200 Hz is 1011b */
    ASSERT_EQ_HEX(reg_read(0x31), 0x0A);  /* FULL_RES, +/-8 g, table 35 */
    ASSERT_EQ_HEX(reg_read(0x2D), 0x08);  /* Measure */
}

/* p.12: "configure the device in standby mode and then to enable measurement
 * mode". From reset, nothing is written after Measure. */
TEST(test_hw_init_enables_measurement_last)
{
    start();
    ASSERT_EQ(init_at(ADXL345_RANGE_2G, ADXL345_RATE_100HZ), ADXL345_OK);
    ASSERT_EQ(model.config_writes_while_measuring, 0);
}

/* p.4: a sample exists only after the turn-on time, so the first read after
 * init must not get the zeros of Table 19 */
TEST(test_hw_first_read_after_init_is_a_sample)
{
    start();
    ASSERT_EQ(init_at(ADXL345_RANGE_2G, ADXL345_RATE_100HZ), ADXL345_OK);
    adxl345_data_t data;
    ASSERT_EQ(adxl345_read(&dev, &data), ADXL345_OK);
    ASSERT_NEAR(data.x_g, 0.0f, 0.01f);
    ASSERT_NEAR(data.y_g, 0.0f, 0.01f);
    ASSERT_NEAR(data.z_g, 1.0f, 0.01f);
}

/* p.4 at every rate the driver accepts; table 7 lists them by rate code */
TEST(test_hw_init_waits_tau_plus_1_1_ms)
{
    static const double hz[14] = {0.10, 0.20, 0.39, 0.78, 1.56, 3.13, 6.25,
                                  12.5, 25.0, 50.0, 100.0, 200.0, 400.0, 800.0};
    for (int code = 0; code < 14; code++) {
        start();
        ASSERT_EQ(init_at(ADXL345_RANGE_2G, (adxl345_data_rate_t)code),
                  ADXL345_OK);
        double need = 1000.0 / hz[code] + 1.1;
        ASSERT_TRUE((double)model.hw.delay_ms_total >= need);
        ASSERT_TRUE((double)model.hw.delay_ms_total < need + 1.0);
        ASSERT_EQ_HEX(reg_read(0x30) & 0x80, 0x80);
    }
}

/* The wait is the caller's when the HAL has no delay callback */
TEST(test_hw_init_without_a_delay_callback)
{
    static const adxl345_i2c_t no_delay = {
        .write_reg = hal_write, .read_regs = hal_read,
        .delay_ms = NULL, .ctx = &model.hw,
    };
    adxl345_cfg_t cfg = {
        .addr = ADXL345_ADDR_PRIMARY, .range = ADXL345_RANGE_2G,
        .data_rate = ADXL345_RATE_100HZ,
    };
    start();
    ASSERT_EQ(adxl345_init(&dev, &no_delay, &cfg), ADXL345_OK);
    adxl345_model_advance_us(&model, 11100);
    adxl345_data_t data;
    ASSERT_EQ(adxl345_read(&dev, &data), ADXL345_OK);
    ASSERT_NEAR(data.z_g, 1.0f, 0.01f);
}

/* p.3: 1 g reads as 1 g at every range, which fails if FULL_RES is left clear */
TEST(test_hw_one_g_at_every_range)
{
    for (int range = 0; range < 4; range++) {
        start();
        ASSERT_EQ(init_at((adxl345_range_t)range, ADXL345_RATE_100HZ),
                  ADXL345_OK);
        adxl345_data_t data;
        ASSERT_EQ(adxl345_read(&dev, &data), ADXL345_OK);
        ASSERT_NEAR(data.z_g, 1.0f, 0.01f);
    }
}

/* Each axis lands in its own field, with its sign */
TEST(test_hw_axes_and_signs)
{
    start();
    adxl345_model_set_accel(&model, G(1), G(-1), 512);
    ASSERT_EQ(init_at(ADXL345_RANGE_8G, ADXL345_RATE_100HZ), ADXL345_OK);
    adxl345_data_t data;
    ASSERT_EQ(adxl345_read(&dev, &data), ADXL345_OK);
    ASSERT_NEAR(data.x_g, 1.0f, 0.01f);
    ASSERT_NEAR(data.y_g, -1.0f, 0.01f);
    ASSERT_NEAR(data.z_g, 2.0f, 0.02f);
}

/* p.27: at +/-2 g, 5 g clips; at +/-8 g the next sample shows it */
TEST(test_hw_set_range_widens_the_next_sample)
{
    start();
    adxl345_model_set_accel(&model, 0, 0, G(5));
    ASSERT_EQ(init_at(ADXL345_RANGE_2G, ADXL345_RATE_100HZ), ADXL345_OK);
    adxl345_data_t data;
    ASSERT_EQ(adxl345_read(&dev, &data), ADXL345_OK);
    ASSERT_NEAR(data.z_g, 2.0f, 0.02f);

    ASSERT_EQ(adxl345_set_range(&dev, ADXL345_RANGE_8G), ADXL345_OK);
    adxl345_model_advance_us(&model, 10000);
    ASSERT_EQ(adxl345_read(&dev, &data), ADXL345_OK);
    ASSERT_NEAR(data.z_g, 5.0f, 0.05f);
}

/* p.27: "It is recommended that a multiple-byte read of all registers be
 * performed to prevent a change in data between reads of sequential registers."
 * Table 19 forbids 0x01 to 0x1C. */
TEST(test_hw_driver_bus_traffic)
{
    start();
    ASSERT_EQ(init_at(ADXL345_RANGE_2G, ADXL345_RATE_100HZ), ADXL345_OK);
    adxl345_data_t data;
    ASSERT_EQ(adxl345_read(&dev, &data), ADXL345_OK);
    ASSERT_EQ(six_byte_reads, 1);
    ASSERT_EQ(reserved_access, 0);
    ASSERT_EQ_HEX(last_addr, 0x53);
}

TEST_MAIN("adxl345_hw_model")
    RUN(test_hw_reset_values);
    RUN(test_hw_read_only_registers_ignore_writes);
    RUN(test_hw_fixed_zero_bits_stay_zero);
    RUN(test_hw_standby_produces_no_data);
    RUN(test_hw_turn_on_time_at_100_hz);
    RUN(test_hw_turn_on_time_follows_the_rate);
    RUN(test_hw_samples_come_at_the_data_rate);
    RUN(test_hw_standby_stops_the_samples);
    RUN(test_hw_reading_the_data_clears_data_ready);
    RUN(test_hw_full_resolution_scale);
    RUN(test_hw_10_bit_scale);
    RUN(test_hw_clipping_at_each_range);
    RUN(test_hw_16g_is_not_clipped);
    RUN(test_hw_left_justified);
    RUN(test_hw_format_applies_to_the_next_sample);
    RUN(test_hw_init_programs_the_part);
    RUN(test_hw_init_enables_measurement_last);
    RUN(test_hw_first_read_after_init_is_a_sample);
    RUN(test_hw_init_waits_tau_plus_1_1_ms);
    RUN(test_hw_init_without_a_delay_callback);
    RUN(test_hw_one_g_at_every_range);
    RUN(test_hw_axes_and_signs);
    RUN(test_hw_set_range_widens_the_next_sample);
    RUN(test_hw_driver_bus_traffic);
TEST_MAIN_END()
