// SPDX-License-Identifier: MIT
/*
 * test_hw_model_mcp4728.c — tests using hardware model
 *
 * Validates MCP4728 driver against a datasheet-accurate model.
 * The model enforces Multi-Write command byte encoding and channel
 * mapping, rejecting malformed protocol bytes.
 */

#include "test_harness.h"
#include "hw_model_mcp4728.h"
#include "../mcp4728.h"

static mcp4728_model_t model;

static const mcp4728_i2c_t model_i2c = {
    .write_regs = mcp4728_model_write_regs,
    .read_regs  = mcp4728_model_read_regs,
    .ctx        = &model,
};

static mcp4728_t dev;

static const mcp4728_cfg_t default_cfg = {
    .addr = MCP4728_ADDR_DEFAULT,
};

static void setup(void)
{
    mcp4728_model_init(&model);
    mcp4728_init(&dev, &model_i2c, &default_cfg);
}

/* --- init --- */

TEST(test_hw_init_ok)
{
    mcp4728_model_init(&model);
    mcp4728_t d;
    ASSERT_EQ(mcp4728_init(&d, &model_i2c, &default_cfg), MCP4728_OK);
    /* No I2C traffic on init (DAC has no chip ID) */
    ASSERT_EQ(model.write_count, 0);
    ASSERT_EQ(model.read_count, 0);
}

/* --- single channel writes --- */

TEST(test_hw_write_channel_a_zero)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 0), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 0);
}

TEST(test_hw_write_channel_a_midscale)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 2048), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 2048);
}

TEST(test_hw_write_channel_a_max)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 4095), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 4095);
}

TEST(test_hw_write_channel_b)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 1, 1500), MCP4728_OK);
    ASSERT_EQ(model.channel_value[1], 1500);
    /* Channel A should remain at POR default (0) */
    ASSERT_EQ(model.channel_value[0], 0);
}

TEST(test_hw_write_channel_c)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 2, 3000), MCP4728_OK);
    ASSERT_EQ(model.channel_value[2], 3000);
}

TEST(test_hw_write_channel_d)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 3, 4000), MCP4728_OK);
    ASSERT_EQ(model.channel_value[3], 4000);
}

/* --- write_all --- */

TEST(test_hw_write_all_varied)
{
    setup();
    const uint16_t vals[4] = {0, 1000, 2500, 4095};
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 0);
    ASSERT_EQ(model.channel_value[1], 1000);
    ASSERT_EQ(model.channel_value[2], 2500);
    ASSERT_EQ(model.channel_value[3], 4095);
    ASSERT_EQ(model.write_count, 1);
}

TEST(test_hw_write_all_then_overwrite_one)
{
    setup();
    const uint16_t vals[4] = {100, 200, 300, 400};
    mcp4728_write_all(&dev, vals);

    mcp4728_write_channel(&dev, 2, 999);

    ASSERT_EQ(model.channel_value[0], 100);
    ASSERT_EQ(model.channel_value[1], 200);
    ASSERT_EQ(model.channel_value[2], 999);
    ASSERT_EQ(model.channel_value[3], 400);
}

/* --- edge cases --- */

TEST(test_hw_sequential_writes_same_channel)
{
    setup();
    for (uint16_t v = 0; v < 4096; v += 500) {
        mcp4728_write_channel(&dev, 0, v);
        ASSERT_EQ(model.channel_value[0], v);
    }
}

TEST(test_hw_all_channels_max)
{
    setup();
    const uint16_t vals[4] = {4095, 4095, 4095, 4095};
    mcp4728_write_all(&dev, vals);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(model.channel_value[i], 4095);
    }
}

TEST(test_hw_i2c_fail_injection)
{
    setup();
    model.fail_next = true;
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 100), MCP4728_ERR_I2C);
    /* Value should not have been written */
    ASSERT_EQ(model.channel_value[0], 0);
}

/* --- the model against the manual, without the driver --- */

/* DS22187E Table 5-1: C2 C1 C0 W1 W0 = 0 1 0 0 0 is Multi-Write. The other
 * commands with C2 C1 = 0 1 mean something else and must not store data. */
TEST(test_hw_model_rejects_other_commands)
{
    static const uint8_t cmds[] = {
        0x50u,  /* 0 1 0 1 0: Sequential Write */
        0x58u,  /* 0 1 0 1 1: Single Write */
        0x60u,  /* 0 1 1 X X: Write I2C Address */
    };
    const uint8_t data[2] = { 0x08u, 0x00u };

    setup();
    for (size_t i = 0; i < sizeof(cmds); i++) {
        ASSERT_FALSE(mcp4728_model_write_regs(&model, 0x60u, cmds[i], data, 2));
        ASSERT_EQ(model.channel_value[0], 0);
    }
}

/* Figure 5-8: the 3rd byte is VREF PD1 PD0 Gx D11-D8, so its upper nibble
 * is configuration, not an error. */
TEST(test_hw_model_accepts_config_bits)
{
    const uint8_t data[2] = { 0x98u, 0x00u };  /* 1 00 1 1000 */

    setup();
    ASSERT_TRUE(mcp4728_model_write_regs(&model, 0x60u, 0x40u, data, 2));
    ASSERT_EQ_HEX(model.channel_value[0], 0x800u);
    ASSERT_EQ(model.channel_vref[0], 1);
    ASSERT_EQ(model.channel_pd[0], 0);
    ASSERT_EQ(model.channel_gain[0], 1);
}

/* Figure 5-8: bytes 2-4 repeat, and the top five bits of a repeated 2nd
 * byte are don't care (Note 3). */
TEST(test_hw_model_repeated_bytes)
{
    const uint8_t data[5] = {
        0x01u, 0x23u,  /* channel A: 0 00 0 0001, 0x23 */
        0xFAu,         /* X X X X X, DAC1 DAC0 = 01, UDAC = 0 */
        0xD4u, 0x56u,  /* channel B: 1 10 1 0100, 0x56 */
    };

    setup();
    ASSERT_TRUE(mcp4728_model_write_regs(&model, 0x60u, 0x40u, data, 5));
    ASSERT_EQ_HEX(model.channel_value[0], 0x123u);
    ASSERT_EQ_HEX(model.channel_value[1], 0x456u);
    ASSERT_EQ(model.channel_vref[1], 1);
    ASSERT_EQ(model.channel_pd[1], 2);
    ASSERT_EQ(model.channel_gain[1], 1);
}

/* The driver writes VREF = 0, so a factory channel (Table 4-2: VREF = 1,
 * internal 2.048 V) moves to VDD at its first write. Others stay. */
TEST(test_hw_write_moves_channel_to_vdd)
{
    setup();
    ASSERT_EQ(model.channel_vref[0], 1);
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 2048), MCP4728_OK);
    ASSERT_EQ(model.channel_vref[0], 0);
    ASSERT_EQ(model.channel_pd[0], 0);
    ASSERT_EQ(model.channel_gain[0], 0);
    ASSERT_EQ(model.channel_vref[1], 1);
}

/* A driver set up for the internal reference keeps a factory channel on it,
 * so the output scale does not change at the first write. */
TEST(test_hw_write_keeps_internal_reference)
{
    const mcp4728_cfg_t cfg = {
        .addr = MCP4728_ADDR_DEFAULT, .vref_internal = 0x0Fu,
    };

    mcp4728_model_init(&model);
    ASSERT_EQ(mcp4728_init(&dev, &model_i2c, &cfg), MCP4728_OK);
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 2048), MCP4728_OK);
    ASSERT_EQ(model.channel_value[0], 2048);
    ASSERT_EQ(model.channel_vref[0], 1);
    ASSERT_EQ(model.channel_pd[0], 0);
    ASSERT_EQ(model.channel_gain[0], 0);
}

TEST(test_hw_write_all_config_per_channel)
{
    const mcp4728_cfg_t cfg = {
        .addr = MCP4728_ADDR_DEFAULT, .vref_internal = 0x0Bu, .gain_x2 = 0x09u,
    };
    const uint16_t vals[4] = {100, 200, 300, 400};
    static const uint8_t vref[4] = {1, 1, 0, 1};
    static const uint8_t gain[4] = {1, 0, 0, 1};

    mcp4728_model_init(&model);
    ASSERT_EQ(mcp4728_init(&dev, &model_i2c, &cfg), MCP4728_OK);
    ASSERT_EQ(mcp4728_write_all(&dev, vals), MCP4728_OK);
    ASSERT_EQ(model.write_count, 1);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(model.channel_value[i], vals[i]);
        ASSERT_EQ(model.channel_vref[i], vref[i]);
        ASSERT_EQ(model.channel_gain[i], gain[i]);
        ASSERT_EQ(model.channel_pd[i], 0);
    }
}

/* --- what the model itself must do (DS22187E) --- */

/* p24 Table 4-2: every channel leaves the factory with VREF = 1, PD = 00,
 * Gx = 0 and code 0 */
TEST(test_hw_factory_eeprom_at_power_up)
{
    mcp4728_model_init(&model);
    for (int ch = 0; ch < 4; ch++) {
        ASSERT_EQ(model.eeprom_vref[ch], 1);
        ASSERT_EQ(model.eeprom_pd[ch], 0);
        ASSERT_EQ(model.eeprom_gain[ch], 0);
        ASSERT_EQ(model.eeprom_value[ch], 0);
        ASSERT_EQ(model.channel_vref[ch], 1);
        ASSERT_EQ(model.channel_value[ch], 0);
    }
}

/* p23: "each channel uploads all configuration and DAC input codes from
 * EEPROM", into "both of the DAC input and output registers simultaneously",
 * and the output is there "regardless of the LDAC and UDAC bit conditions" */
TEST(test_hw_power_on_uploads_the_eeprom)
{
    mcp4728_model_init(&model);
    model.eeprom_value[2] = 0x0ABCu;
    model.eeprom_vref[2]  = 0;
    model.eeprom_pd[2]    = 2;
    model.eeprom_gain[2]  = 1;
    mcp4728_model_power_on(&model);
    ASSERT_EQ_HEX(model.channel_value[2], 0x0ABCu);
    ASSERT_EQ_HEX(model.channel_output[2], 0x0ABCu);
    ASSERT_EQ(model.channel_vref[2], 0);
    ASSERT_EQ(model.channel_pd[2], 2);
    ASSERT_EQ(model.channel_gain[2], 1);
}

/* p34 Table 5-1, Multi-Write: "EEPROM is not affected", so a power cycle takes
 * the channel back to what the EEPROM holds */
TEST(test_hw_multi_write_is_lost_at_power_off)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 0, 2048), MCP4728_OK);
    ASSERT_EQ(model.eeprom_value[0], 0);
    ASSERT_EQ(model.eeprom_vref[0], 1);

    mcp4728_model_power_on(&model);
    ASSERT_EQ(model.channel_value[0], 0);
    ASSERT_EQ(model.channel_output[0], 0);
    ASSERT_EQ(model.channel_vref[0], 1);
}

/* p45 Figure 5-15: per channel, A to D, three bytes of the DAC input register
 * and then three of the EEPROM. The first of each three is RDY/BSY POR DAC1
 * DAC0 0 A2 A1 A0, RDY/BSY = 1 when idle (p25), POR = 1 when powered. */
TEST(test_hw_read_frame_follows_figure_5_15)
{
    static const uint8_t want[24] = {
        0xC0u, 0x80u, 0x00u, 0xC0u, 0x80u, 0x00u,  /* A: factory, both */
        0xD0u, 0x0Au, 0xBCu, 0xD0u, 0x80u, 0x00u,  /* B: written, then EEPROM */
        0xE0u, 0x80u, 0x00u, 0xE0u, 0x80u, 0x00u,  /* C */
        0xF0u, 0x80u, 0x00u, 0xF0u, 0x80u, 0x00u,  /* D */
    };
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 1, 0x0ABCu), MCP4728_OK);

    uint8_t frame[24];
    memset(frame, 0xA5, sizeof(frame));
    ASSERT_TRUE(mcp4728_model_read_regs(&model, MCP4728_ADDR_DEFAULT, 0,
                                        frame, sizeof(frame)));
    ASSERT_MEMEQ_N(frame, want, sizeof(want));
}

/* The same frame carries the VREF, PD and Gx bits and the address bits */
TEST(test_hw_read_shows_config_and_address)
{
    mcp4728_model_init(&model);
    model.channel_vref[0]  = 1;
    model.channel_pd[0]    = 3;
    model.channel_gain[0]  = 1;
    model.channel_value[0] = 0x0123u;

    uint8_t frame[3];
    ASSERT_TRUE(mcp4728_model_read_regs(&model, 0x65u, 0, frame, sizeof(frame)));
    ASSERT_EQ_HEX(frame[0], 0xC5u);
    ASSERT_EQ_HEX(frame[1], 0xF1u);
    ASSERT_EQ_HEX(frame[2], 0x23u);
}

/* p39 Figure 5-8: "If UDAC = 0 or LDAC Pin = 0: VOUT is updated after the 4th
 * byte's ACK is issued." */
TEST(test_hw_udac_set_leaves_the_output)
{
    mcp4728_model_init(&model);
    uint8_t data[2] = {0x01u, 0x23u};
    ASSERT_TRUE(mcp4728_model_write_regs(&model, MCP4728_ADDR_DEFAULT, 0x41u,
                                         data, sizeof(data)));
    ASSERT_EQ_HEX(model.channel_value[0], 0x0123u);
    ASSERT_EQ_HEX(model.channel_output[0], 0x0000u);

    model.ldac_low = true;
    ASSERT_TRUE(mcp4728_model_write_regs(&model, MCP4728_ADDR_DEFAULT, 0x41u,
                                         data, sizeof(data)));
    ASSERT_EQ_HEX(model.channel_output[0], 0x0123u);
}

/* The driver has no LDAC, so it must write UDAC = 0 for the output to move */
TEST(test_hw_driver_writes_reach_the_output)
{
    setup();
    ASSERT_EQ(mcp4728_write_channel(&dev, 3, 1000), MCP4728_OK);
    ASSERT_EQ(model.channel_output[3], 1000);

    const uint16_t values[4] = {10, 20, 30, 40};
    ASSERT_EQ(mcp4728_write_all(&dev, values), MCP4728_OK);
    for (int ch = 0; ch < 4; ch++) {
        ASSERT_EQ(model.channel_output[ch], values[ch]);
    }
}

TEST_MAIN("mcp4728 hardware model")
    RUN(test_hw_init_ok);
    RUN(test_hw_write_channel_a_zero);
    RUN(test_hw_write_channel_a_midscale);
    RUN(test_hw_write_channel_a_max);
    RUN(test_hw_write_channel_b);
    RUN(test_hw_write_channel_c);
    RUN(test_hw_write_channel_d);
    RUN(test_hw_write_all_varied);
    RUN(test_hw_write_all_then_overwrite_one);
    RUN(test_hw_sequential_writes_same_channel);
    RUN(test_hw_all_channels_max);
    RUN(test_hw_i2c_fail_injection);
    RUN(test_hw_model_rejects_other_commands);
    RUN(test_hw_model_accepts_config_bits);
    RUN(test_hw_model_repeated_bytes);
    RUN(test_hw_write_moves_channel_to_vdd);
    RUN(test_hw_write_keeps_internal_reference);
    RUN(test_hw_write_all_config_per_channel);
    RUN(test_hw_factory_eeprom_at_power_up);
    RUN(test_hw_power_on_uploads_the_eeprom);
    RUN(test_hw_multi_write_is_lost_at_power_off);
    RUN(test_hw_read_frame_follows_figure_5_15);
    RUN(test_hw_read_shows_config_and_address);
    RUN(test_hw_udac_set_leaves_the_output);
    RUN(test_hw_driver_writes_reach_the_output);
TEST_MAIN_END()
