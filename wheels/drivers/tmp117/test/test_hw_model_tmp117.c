// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_tmp117.h"
#include "tmp117.h"
#include <math.h>

static const tmp117_i2c_t hw_i2c = {
    .write_regs = hw_model_write_regs,
    .read_regs  = hw_model_read_regs,
    .delay_ms   = hw_model_delay_ms,
    .ctx        = NULL,
};

static const tmp117_cfg_t cfg = {
    .addr = TMP117_ADDR_GND,
    .avg  = TMP117_AVG_NONE,
};

static tmp117_t dev;

/* Configuration register fields, SNOSD82D Figure 7-14 p27 */
#define CFG_DATA_READY  0x2000u
#define CFG_EEPROM_BUSY 0x1000u
#define CFG_MOD_SD      0x0400u
#define CFG_SOFT_RESET  0x0002u

static uint16_t bus_read(uint8_t reg)
{
    uint8_t buf[2] = {0xA5u, 0xA5u};
    (void)hw_model_read_regs(NULL, TMP117_ADDR_GND, reg, buf, 2);
    return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

static void bus_write(uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = {(uint8_t)(val >> 8), (uint8_t)(val & 0xFFu)};
    ASSERT_TRUE(hw_model_write_regs(NULL, TMP117_ADDR_GND, reg, buf, 2));
}

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    ASSERT_EQ(tmp117_init(&dev, &hw_i2c, &cfg), TMP117_OK);
}

TEST(test_hw_device_id_check)
{
    hw_model_reset();
    hw_regs16[0x0F] = 0xBEEF;
    ASSERT_EQ(tmp117_init(&dev, &hw_i2c, &cfg), TMP117_ERR_ID);
}

TEST(test_hw_config_written)
{
    hw_model_reset();
    tmp117_cfg_t avg_cfg = { .addr = TMP117_ADDR_GND, .avg = TMP117_AVG_32 };
    ASSERT_EQ(tmp117_init(&dev, &hw_i2c, &avg_cfg), TMP117_OK);
    ASSERT_EQ((hw_regs16[0x01] >> 5) & 0x03, 2);
}

TEST(test_hw_read_roundtrip_25c)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_temperature(25.0f);
    float temp = 0.0f;
    tmp117_read(&dev, &temp);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

TEST(test_hw_read_roundtrip_neg40)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_temperature(-40.0f);
    float temp = 0.0f;
    tmp117_read(&dev, &temp);
    ASSERT_NEAR(temp, -40.0f, 0.01f);
}

TEST(test_hw_reset_bit)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_temperature(25.0f);
    ASSERT_EQ(tmp117_reset(&dev), TMP117_OK);

    /* p28: Soft_Reset "will always read back 0", and the registers return to
     * their p25 reset values, AVG included: the driver does not reapply it */
    ASSERT_EQ(hw_reset_count, 1);
    ASSERT_EQ_HEX(bus_read(0x01), 0x0220u);
    ASSERT_EQ_HEX(bus_read(0x00), 0x8000u);
}

/* p26: the register "reads -256 C" until the first conversion is complete, so
 * the driver must say not ready again after a reset */
TEST(test_hw_read_after_reset_is_not_ready)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c, &cfg);
    hw_model_inject_temperature(25.0f);
    ASSERT_EQ(tmp117_reset(&dev), TMP117_OK);

    float temp = 99.0f;
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_ERR_NOT_READY);
    hw_model_inject_temperature(30.0f);
    ASSERT_EQ(tmp117_read(&dev, &temp), TMP117_OK);
    ASSERT_NEAR(temp, 30.0f, 0.01f);
}

/* p28: "software reset with a duration of 2 ms" */
TEST(test_hw_driver_waits_out_the_reset)
{
    hw_model_reset();
    tmp117_init(&dev, &hw_i2c, &cfg);
    ASSERT_EQ(tmp117_reset(&dev), TMP117_OK);
    ASSERT_EQ_HEX(bus_read(0x01) & CFG_EEPROM_BUSY, 0u);

    /* so a configuration right after it takes */
    tmp117_cfg_t avg_cfg = { .addr = TMP117_ADDR_GND, .avg = TMP117_AVG_32 };
    ASSERT_EQ(tmp117_init(&dev, &hw_i2c, &avg_cfg), TMP117_OK);
    ASSERT_EQ((bus_read(0x01) >> 5) & 0x03u, 2u);
}

/* --- what the model itself must do (SNOSD82D) --------------------------- */

/* p25 Table 7-3 */
TEST(test_hw_table_7_3_reset_values)
{
    hw_model_reset();
    ASSERT_EQ_HEX(bus_read(0x00), 0x8000u);
    ASSERT_EQ_HEX(bus_read(0x01), 0x0220u);
    ASSERT_EQ_HEX(bus_read(0x02), 0x6000u);
    ASSERT_EQ_HEX(bus_read(0x03), 0x8000u);
    ASSERT_EQ_HEX(bus_read(0x04), 0x0000u);
    ASSERT_EQ_HEX(bus_read(0x07), 0x0000u);
    ASSERT_EQ_HEX(bus_read(0x0F), 0x0117u);
}

/* p25 Table 7-3: Temp_Result and Device_ID have type R */
TEST(test_hw_read_only_registers_ignore_writes)
{
    hw_model_reset();
    bus_write(0x00, 0x1234u);
    bus_write(0x0F, 0x1234u);
    ASSERT_EQ_HEX(bus_read(0x00), 0x8000u);
    ASSERT_EQ_HEX(bus_read(0x0F), 0x0117u);
}

/* p27 Figure 7-14: bits 15:12 and bit 0 are R. The rest of the word still
 * lands. */
TEST(test_hw_config_read_only_bits_ignore_writes)
{
    hw_model_reset();
    bus_write(0x01, 0xF385u);
    ASSERT_EQ_HEX(bus_read(0x01), 0x0384u);
}

/* p27: "10: Continuous conversion (CC), Same as 00 (reads back = 00)" */
TEST(test_hw_mod_10_reads_back_00)
{
    hw_model_reset();
    bus_write(0x01, 0x0820u);
    ASSERT_EQ_HEX(bus_read(0x01), 0x0020u);
}

/* p28: a Soft_Reset write reloads the reset values, limits included */
TEST(test_hw_soft_reset_reloads_every_register)
{
    hw_model_reset();
    bus_write(0x01, 0x0000u);
    bus_write(0x02, 0x1111u);
    bus_write(0x03, 0x2222u);
    hw_model_inject_temperature(25.0f);
    bus_write(0x01, CFG_SOFT_RESET);
    hw_model_delay_ms(NULL, 2);
    ASSERT_EQ_HEX(bus_read(0x00), 0x8000u);
    ASSERT_EQ_HEX(bus_read(0x01), 0x0220u);
    ASSERT_EQ_HEX(bus_read(0x02), 0x6000u);
    ASSERT_EQ_HEX(bus_read(0x03), 0x8000u);
}

/* p18: "Any I2C writes performed during this initial POR period to the limit
 * registers or the configuration register are ignored", and p28 gives the
 * reset 2 ms. "While the POR sequence is being executed, the EEPROM_Busy status
 * flag ... is set." */
TEST(test_hw_writes_are_lost_during_the_reset)
{
    hw_model_reset();
    bus_write(0x01, CFG_SOFT_RESET);
    ASSERT_EQ_HEX(bus_read(0x01) & CFG_EEPROM_BUSY, CFG_EEPROM_BUSY);
    bus_write(0x01, 0x0040u);
    bus_write(0x02, 0x1111u);
    bus_write(0x03, 0x2222u);
    hw_model_delay_ms(NULL, 1);
    ASSERT_EQ_HEX(bus_read(0x01) & CFG_EEPROM_BUSY, CFG_EEPROM_BUSY);
    hw_model_delay_ms(NULL, 1);
    ASSERT_EQ_HEX(bus_read(0x01), 0x0220u);
    ASSERT_EQ_HEX(bus_read(0x02), 0x6000u);
    ASSERT_EQ_HEX(bus_read(0x03), 0x8000u);

    bus_write(0x01, 0x0040u);
    ASSERT_EQ_HEX(bus_read(0x01), 0x0040u);
}

/* p27: Data_Ready "is set at the end of the conversion when the temperature
 * register is updated", and "Every time the temperature register or
 * configuration register is read, this bit is cleared." */
TEST(test_hw_temperature_read_clears_data_ready)
{
    hw_model_reset();
    hw_model_inject_temperature(25.0f);
    ASSERT_EQ_HEX(hw_regs16[0x01] & CFG_DATA_READY, CFG_DATA_READY);
    (void)bus_read(0x00);
    ASSERT_EQ_HEX(hw_regs16[0x01] & CFG_DATA_READY, 0u);
}

TEST(test_hw_config_read_clears_data_ready)
{
    hw_model_reset();
    hw_model_inject_temperature(25.0f);
    ASSERT_EQ_HEX(bus_read(0x01) & CFG_DATA_READY, CFG_DATA_READY);
    ASSERT_EQ_HEX(bus_read(0x01) & CFG_DATA_READY, 0u);
}

/* p14: at MOD 01 the part "enters a low-power shutdown mode" and "powers down
 * all active circuitry", so nothing converts */
TEST(test_hw_shutdown_makes_no_conversion)
{
    hw_model_reset();
    bus_write(0x01, CFG_MOD_SD);
    hw_model_inject_temperature(25.0f);
    ASSERT_EQ_HEX(bus_read(0x00), 0x8000u);
    ASSERT_EQ_HEX(bus_read(0x01) & CFG_DATA_READY, 0u);
}

TEST_MAIN("tmp117_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_device_id_check);
    RUN(test_hw_config_written);
    RUN(test_hw_read_roundtrip_25c);
    RUN(test_hw_read_roundtrip_neg40);
    RUN(test_hw_reset_bit);
    RUN(test_hw_read_after_reset_is_not_ready);
    RUN(test_hw_driver_waits_out_the_reset);
    RUN(test_hw_table_7_3_reset_values);
    RUN(test_hw_read_only_registers_ignore_writes);
    RUN(test_hw_config_read_only_bits_ignore_writes);
    RUN(test_hw_mod_10_reads_back_00);
    RUN(test_hw_soft_reset_reloads_every_register);
    RUN(test_hw_writes_are_lost_during_the_reset);
    RUN(test_hw_temperature_read_clears_data_ready);
    RUN(test_hw_config_read_clears_data_ready);
    RUN(test_hw_shutdown_makes_no_conversion);
TEST_MAIN_END()
