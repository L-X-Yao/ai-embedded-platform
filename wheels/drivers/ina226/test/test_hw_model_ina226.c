// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ina226.h"
#include "ina226.h"

static const ina226_i2c_t hw_i2c = {
    .read_regs  = hw_model_read_regs,
    .write_regs = hw_model_write_regs,
    .ctx        = NULL,
};

static ina226_t dev;

static const ina226_cfg_t hw_cfg = {
    .addr                  = 0x40,
    .shunt_resistance_mohm = 100,
    .max_current_ma        = 1000,
    .averaging             = INA226_AVG_1,
    .bus_conv_time         = INA226_CT_1100US,
    .shunt_conv_time       = INA226_CT_1100US,
};

TEST(test_hw_init_verifies_ids)
{
    hw_model_reset();
    ina226_err_t e = ina226_init(&dev, &hw_i2c, &hw_cfg);
    ASSERT_EQ(e, INA226_OK);
    ASSERT_TRUE(hw_read_count >= 2);  /* Read mfr ID and die ID */
}

TEST(test_hw_shunt_voltage_round_trip)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c, &hw_cfg);

    /* Inject 5000 uV shunt voltage */
    hw_model_inject_shunt_uv(5000);
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_EQ(data.shunt_voltage_uv, 5000);
}

TEST(test_hw_bus_voltage_round_trip)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c, &hw_cfg);

    /* Inject 12000 mV bus voltage */
    hw_model_inject_bus_mv(12000);
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_EQ(data.bus_voltage_mv, 12000u);
}

TEST(test_hw_config_register_written)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c, &hw_cfg);

    uint16_t config = hw_model_get_reg16(0x00);
    /* Mode should be continuous shunt+bus (0x07) */
    ASSERT_EQ(config & 0x07, 0x07);
}

TEST(test_hw_calibration_nonzero)
{
    hw_model_reset();
    ina226_init(&dev, &hw_i2c, &hw_cfg);

    uint16_t cal = hw_model_get_reg16(0x05);
    ASSERT_TRUE(cal > 0);
}

/* --- the model against the manual --- */

static void write16(uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFFu) };
    hw_i2c.write_regs(NULL, 0x40, reg, buf, 2);
}

static uint16_t read16(uint8_t reg)
{
    uint8_t buf[2] = { 0, 0 };
    hw_i2c.read_regs(NULL, 0x40, reg, buf, 2);
    return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

/* Table 7-1, p22 */
TEST(test_hw_por_values)
{
    hw_model_reset();
    ASSERT_EQ_HEX(read16(0x00), 0x4127);
    for (uint8_t reg = 0x01; reg <= 0x07; reg++) {
        ASSERT_EQ_HEX(read16(reg), 0x0000);
    }
    ASSERT_EQ_HEX(read16(0xFE), 0x5449);
    ASSERT_EQ_HEX(read16(0xFF), 0x2260);
}

/* "Resets all registers to default values; this bit self-clears", p23 */
TEST(test_hw_rst_restores_the_por_values)
{
    hw_model_reset();
    ASSERT_EQ(ina226_init(&dev, &hw_i2c, &hw_cfg), INA226_OK);
    hw_model_inject_shunt_uv(5000);
    write16(0x07, 0x1234);
    ASSERT_TRUE(read16(0x05) != 0);
    ASSERT_TRUE(read16(0x04) != 0);

    ASSERT_EQ(ina226_reset(&dev), INA226_OK);
    ASSERT_EQ_HEX(read16(0x00), 0x4127);
    ASSERT_EQ_HEX(read16(0x05), 0x0000);
    ASSERT_EQ_HEX(read16(0x07), 0x0000);
    ASSERT_EQ_HEX(read16(0x04), 0x0000);
    ASSERT_EQ_HEX(read16(0x03), 0x0000);
}

/* Type R in Table 7-1, p22 */
TEST(test_hw_read_only_registers_ignore_writes)
{
    static const uint8_t ro[] = { 0x01, 0x02, 0x03, 0x04, 0xFE, 0xFF };
    hw_model_reset();
    for (unsigned i = 0; i < sizeof(ro); i++) {
        uint16_t before = read16(ro[i]);
        write16(ro[i], 0x1234);
        ASSERT_EQ_HEX(read16(ro[i]), before);
    }
    ASSERT_EQ(hw_ro_writes, 6);
}

/* The example of 6.5.1, p15 and p16: CAL 2560, shunt 8000 and bus 9584
 * give Current 10000 (2710h) and Power 4792 (12B8h) */
TEST(test_hw_equations_3_and_4)
{
    hw_model_reset();
    write16(0x05, 2560);
    hw_model_inject_shunt_raw(8000);
    hw_model_inject_bus_raw(9584);
    ASSERT_EQ_HEX(read16(0x04), 0x2710);
    ASSERT_EQ_HEX(read16(0x03), 0x12B8);

    /* The shunt value is signed, so the current is too (p24, p26) */
    hw_model_inject_shunt_raw(-8000);
    ASSERT_EQ((int16_t)read16(0x04), -10000);
}

/* "Until the Calibration Register is programmed, the Current Register (04h)
 * and Power Register (03h) remain at zero", p15 */
TEST(test_hw_no_calibration_no_current)
{
    hw_model_reset();
    hw_model_inject_shunt_raw(8000);
    hw_model_inject_bus_raw(9584);
    ASSERT_EQ_HEX(read16(0x04), 0x0000);
    ASSERT_EQ_HEX(read16(0x03), 0x0000);

    /* "After programming ... update accordingly", p15 */
    write16(0x05, 2560);
    ASSERT_EQ_HEX(read16(0x04), 0x2710);
}

/* D15 of the Calibration Register has no field, Table 7-11 on p26 */
TEST(test_hw_calibration_has_15_bits)
{
    hw_model_reset();
    write16(0x05, 0x8A00);
    ASSERT_EQ_HEX(read16(0x05), 0x0A00);
    hw_model_inject_shunt_raw(8000);
    ASSERT_EQ_HEX(read16(0x04), 0x2710);
}

/* OVF: "set to '1' if an arithmetic operation resulted in an overflow
 * error", p27 */
TEST(test_hw_overflow_sets_ovf)
{
    hw_model_reset();
    write16(0x05, 0x7FFF);
    hw_model_inject_shunt_raw(8000);
    ASSERT_TRUE(read16(0x06) & 0x0004);
}

/* "The Conversion Ready flag (CVRF) bit clears": on a write to Config, except
 * for power-down, and on a read of Mask/Enable, p11 */
TEST(test_hw_cvrf_follows_the_conversions)
{
    hw_model_reset();
    hw_model_inject_shunt_raw(100);
    ASSERT_TRUE(read16(0x06) & 0x0008);
    ASSERT_EQ(read16(0x06) & 0x0008, 0);

    hw_model_inject_shunt_raw(200);
    write16(0x00, 0x4120);  /* power-down keeps the flag */
    ASSERT_TRUE(read16(0x06) & 0x0008);

    hw_model_inject_shunt_raw(300);
    write16(0x00, 0x4124);  /* power-down again: nothing was converted */
    ASSERT_EQ_HEX(read16(0x01), 200);
}

/* Flags are the chip's: writes reach the other bits only, Table 7-12 on p26 */
TEST(test_hw_mask_enable_flags_are_not_writable)
{
    hw_model_reset();
    write16(0x06, 0xFFFF);
    ASSERT_EQ_HEX(read16(0x06), 0xFC03);
}

/* A triggered mode converts once per write of Config, p11 */
TEST(test_hw_triggered_mode_converts_on_the_config_write)
{
    hw_model_reset();
    write16(0x00, 0x4123);
    hw_model_inject_shunt_raw(400);
    ASSERT_EQ_HEX(read16(0x01), 0x0000);
    write16(0x00, 0x4123);
    ASSERT_EQ_HEX(read16(0x01), 400);
}

/* --- the driver on the corrected model --- */

/* The same example through the API: 2 mOhm, 1 mA/bit, so a maximum of
 * 32.768 A; 20 mV, 11.98 V, 10 A and 119.8 W, p15 and p16 */
TEST(test_hw_driver_reads_the_manual_example)
{
    static const ina226_cfg_t cfg = {
        .addr                  = 0x40,
        .shunt_resistance_mohm = 2,
        .max_current_ma        = 32768,
        .averaging             = INA226_AVG_1,
        .bus_conv_time         = INA226_CT_1100US,
        .shunt_conv_time       = INA226_CT_1100US,
    };
    hw_model_reset();
    ASSERT_EQ(ina226_init(&dev, &hw_i2c, &cfg), INA226_OK);
    ASSERT_EQ(read16(0x05), 2560);

    hw_model_inject_shunt_raw(8000);
    hw_model_inject_bus_raw(9584);
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_EQ(data.shunt_voltage_uv, 20000);
    ASSERT_EQ(data.bus_voltage_mv, 11980u);
    ASSERT_EQ(data.current_ma, 10000);
    ASSERT_EQ(data.power_mw, 119800u);
}

/* CAL past 15 bits: the driver must scale from what the register holds.
 * 3 A through 1 mOhm is 3 mV, raw 1200 at 2.5 uV/bit. */
TEST(test_hw_saturated_calibration_keeps_the_scale)
{
    static const ina226_cfg_t cfg = {
        .addr                  = 0x40,
        .shunt_resistance_mohm = 1,
        .max_current_ma        = 5000,
        .averaging             = INA226_AVG_1,
        .bus_conv_time         = INA226_CT_1100US,
        .shunt_conv_time       = INA226_CT_1100US,
    };
    hw_model_reset();
    ASSERT_EQ(ina226_init(&dev, &hw_i2c, &cfg), INA226_OK);
    ASSERT_EQ_HEX(read16(0x05), 0x7FFF);

    hw_model_inject_shunt_raw(1200);
    ina226_data_t data;
    ASSERT_EQ(ina226_read(&dev, &data), INA226_OK);
    ASSERT_NEAR((float)data.current_ma, 3000.0f, 1.0f);
}

TEST_MAIN("ina226_hw_model")
    RUN(test_hw_por_values);
    RUN(test_hw_rst_restores_the_por_values);
    RUN(test_hw_read_only_registers_ignore_writes);
    RUN(test_hw_equations_3_and_4);
    RUN(test_hw_no_calibration_no_current);
    RUN(test_hw_calibration_has_15_bits);
    RUN(test_hw_overflow_sets_ovf);
    RUN(test_hw_cvrf_follows_the_conversions);
    RUN(test_hw_mask_enable_flags_are_not_writable);
    RUN(test_hw_triggered_mode_converts_on_the_config_write);
    RUN(test_hw_driver_reads_the_manual_example);
    RUN(test_hw_saturated_calibration_keeps_the_scale);
    RUN(test_hw_init_verifies_ids);
    RUN(test_hw_shunt_voltage_round_trip);
    RUN(test_hw_bus_voltage_round_trip);
    RUN(test_hw_config_register_written);
    RUN(test_hw_calibration_nonzero);
TEST_MAIN_END()
