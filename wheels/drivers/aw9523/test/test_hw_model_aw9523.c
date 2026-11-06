// SPDX-License-Identifier: MIT
/*
 * test_hw_model_aw9523.c — tests using the hardware register model
 *
 * Checks the AW9523 driver and the model against the AW9523B datasheet,
 * May 2016, V1.1.1. Expected values come from that manual.
 */

#include "test_harness.h"
#include "hw_model_aw9523.h"
#include "../aw9523.h"

#define ADDR 0x58u

static aw9523_model_t model;
static aw9523_t dev;

static const aw9523_i2c_t model_i2c = {
    .write_reg = aw9523_model_write_reg,
    .read_regs = aw9523_model_read_regs,
    .delay_ms  = aw9523_model_delay,
    .ctx       = &model,
};

static void setup(void)
{
    aw9523_model_init(&model);
}

static void setup_driver(void)
{
    setup();
    ASSERT_EQ(aw9523_init(&dev, &model_i2c, ADDR), AW9523_OK);
}

static uint8_t bus_read(uint8_t reg)
{
    uint8_t val = 0xA5u;
    (void)aw9523_model_read_regs(&model, model.addr, reg, &val, 1);
    return val;
}

static void bus_write(uint8_t reg, uint8_t val)
{
    ASSERT_TRUE(aw9523_model_write_reg(&model, model.addr, reg, val));
}

/* --- what the model itself must do ------------------------------------- */

/* p17 Table-4 */
TEST(test_table_4_reset_values)
{
    setup();
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CONFIG_P0), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CONFIG_P1), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INT_P0), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INT_P1), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_ID), 0x23u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CTL), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P0), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P1), 0xFFu);
    for (uint8_t i = 0; i < 16u; i++) {
        ASSERT_EQ_HEX(bus_read((uint8_t)(AW9523M_REG_DIM_BASE + i)), 0x00u);
    }
}

/* p10-11 Table 1: the AD pins set the output defaults. Hi-Z on P0 is a 1 in
 * the latch, since p10 has P0 open-drain by default. */
static const struct { bool ad1; bool ad0; uint8_t output; } table_1[] = {
    { false, false, 0x00u },
    { false, true,  0x0Fu },
    { true,  false, 0xF0u },
    { true,  true,  0xFFu },
};

TEST(test_table_1_output_defaults)
{
    for (size_t i = 0; i < sizeof(table_1) / sizeof(table_1[0]); i++) {
        setup();
        aw9523_model_set_straps(&model, table_1[i].ad1, table_1[i].ad0);
        ASSERT_EQ_HEX(bus_read(AW9523M_REG_OUTPUT_P0), table_1[i].output);
        ASSERT_EQ_HEX(bus_read(AW9523M_REG_OUTPUT_P1), table_1[i].output);
        /* P1 is push-pull, so the pins follow; P0 stays low with no pull-up */
        ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P1), table_1[i].output);
        ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P0), 0x00u);
    }
}

/* p15: after reset "All GPIO are configured as output, which value is decided
 * by 2 device address (AD1/AD0)", and p16: the software reset state "is the
 * same as hardware reset" */
TEST(test_soft_reset_returns_to_table_1)
{
    for (size_t i = 0; i < sizeof(table_1) / sizeof(table_1[0]); i++) {
        setup();
        aw9523_model_set_straps(&model, table_1[i].ad1, table_1[i].ad0);
        bus_write(AW9523M_REG_OUTPUT_P0, 0x55u);
        bus_write(AW9523M_REG_OUTPUT_P1, 0x55u);
        bus_write(AW9523M_REG_RESET, 0x00u);
        ASSERT_EQ_HEX(bus_read(AW9523M_REG_OUTPUT_P0), table_1[i].output);
        ASSERT_EQ_HEX(bus_read(AW9523M_REG_OUTPUT_P1), table_1[i].output);
    }
}

/* p16: "Writing 00H to the software register(7FH) will generate a reset pulse."
 * Other values do nothing. */
TEST(test_only_00h_resets)
{
    setup();
    bus_write(AW9523M_REG_CONFIG_P0, 0xFFu);
    bus_write(AW9523M_REG_RESET, 0x01u);
    bus_write(AW9523M_REG_RESET, 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CONFIG_P0), 0xFFu);
    bus_write(AW9523M_REG_RESET, 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CONFIG_P0), 0x00u);
}

TEST(test_soft_reset_restores_every_register)
{
    setup();
    bus_write(AW9523M_REG_CONFIG_P1, 0x0Fu);
    bus_write(AW9523M_REG_INT_P0, 0xFFu);
    bus_write(AW9523M_REG_CTL, 0x10u);
    bus_write(AW9523M_REG_LED_MODE_P0, 0x00u);
    bus_write(AW9523M_REG_LED_MODE_P1, 0x00u);
    bus_write(AW9523M_REG_DIM_BASE, 0x80u);
    bus_write(AW9523M_REG_RESET, 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CONFIG_P1), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INT_P0), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CTL), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P0), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P1), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_DIM_BASE), 0x00u);
}

/* p18: the Input registers: "Writes to these registers have no effect." */
TEST(test_input_registers_ignore_writes)
{
    setup();
    bus_write(AW9523M_REG_INPUT_P0, 0xFFu);
    bus_write(AW9523M_REG_INPUT_P1, 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P0), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P1), 0x00u);
}

/* p19: "ID register, read only, the readout value is 23H" */
TEST(test_id_is_read_only)
{
    setup();
    bus_write(AW9523M_REG_ID, 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_ID), 0x23u);
}

/* p10: "P0 port is Open-Drain mode (default)", and p19: "if D[4]=0, P0 port is
 * Open-Drain mode". A 1 lets go of the pin, a 0 pulls it low. */
TEST(test_p0_open_drain_by_default)
{
    setup();
    bus_write(AW9523M_REG_OUTPUT_P0, 0x0Fu);

    aw9523_model_set_pin_input_p0(&model, 0x00u);  /* no pull-up */
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P0), 0x00u);

    aw9523_model_set_pin_input_p0(&model, 0xFFu);  /* pull-up on every pin */
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P0), 0x0Fu);
}

/* p19: "if D[4]=1, P0 port is Push-Pull mode", no pull-up needed */
TEST(test_gpomd_makes_p0_push_pull)
{
    setup();
    bus_write(AW9523M_REG_OUTPUT_P0, 0x0Fu);
    bus_write(AW9523M_REG_CTL, 0x10u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P0), 0x0Fu);

    bus_write(AW9523M_REG_CTL, 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P0), 0x00u);
}

/* p10: "The P1 port is Push-Pull mode", whatever GPOMD holds */
TEST(test_p1_always_push_pull)
{
    setup();
    bus_write(AW9523M_REG_OUTPUT_P1, 0x3Cu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P1), 0x3Cu);
}

/* p18: reads of Output "reflect the value that is in the flip-flop controlling
 * the output selection, not the actual pin value" */
TEST(test_output_reads_the_latch_not_the_pin)
{
    setup();
    bus_write(AW9523M_REG_OUTPUT_P0, 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P0), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_OUTPUT_P0), 0xFFu);
}

/* p18: "Bit values in this register have no effect on pins defined as inputs",
 * and Input shows the pin "regardless of whether the pin is defined as an
 * input or an output" */
TEST(test_output_has_no_effect_on_input_pins)
{
    setup();
    bus_write(AW9523M_REG_CONFIG_P1, 0xFFu);
    aw9523_model_set_pin_input_p1(&model, 0xA5u);
    bus_write(AW9523M_REG_OUTPUT_P1, 0x5Au);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INPUT_P1), 0xA5u);
}

/* p13: the device address is 1 0 1 1 0 AD1 AD0, and only that one answers */
TEST(test_only_the_strapped_address_answers)
{
    setup();
    aw9523_model_set_straps(&model, true, false);
    uint8_t val = 0;
    ASSERT_TRUE(aw9523_model_read_regs(&model, 0x5Au, AW9523M_REG_ID, &val, 1));
    ASSERT_FALSE(aw9523_model_read_regs(&model, 0x58u, AW9523M_REG_ID, &val, 1));
    ASSERT_FALSE(aw9523_model_write_reg(&model, 0x59u, AW9523M_REG_CTL, 0x10u));
}

/* --- the driver against the model --------------------------------------- */

TEST(test_init_at_all_four_addresses)
{
    for (size_t i = 0; i < sizeof(table_1) / sizeof(table_1[0]); i++) {
        setup();
        aw9523_model_set_straps(&model, table_1[i].ad1, table_1[i].ad0);
        uint8_t addr = (uint8_t)(0x58u | (table_1[i].ad1 ? 2u : 0u) |
                                 (table_1[i].ad0 ? 1u : 0u));
        ASSERT_EQ(aw9523_init(&dev, &model_i2c, addr), AW9523_OK);
    }
}

TEST(test_init_wrong_address_fails)
{
    setup();
    ASSERT_EQ(aw9523_init(&dev, &model_i2c, 0x59u), AW9523_ERR_I2C);
}

TEST(test_init_wrong_id)
{
    setup();
    model.hw.reset_values[AW9523M_REG_ID] = 0x00u;
    hw_model_reset(&model.hw);
    ASSERT_EQ(aw9523_init(&dev, &model_i2c, ADDR), AW9523_ERR_ID);
}

/* aw9523.h promises GPIO inputs, interrupts off, latches low. Config 1 is
 * input (p18), Int 1 is disabled (p18), LED mode 1 is GPIO (p19). */
TEST(test_init_state)
{
    setup_driver();
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CONFIG_P0), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CONFIG_P1), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INT_P0), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INT_P1), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P0), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P1), 0xFFu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_OUTPUT_P0), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_OUTPUT_P1), 0x00u);
}

/* p19: bits D[7:5] and D[3:2] of CTL "must configure to 0, or system function
 * error may occur"; p20: of the reserved registers, "user should not write
 * these registers, or may cause function error" */
TEST(test_init_stays_off_test_bits_and_reserved_registers)
{
    setup_driver();
    ASSERT_EQ(model.ctl_test_bit_writes, 0);
    ASSERT_EQ(model.reserved_writes, 0);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CTL), 0x10u);
}

/* DIM values outlive an MCU restart unless init resets the part */
TEST(test_init_clears_dim_left_by_an_earlier_run)
{
    setup();
    bus_write(AW9523M_REG_DIM_BASE, 0x80u);
    ASSERT_EQ(aw9523_init(&dev, &model_i2c, ADDR), AW9523_OK);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_DIM_BASE), 0x00u);
}

/* aw9523.h promises push-pull, so P0 must drive high with no pull-up */
TEST(test_p0_output_drives_high_after_init)
{
    setup_driver();
    ASSERT_EQ(aw9523_set_direction(&dev, AW9523_PORT0, 3, AW9523_DIR_OUTPUT),
              AW9523_OK);
    ASSERT_EQ(aw9523_write_pin(&dev, AW9523_PORT0, 3, AW9523_PIN_HIGH), AW9523_OK);

    aw9523_level_t level = AW9523_PIN_LOW;
    ASSERT_EQ(aw9523_read_pin(&dev, AW9523_PORT0, 3, &level), AW9523_OK);
    ASSERT_EQ(level, AW9523_PIN_HIGH);
}

TEST(test_read_pin_sees_the_outside_level)
{
    setup_driver();
    aw9523_model_set_pin_input_p1(&model, 0x40u);
    aw9523_level_t level = AW9523_PIN_LOW;
    ASSERT_EQ(aw9523_read_pin(&dev, AW9523_PORT1, 6, &level), AW9523_OK);
    ASSERT_EQ(level, AW9523_PIN_HIGH);
    ASSERT_EQ(aw9523_read_pin(&dev, AW9523_PORT1, 5, &level), AW9523_OK);
    ASSERT_EQ(level, AW9523_PIN_LOW);
}

TEST(test_write_port_reaches_the_pins)
{
    setup_driver();
    bus_write(AW9523M_REG_CONFIG_P1, 0x00u);
    ASSERT_EQ(aw9523_write_port(&dev, AW9523_PORT1, 0xC3u), AW9523_OK);
    uint8_t val = 0;
    ASSERT_EQ(aw9523_read_port(&dev, AW9523_PORT1, &val), AW9523_OK);
    ASSERT_EQ_HEX(val, 0xC3u);
}

/* p20 Table 13: which DIM register belongs to which pin */
TEST(test_led_current_uses_table_13)
{
    static const struct { aw9523_port_t port; uint8_t pin; uint8_t reg; } rows[] = {
        { AW9523_PORT1, 0, 0x20u }, { AW9523_PORT1, 1, 0x21u },
        { AW9523_PORT1, 2, 0x22u }, { AW9523_PORT1, 3, 0x23u },
        { AW9523_PORT0, 0, 0x24u }, { AW9523_PORT0, 1, 0x25u },
        { AW9523_PORT0, 2, 0x26u }, { AW9523_PORT0, 3, 0x27u },
        { AW9523_PORT0, 4, 0x28u }, { AW9523_PORT0, 5, 0x29u },
        { AW9523_PORT0, 6, 0x2Au }, { AW9523_PORT0, 7, 0x2Bu },
        { AW9523_PORT1, 4, 0x2Cu }, { AW9523_PORT1, 5, 0x2Du },
        { AW9523_PORT1, 6, 0x2Eu }, { AW9523_PORT1, 7, 0x2Fu },
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        setup_driver();
        ASSERT_EQ(aw9523_set_led_current(&dev, rows[i].port, rows[i].pin, 0x7Eu),
                  AW9523_OK);
        ASSERT_EQ_HEX(bus_read(rows[i].reg), 0x7Eu);
    }
}

/* p19: "1：GPIO mode", "0：LED mode" */
TEST(test_led_mode_clears_only_its_bit)
{
    setup_driver();
    ASSERT_EQ(aw9523_set_led_mode(&dev, AW9523_PORT0, 2, AW9523_MODE_LED), AW9523_OK);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P0), 0xFBu);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P1), 0xFFu);
    ASSERT_EQ(aw9523_set_led_mode(&dev, AW9523_PORT0, 2, AW9523_MODE_GPIO), AW9523_OK);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_LED_MODE_P0), 0xFFu);
}

/* p18: "0-enable; 1-disable", while the API takes 1 as enabled */
TEST(test_interrupt_mask_is_inverted)
{
    setup_driver();
    ASSERT_EQ(aw9523_set_interrupt_enable(&dev, AW9523_PORT1, 0x0Fu), AW9523_OK);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INT_P1), 0xF0u);
}

TEST(test_driver_reset_returns_to_defaults)
{
    setup_driver();
    ASSERT_EQ(aw9523_reset(&dev), AW9523_OK);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CONFIG_P0), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_INT_P1), 0x00u);
    ASSERT_EQ_HEX(bus_read(AW9523M_REG_CTL), 0x00u);
}

TEST_MAIN("aw9523 hardware model")
    RUN(test_table_4_reset_values);
    RUN(test_table_1_output_defaults);
    RUN(test_soft_reset_returns_to_table_1);
    RUN(test_only_00h_resets);
    RUN(test_soft_reset_restores_every_register);
    RUN(test_input_registers_ignore_writes);
    RUN(test_id_is_read_only);
    RUN(test_p0_open_drain_by_default);
    RUN(test_gpomd_makes_p0_push_pull);
    RUN(test_p1_always_push_pull);
    RUN(test_output_reads_the_latch_not_the_pin);
    RUN(test_output_has_no_effect_on_input_pins);
    RUN(test_only_the_strapped_address_answers);
    RUN(test_init_at_all_four_addresses);
    RUN(test_init_wrong_address_fails);
    RUN(test_init_wrong_id);
    RUN(test_init_state);
    RUN(test_init_stays_off_test_bits_and_reserved_registers);
    RUN(test_init_clears_dim_left_by_an_earlier_run);
    RUN(test_p0_output_drives_high_after_init);
    RUN(test_read_pin_sees_the_outside_level);
    RUN(test_write_port_reaches_the_pins);
    RUN(test_led_current_uses_table_13);
    RUN(test_led_mode_clears_only_its_bit);
    RUN(test_interrupt_mask_is_inverted);
    RUN(test_driver_reset_returns_to_defaults);
TEST_MAIN_END()
