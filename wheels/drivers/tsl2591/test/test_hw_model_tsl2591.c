// SPDX-License-Identifier: MIT
/*
 * test_hw_model_tsl2591.c -- the driver against the data sheet model, and the
 * model against ams TSL2591 datasheet, Apr. 2013, ams163.5
 */
#include "test_harness.h"
#include "hw_model_tsl2591.h"
#include "tsl2591.h"

static tsl2591_model_t model;
static tsl2591_t dev;

/* The HAL checks what the model cannot see for itself */
static int bad_address;      /* p.4: the only slave address is 0x29 */
static int bad_command;      /* p.11: CMD 1, TRANSACTION 01 on every access */
static int four_byte_reads;  /* p.17: all four ADC bytes in one read */

static void check_access(uint8_t addr, uint8_t cmd)
{
    if (addr != 0x29u) bad_address++;
    if ((cmd & 0xE0u) != 0xA0u) bad_command++;
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
    if (reg == TSL_C0DATAL && len == 4) four_byte_reads++;
    return hw_model_i2c_read_regs(ctx, addr, reg, buf, len);
}

static const tsl2591_i2c_t hal = {
    .write_reg = hal_write,
    .read_regs = hal_read,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static void start(void)
{
    tsl2591_model_init(&model);
    bad_address = 0;
    bad_command = 0;
    four_byte_reads = 0;
}

static uint8_t reg_read(uint8_t reg)
{
    uint8_t val = 0;
    (void)hw_model_i2c_read_regs(&model.hw, 0x29, reg, &val, 1);
    return val;
}

static void reg_write(uint8_t reg, uint8_t val)
{
    (void)hw_model_i2c_write_reg(&model.hw, 0x29, reg, val);
}

/* ---------- the model ---------- */

/* p.16: ID "is a read-only register whose value never changes" */
TEST(test_hw_id_never_changes)
{
    start();
    ASSERT_EQ_HEX(reg_read(TSL_ID), 0x50);
    reg_write(TSL_ID, 0x00);
    ASSERT_EQ_HEX(reg_read(TSL_ID), 0x50);
}

/* p.16: PID is read only too, and "Package Identification = 00" */
TEST(test_hw_pid_is_read_only)
{
    start();
    reg_write(TSL_PID, 0x30);
    ASSERT_EQ_HEX(reg_read(TSL_PID) & 0x30, 0x00);
}

/* p.16: "This register is read only." */
TEST(test_hw_status_is_read_only)
{
    start();
    reg_write(TSL_STATUS, 0x31);
    ASSERT_EQ_HEX(reg_read(TSL_STATUS), 0x00);
}

/* p.12: NPIEN 7, SAI 6, AIEN 4, AEN 1, PON 0; bits 5, 3 and 2 are reserved */
TEST(test_hw_enable_has_sai_and_keeps_reserved_bits_zero)
{
    start();
    reg_write(TSL_ENABLE, 0x40);
    ASSERT_EQ_HEX(reg_read(TSL_ENABLE), 0x40);
    reg_write(TSL_ENABLE, 0xFF);
    ASSERT_EQ_HEX(reg_read(TSL_ENABLE), 0xD3);
}

/* p.13: "Upon power up, the CONTROL register resets to 0x00." */
TEST(test_hw_control_resets_to_zero)
{
    start();
    ASSERT_EQ_HEX(reg_read(TSL_CONFIG), 0x00);
}

/* p.13: bits 6 and 3 are reserved */
TEST(test_hw_control_keeps_reserved_bits_zero)
{
    start();
    reg_write(TSL_CONFIG, 0x7F);
    ASSERT_EQ_HEX(reg_read(TSL_CONFIG), 0x37);
}

/* p.13: "When asserted, the device will reset equivalent to a power-on reset.
 * SRESET is self-clearing." */
TEST(test_hw_sreset_is_a_power_on_reset)
{
    start();
    reg_write(TSL_ENABLE, 0x03);
    reg_write(TSL_CONFIG, 0x35);
    reg_write(TSL_CMD(0x04u), 0x12);  /* AILTL, default 00h, p.14 */
    reg_write(TSL_CMD(0x0Cu), 0x05);  /* PERSIST, resets to 00h, p.15 */
    tsl2591_model_end_integration(&model);

    reg_write(TSL_CONFIG, 0x80);

    ASSERT_EQ_HEX(reg_read(TSL_CONFIG), 0x00);
    ASSERT_EQ_HEX(reg_read(TSL_ENABLE), 0x00);
    ASSERT_EQ_HEX(reg_read(TSL_STATUS), 0x00);
    ASSERT_EQ_HEX(reg_read(TSL_CMD(0x04u)), 0x00);
    ASSERT_EQ_HEX(reg_read(TSL_CMD(0x0Cu)), 0x00);
    ASSERT_EQ_HEX(reg_read(TSL_ID), 0x50);
}

/* p.16: AVALID "Indicates that the ADC channels have completed an integration
 * cycle since the AEN bit was asserted." */
TEST(test_hw_avalid_waits_for_the_integration)
{
    start();
    reg_write(TSL_ENABLE, 0x03);
    ASSERT_EQ_HEX(reg_read(TSL_STATUS) & 0x01, 0x00);
    ASSERT_EQ(reg_read(TSL_C0DATAL), 0);

    tsl2591_model_end_integration(&model);
    ASSERT_EQ_HEX(reg_read(TSL_STATUS) & 0x01, 0x01);
    ASSERT_EQ(reg_read(TSL_C0DATAL), 500 & 0xFF);
}

TEST(test_hw_asserting_aen_again_clears_avalid)
{
    start();
    reg_write(TSL_ENABLE, 0x03);
    tsl2591_model_end_integration(&model);
    reg_write(TSL_ENABLE, 0x01);
    reg_write(TSL_ENABLE, 0x03);
    ASSERT_EQ_HEX(reg_read(TSL_STATUS) & 0x01, 0x00);
}

/* Changing an interrupt enable does not assert AEN again */
TEST(test_hw_other_enable_bits_leave_avalid)
{
    start();
    reg_write(TSL_ENABLE, 0x03);
    tsl2591_model_end_integration(&model);
    reg_write(TSL_ENABLE, 0x13);
    ASSERT_EQ_HEX(reg_read(TSL_STATUS) & 0x01, 0x01);
}

/* p.2: "Upon completion of the conversion cycle, the conversion result is
 * transferred to the Channel 0 and Channel 1 data registers" */
TEST(test_hw_light_reaches_the_registers_at_the_end_of_a_cycle)
{
    start();
    reg_write(TSL_ENABLE, 0x03);
    tsl2591_model_end_integration(&model);
    tsl2591_model_set_channels(&model, 0x1234, 0x0056);
    ASSERT_EQ_HEX(reg_read(TSL_C0DATAH), 500 >> 8);

    tsl2591_model_end_integration(&model);
    ASSERT_EQ_HEX(reg_read(TSL_C0DATAL), 0x34);
    ASSERT_EQ_HEX(reg_read(TSL_C0DATAH), 0x12);
    ASSERT_EQ_HEX(reg_read(TSL_C1DATAL), 0x56);
    ASSERT_EQ_HEX(reg_read(TSL_C1DATAH), 0x00);
}

/* p.12: PON "activates the internal oscillator to permit the timers and ADC
 * channels to operate", AEN "activates the ALS" */
TEST(test_hw_no_cycle_without_pon_and_aen)
{
    static const uint8_t off[] = {0x00, 0x01, 0x02};
    for (size_t i = 0; i < sizeof(off); i++) {
        start();
        reg_write(TSL_ENABLE, off[i]);
        tsl2591_model_end_integration(&model);
        ASSERT_EQ_HEX(reg_read(TSL_STATUS) & 0x01, 0x00);
        ASSERT_EQ(reg_read(TSL_C0DATAL), 0);
    }
}

/* p.13: MAX COUNT is 37888 at 100 ms and 65535 at the other five times */
TEST(test_hw_counts_stop_at_max_count)
{
    start();
    tsl2591_model_set_channels(&model, 0xFFFF, 0xFFFF);
    reg_write(TSL_ENABLE, 0x03);
    tsl2591_model_end_integration(&model);
    ASSERT_EQ(reg_read(TSL_C0DATAL) | (reg_read(TSL_C0DATAH) << 8), 37888);
    ASSERT_EQ(reg_read(TSL_C1DATAL) | (reg_read(TSL_C1DATAH) << 8), 37888);

    reg_write(TSL_CONFIG, 0x05);
    tsl2591_model_end_integration(&model);
    ASSERT_EQ(reg_read(TSL_C0DATAL) | (reg_read(TSL_C0DATAH) << 8), 65535);
}

/* ---------- the driver on the model ---------- */

TEST(test_hw_init_powers_up_and_configures)
{
    start();
    ASSERT_EQ(tsl2591_init(&dev, &hal, TSL2591_GAIN_HIGH, TSL2591_ITIME_300MS),
              TSL2591_OK);
    ASSERT_EQ_HEX(reg_read(TSL_ENABLE), 0x03);
    ASSERT_EQ_HEX(reg_read(TSL_CONFIG), 0x22);  /* AGAIN 10, ATIME 010, p.13 */
}

/* p.13: the gain and time codes reach AGAIN and ATIME */
TEST(test_hw_settings_reach_the_control_register)
{
    start();
    ASSERT_EQ(tsl2591_init(&dev, &hal, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_OK);
    ASSERT_EQ(tsl2591_set_gain(&dev, TSL2591_GAIN_MAX), TSL2591_OK);
    ASSERT_EQ_HEX(reg_read(TSL_CONFIG), 0x30);
    ASSERT_EQ(tsl2591_set_integration_time(&dev, TSL2591_ITIME_600MS), TSL2591_OK);
    ASSERT_EQ_HEX(reg_read(TSL_CONFIG), 0x35);
    ASSERT_EQ(tsl2591_set_gain(&dev, TSL2591_GAIN_MEDIUM), TSL2591_OK);
    ASSERT_EQ_HEX(reg_read(TSL_CONFIG), 0x15);
}

/* p.16: no result exists until an integration cycle has finished, so a read
 * must not pass 0 counts off as a measurement */
TEST(test_hw_read_before_the_first_integration)
{
    start();
    ASSERT_EQ(tsl2591_init(&dev, &hal, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_OK);

    tsl2591_data_t data;
    uint16_t ch0, ch1;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_ERR_NOT_READY);
    ASSERT_EQ(tsl2591_read_raw(&dev, &ch0, &ch1), TSL2591_ERR_NOT_READY);

    tsl2591_model_end_integration(&model);
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_EQ(data.ch0_raw, 500);
    ASSERT_EQ(data.ch1_raw, 100);
    ASSERT_EQ(tsl2591_read_raw(&dev, &ch0, &ch1), TSL2591_OK);
    ASSERT_EQ(ch0, 500);
    ASSERT_EQ(ch1, 100);
}

/* p.16: after AEN is asserted again the old result no longer counts */
TEST(test_hw_read_after_enable_waits_again)
{
    start();
    ASSERT_EQ(tsl2591_init(&dev, &hal, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_OK);
    tsl2591_model_end_integration(&model);
    ASSERT_EQ(tsl2591_disable(&dev), TSL2591_OK);
    ASSERT_EQ(tsl2591_enable(&dev), TSL2591_OK);

    tsl2591_data_t data;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_ERR_NOT_READY);
    tsl2591_model_end_integration(&model);
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
}

/* p.16: AVALID is bit 0 of STATUS */
TEST(test_hw_get_status_shows_avalid)
{
    start();
    ASSERT_EQ(tsl2591_init(&dev, &hal, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_OK);
    uint8_t status = 0xFF;
    ASSERT_EQ(tsl2591_get_status(&dev, &status), TSL2591_OK);
    ASSERT_EQ_HEX(status & 0x01, 0x00);
    tsl2591_model_end_integration(&model);
    ASSERT_EQ(tsl2591_get_status(&dev, &status), TSL2591_OK);
    ASSERT_EQ_HEX(status & 0x01, 0x01);
}

/* p.17: "The simplest way to accomplish this is to perform a four-byte I2C read
 * operation". p.4 and p.11 cover the address and the command byte. */
TEST(test_hw_driver_bus_traffic)
{
    start();
    ASSERT_EQ(tsl2591_init(&dev, &hal, TSL2591_GAIN_LOW, TSL2591_ITIME_100MS),
              TSL2591_OK);
    tsl2591_model_end_integration(&model);

    tsl2591_data_t data;
    uint16_t ch0, ch1;
    ASSERT_EQ(tsl2591_read(&dev, &data), TSL2591_OK);
    ASSERT_EQ(tsl2591_read_raw(&dev, &ch0, &ch1), TSL2591_OK);
    ASSERT_EQ(four_byte_reads, 2);
    ASSERT_EQ(bad_address, 0);
    ASSERT_EQ(bad_command, 0);
}

TEST_MAIN("tsl2591_hw_model")
    RUN(test_hw_id_never_changes);
    RUN(test_hw_pid_is_read_only);
    RUN(test_hw_status_is_read_only);
    RUN(test_hw_enable_has_sai_and_keeps_reserved_bits_zero);
    RUN(test_hw_control_resets_to_zero);
    RUN(test_hw_control_keeps_reserved_bits_zero);
    RUN(test_hw_sreset_is_a_power_on_reset);
    RUN(test_hw_avalid_waits_for_the_integration);
    RUN(test_hw_asserting_aen_again_clears_avalid);
    RUN(test_hw_other_enable_bits_leave_avalid);
    RUN(test_hw_light_reaches_the_registers_at_the_end_of_a_cycle);
    RUN(test_hw_no_cycle_without_pon_and_aen);
    RUN(test_hw_counts_stop_at_max_count);
    RUN(test_hw_init_powers_up_and_configures);
    RUN(test_hw_settings_reach_the_control_register);
    RUN(test_hw_read_before_the_first_integration);
    RUN(test_hw_read_after_enable_waits_again);
    RUN(test_hw_get_status_shows_avalid);
    RUN(test_hw_driver_bus_traffic);
TEST_MAIN_END()
