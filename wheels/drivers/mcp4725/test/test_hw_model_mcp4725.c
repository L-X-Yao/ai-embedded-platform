// SPDX-License-Identifier: MIT
/*
 * test_hw_model_mcp4725.c — tests using hardware model
 *
 * Validates MCP4725 driver against a datasheet-accurate model.
 * The model enforces command byte encoding, DAC/EEPROM separation,
 * power-down modes, and read-back format.
 */

#include "test_harness.h"
#include "hw_model_mcp4725.h"
#include "../mcp4725.h"

static mcp4725_model_t model;

static const mcp4725_i2c_t model_i2c = {
    .write = mcp4725_model_write,
    .read  = mcp4725_model_read,
    .ctx   = &model,
};

static mcp4725_t dev;

static void setup(void)
{
    mcp4725_model_init(&model);
    mcp4725_init(&dev, &model_i2c, MCP4725_ADDR_A);
}

/* --- init --- */

TEST(test_init_reads_status)
{
    mcp4725_model_init(&model);
    mcp4725_t d;
    mcp4725_init(&d, &model_i2c, MCP4725_ADDR_A);
    ASSERT_EQ(model.read_count, 1);
}

/* --- fast write --- */

TEST(test_fast_write_zero)
{
    setup();
    mcp4725_write_fast(&dev, 0);
    ASSERT_EQ(model.dac_value, 0);
    ASSERT_EQ(model.dac_pd, 0);
}

TEST(test_fast_write_midscale)
{
    setup();
    mcp4725_write_fast(&dev, 2048);
    ASSERT_EQ(model.dac_value, 2048);
}

TEST(test_fast_write_max)
{
    setup();
    mcp4725_write_fast(&dev, 4095);
    ASSERT_EQ(model.dac_value, 4095);
}

TEST(test_fast_write_does_not_touch_eeprom)
{
    setup();
    mcp4725_model_set_eeprom(&model, 1000, 0);
    mcp4725_write_fast(&dev, 3000);
    ASSERT_EQ(model.dac_value, 3000);
    ASSERT_EQ(model.eeprom_value, 1000);
}

/* --- DAC register write --- */

TEST(test_dac_write_normal)
{
    setup();
    mcp4725_write_dac(&dev, 1500, MCP4725_PD_NORMAL);
    ASSERT_EQ(model.dac_value, 1500);
    ASSERT_EQ(model.dac_pd, 0);
}

TEST(test_dac_write_pd_1k)
{
    setup();
    mcp4725_write_dac(&dev, 0, MCP4725_PD_1K);
    ASSERT_EQ(model.dac_pd, 1);
}

TEST(test_dac_write_pd_100k)
{
    setup();
    mcp4725_write_dac(&dev, 0, MCP4725_PD_100K);
    ASSERT_EQ(model.dac_pd, 2);
}

TEST(test_dac_write_pd_500k)
{
    setup();
    mcp4725_write_dac(&dev, 0, MCP4725_PD_500K);
    ASSERT_EQ(model.dac_pd, 3);
}

TEST(test_dac_write_does_not_touch_eeprom)
{
    setup();
    mcp4725_model_set_eeprom(&model, 500, 1);
    mcp4725_write_dac(&dev, 2000, MCP4725_PD_NORMAL);
    ASSERT_EQ(model.eeprom_value, 500);
    ASSERT_EQ(model.eeprom_pd, 1);
}

/* --- EEPROM write --- */

TEST(test_eeprom_write_updates_both)
{
    setup();
    mcp4725_write_eeprom(&dev, 3500, MCP4725_PD_NORMAL);
    ASSERT_EQ(model.dac_value, 3500);
    ASSERT_EQ(model.eeprom_value, 3500);
}

TEST(test_eeprom_write_with_pd)
{
    setup();
    mcp4725_write_eeprom(&dev, 1000, MCP4725_PD_100K);
    ASSERT_EQ(model.dac_pd, 2);
    ASSERT_EQ(model.eeprom_pd, 2);
}

/* --- read-back --- */

TEST(test_read_status_dac_value)
{
    setup();
    mcp4725_write_dac(&dev, 2500, MCP4725_PD_NORMAL);

    mcp4725_status_t st;
    mcp4725_read_status(&dev, &st);
    ASSERT_EQ(st.dac_value, 2500);
    ASSERT_EQ(st.pd_mode, 0);
}

TEST(test_read_status_eeprom)
{
    setup();
    mcp4725_write_eeprom(&dev, 1234, MCP4725_PD_500K);

    mcp4725_status_t st;
    mcp4725_read_status(&dev, &st);
    ASSERT_EQ(st.eeprom_value, 1234);
    ASSERT_EQ(st.eeprom_pd, 3);
}

TEST(test_read_status_busy_flag)
{
    setup();
    model.eeprom_busy = true;

    mcp4725_status_t st;
    mcp4725_read_status(&dev, &st);
    ASSERT_EQ(st.eeprom_busy, true);
}

TEST(test_read_status_ready_flag)
{
    setup();
    model.eeprom_busy = false;

    mcp4725_status_t st;
    mcp4725_read_status(&dev, &st);
    ASSERT_EQ(st.eeprom_busy, false);
}

/* DS22039D Figure 6-3, checked on the model's bytes without the driver */
TEST(test_model_read_frame_figure_6_3)
{
    mcp4725_model_init(&model);
    model.dac_value    = 0xABCu;
    model.dac_pd       = 2;
    model.eeprom_value = 0x123u;
    model.eeprom_pd    = 1;

    uint8_t b[5];
    ASSERT_TRUE(mcp4725_model_read(&model, MCP4725_ADDR_A, b, 5));
    ASSERT_EQ_HEX(b[0], 0xC4u);  /* RDY/BSY POR X X X PD1 PD0 X: 1 1 000 10 0 */
    ASSERT_EQ_HEX(b[1], 0xABu);  /* D11-D4 */
    ASSERT_EQ_HEX(b[2], 0xC0u);  /* D3-D0 X X X X */
    ASSERT_EQ_HEX(b[3], 0x21u);  /* X PD1 PD0 X D11-D8: 0 01 0 0001 */
    ASSERT_EQ_HEX(b[4], 0x23u);  /* D7-D0 */
}

/* --- edge values --- */

TEST(test_all_zeros)
{
    setup();
    mcp4725_write_dac(&dev, 0, MCP4725_PD_NORMAL);

    mcp4725_status_t st;
    mcp4725_read_status(&dev, &st);
    ASSERT_EQ(st.dac_value, 0);
    ASSERT_EQ(st.pd_mode, 0);
}

TEST(test_all_ones)
{
    setup();
    mcp4725_write_dac(&dev, 4095, MCP4725_PD_500K);

    mcp4725_status_t st;
    mcp4725_read_status(&dev, &st);
    ASSERT_EQ(st.dac_value, 4095);
    ASSERT_EQ(st.pd_mode, 3);
}

TEST(test_sequential_writes)
{
    setup();
    for (uint16_t v = 0; v < 4096; v += 1000) {
        mcp4725_write_dac(&dev, v, MCP4725_PD_NORMAL);
        ASSERT_EQ(model.dac_value, v);
    }
}

TEST(test_fast_then_dac_then_eeprom)
{
    setup();
    mcp4725_write_fast(&dev, 100);
    ASSERT_EQ(model.dac_value, 100);
    ASSERT_EQ(model.eeprom_value, 0);

    mcp4725_write_dac(&dev, 200, MCP4725_PD_1K);
    ASSERT_EQ(model.dac_value, 200);
    ASSERT_EQ(model.eeprom_value, 0);

    mcp4725_write_eeprom(&dev, 300, MCP4725_PD_NORMAL);
    ASSERT_EQ(model.dac_value, 300);
    ASSERT_EQ(model.eeprom_value, 300);
}

TEST_MAIN("mcp4725 hardware model")
    RUN(test_init_reads_status);
    RUN(test_fast_write_zero);
    RUN(test_fast_write_midscale);
    RUN(test_fast_write_max);
    RUN(test_fast_write_does_not_touch_eeprom);
    RUN(test_dac_write_normal);
    RUN(test_dac_write_pd_1k);
    RUN(test_dac_write_pd_100k);
    RUN(test_dac_write_pd_500k);
    RUN(test_dac_write_does_not_touch_eeprom);
    RUN(test_eeprom_write_updates_both);
    RUN(test_eeprom_write_with_pd);
    RUN(test_read_status_dac_value);
    RUN(test_read_status_eeprom);
    RUN(test_read_status_busy_flag);
    RUN(test_read_status_ready_flag);
    RUN(test_model_read_frame_figure_6_3);
    RUN(test_all_zeros);
    RUN(test_all_ones);
    RUN(test_sequential_writes);
    RUN(test_fast_then_dac_then_eeprom);
TEST_MAIN_END()
