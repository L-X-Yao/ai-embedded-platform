// SPDX-License-Identifier: MIT
/*
 * test_hw_model_gc9a01.c -- tests using GC9A01 SPI display model
 *
 * Validates the gc9a01 driver against a datasheet-accurate command
 * processor model that tracks internal display state:
 *   sleep mode, display on/off, inversion, MADCTL rotation, COLMOD,
 *   column/row address window, pixel data, command sequencing.
 *
 * Reference: GalaxyCore GC9A01 datasheet
 */

#include "test_harness.h"
#include "hw_model_gc9a01.h"
#include "../gc9a01.h"

static gc9a01_model_t model;
static gc9a01_spi_t   model_spi;
static gc9a01_t       dev;

static gc9a01_err_t setup(void)
{
    gc9a01_model_init(&model);
    model_spi = gc9a01_model_spi(&model);
    return gc9a01_init(&dev, &model_spi);
}

/* ====================================================================
 * 1. Init sequence -- sleep / display state
 * ==================================================================== */

TEST(test_init_exits_sleep)
{
    setup();
    ASSERT_TRUE(gc9a01_model_is_awake(&model));
}

TEST(test_init_display_on)
{
    setup();
    ASSERT_TRUE(gc9a01_model_is_display_on(&model));
}

TEST(test_init_no_sequence_violations)
{
    setup();
    ASSERT_EQ(model.seq_violations, 0);
}

/* ====================================================================
 * 2. Init sequence -- COLMOD / MADCTL
 * ==================================================================== */

TEST(test_init_sets_rgb565)
{
    setup();
    ASSERT_EQ(model.colmod, GC9A01_COLMOD_RGB565);
}

TEST(test_init_sets_madctl_rot0)
{
    setup();
    ASSERT_EQ(model.madctl, 0x00);
}

TEST(test_init_inversion_on)
{
    setup();
    ASSERT_TRUE(model.inversion);
}

/* ====================================================================
 * 3. Hardware reset
 * ==================================================================== */

TEST(test_init_triggers_hw_reset)
{
    setup();
    ASSERT_TRUE(model.rst_toggle_count >= 1);
}

TEST(test_init_delays_sufficient)
{
    setup();
    /* HW reset(10) + release(120) + SW reset(120) + sleep out(120) + display on(20) = 390 */
    ASSERT_TRUE(model.delay_total_ms >= 390);
}

/* ====================================================================
 * 4. Window addressing
 * ==================================================================== */

TEST(test_set_window_updates_model)
{
    setup();
    gc9a01_set_window(&dev, 10, 20, 50, 60);
    ASSERT_EQ(model.col_start, 10);
    ASSERT_EQ(model.col_end,   50);
    ASSERT_EQ(model.row_start, 20);
    ASSERT_EQ(model.row_end,   60);
}

TEST(test_set_window_full_screen)
{
    setup();
    gc9a01_set_window(&dev, 0, 0, 239, 239);
    ASSERT_EQ(model.col_start, 0);
    ASSERT_EQ(model.col_end,   239);
    ASSERT_EQ(model.row_start, 0);
    ASSERT_EQ(model.row_end,   239);
}

/* ====================================================================
 * 5. Pixel data
 * ==================================================================== */

TEST(test_write_pixels_model)
{
    setup();
    gc9a01_set_window(&dev, 0, 0, 1, 0);
    uint16_t px[2] = { 0xF800, 0x001F };
    gc9a01_write_pixels(&dev, px, 2);
    /* pixel_buf should contain big-endian bytes */
    ASSERT_EQ(model.pixel_buf[0], 0xF8);
    ASSERT_EQ(model.pixel_buf[1], 0x00);
    ASSERT_EQ(model.pixel_buf[2], 0x00);
    ASSERT_EQ(model.pixel_buf[3], 0x1F);
    ASSERT_EQ(model.pixel_buf_pos, 4);
}

/* ====================================================================
 * 6. Rotation via MADCTL
 * ==================================================================== */

TEST(test_rotation_1_madctl_model)
{
    setup();
    gc9a01_set_rotation(&dev, 1);
    /* MV | MY = 0x20 | 0x80 = 0xA0 */
    ASSERT_EQ(model.madctl, 0xA0);
}

TEST(test_rotation_2_madctl_model)
{
    setup();
    gc9a01_set_rotation(&dev, 2);
    /* MX | MY = 0x40 | 0x80 = 0xC0 */
    ASSERT_EQ(model.madctl, 0xC0);
}

TEST(test_rotation_3_madctl_model)
{
    setup();
    gc9a01_set_rotation(&dev, 3);
    /* MV | MX = 0x20 | 0x40 = 0x60 */
    ASSERT_EQ(model.madctl, 0x60);
}

/* ====================================================================
 * 7. Sleep / display control
 * ==================================================================== */

TEST(test_sleep_enter_model)
{
    setup();
    gc9a01_sleep(&dev, true);
    ASSERT_FALSE(gc9a01_model_is_awake(&model));
}

TEST(test_sleep_exit_model)
{
    setup();
    gc9a01_sleep(&dev, true);
    gc9a01_sleep(&dev, false);
    ASSERT_TRUE(gc9a01_model_is_awake(&model));
    ASSERT_EQ(model.seq_violations, 0);  /* 120 ms between SLPIN and SLPOUT */
}

TEST(test_display_off_model)
{
    setup();
    gc9a01_display(&dev, false);
    ASSERT_FALSE(gc9a01_model_is_display_on(&model));
}

TEST(test_display_toggle_model)
{
    setup();
    gc9a01_display(&dev, false);
    gc9a01_display(&dev, true);
    ASSERT_TRUE(gc9a01_model_is_display_on(&model));
}

/* ====================================================================
 * 8. Reset defaults and sleep timing, from the manual
 * ==================================================================== */

static void send_cmd(uint8_t cmd)
{
    gc9a01_model_dc_cmd(&model);
    gc9a01_model_xfer(&model, cmd);
}

static void send_arg(uint8_t cmd, uint8_t arg)
{
    send_cmd(cmd);
    gc9a01_model_dc_data(&model);
    gc9a01_model_xfer(&model, arg);
}

/* The Default Value tables of 6.2.3, 6.2.7, 6.2.9, 6.2.11 and 6.2.12 (p101
 * to p114), MADCTL (p128) and COLMOD (p135) */
TEST(test_model_power_on_defaults)
{
    gc9a01_model_init(&model);
    ASSERT_TRUE(model.sleep);
    ASSERT_FALSE(model.display_on);
    ASSERT_FALSE(model.inversion);
    ASSERT_EQ_HEX(model.madctl, 0x00);
    ASSERT_EQ_HEX(model.colmod, 0x66);
    ASSERT_EQ_HEX(model.col_start, 0x0000);
    ASSERT_EQ_HEX(model.col_end, 0x013F);
    ASSERT_EQ_HEX(model.row_start, 0x0000);
    ASSERT_EQ_HEX(model.row_end, 0x00EF);
}

TEST(test_model_hw_reset_restores_the_defaults)
{
    setup();
    gc9a01_set_rotation(&dev, 1);
    gc9a01_set_window(&dev, 10, 20, 50, 60);

    gc9a01_model_rst_low(&model);
    gc9a01_model_rst_high(&model);
    ASSERT_TRUE(model.sleep);
    ASSERT_FALSE(model.display_on);
    ASSERT_FALSE(model.inversion);
    ASSERT_EQ_HEX(model.madctl, 0x00);
    ASSERT_EQ_HEX(model.colmod, 0x66);
    ASSERT_EQ_HEX(model.col_start, 0x0000);
    ASSERT_EQ_HEX(model.col_end, 0x00EF);
    ASSERT_EQ_HEX(model.row_start, 0x0000);
    ASSERT_EQ_HEX(model.row_end, 0x00EF);
}

/* MADCTL "SW Reset No change" (p128), COLMOD "SW Reset No Change" (p135),
 * and the others go back: Sleep IN, Display OFF, Inversion OFF */
TEST(test_model_sw_reset_keeps_madctl_and_colmod)
{
    setup();
    gc9a01_set_rotation(&dev, 2);
    ASSERT_EQ_HEX(model.madctl, 0xC0);

    send_cmd(0x01);
    ASSERT_EQ_HEX(model.madctl, 0xC0);
    ASSERT_EQ_HEX(model.colmod, GC9A01_COLMOD_RGB565);
    ASSERT_TRUE(model.sleep);
    ASSERT_FALSE(model.display_on);
    ASSERT_FALSE(model.inversion);
}

/* CASET and RASET under SW Reset: "If MADCTL's B5 = 0: EC [15:0]=013Fh", "If
 * MADCTL's B5 = 1: EC [15:0]=00EFh", p112; EP the other way round, p114 */
TEST(test_model_sw_reset_window_follows_madctl_b5)
{
    gc9a01_model_init(&model);
    send_arg(0x36, 0x00);
    send_cmd(0x01);
    ASSERT_EQ_HEX(model.col_end, 0x013F);
    ASSERT_EQ_HEX(model.row_end, 0x00EF);

    send_arg(0x36, 0x20);
    send_cmd(0x01);
    ASSERT_EQ_HEX(model.col_end, 0x00EF);
    ASSERT_EQ_HEX(model.row_end, 0x013F);
}

/* "It will be necessary to wait 5msec before sending next command", p103 */
TEST(test_model_wants_5ms_after_slpout)
{
    gc9a01_model_init(&model);
    send_cmd(0x11);
    gc9a01_model_delay_ms(&model, 4);
    send_cmd(0x28);
    ASSERT_EQ(model.seq_violations, 1);

    gc9a01_model_init(&model);
    send_cmd(0x11);
    gc9a01_model_delay_ms(&model, 5);
    send_cmd(0x28);
    ASSERT_EQ(model.seq_violations, 0);
}

/* The same 5 ms after SLPIN, p101 */
TEST(test_model_wants_5ms_after_slpin)
{
    gc9a01_model_init(&model);
    send_cmd(0x11);
    gc9a01_model_delay_ms(&model, 120);
    send_cmd(0x10);
    gc9a01_model_delay_ms(&model, 4);
    send_cmd(0x28);
    ASSERT_EQ(model.seq_violations, 1);
}

/* "wait 120msec after sending Sleep Out command (when in Sleep In Mode)
 * before Sleep In command can be sent", p101 */
TEST(test_model_wants_120ms_between_slpout_and_slpin)
{
    gc9a01_model_init(&model);
    send_cmd(0x11);
    gc9a01_model_delay_ms(&model, 119);
    send_cmd(0x10);
    ASSERT_EQ(model.seq_violations, 1);

    gc9a01_model_init(&model);
    send_cmd(0x11);
    gc9a01_model_delay_ms(&model, 120);
    send_cmd(0x10);
    ASSERT_EQ(model.seq_violations, 0);
}

/* "This command has no effect when module is already in sleep out mode", so
 * a repeat starts no clock, p103 */
TEST(test_model_repeated_slpout_has_no_effect)
{
    gc9a01_model_init(&model);
    send_cmd(0x11);
    gc9a01_model_delay_ms(&model, 120);
    send_cmd(0x11);
    send_cmd(0x10);
    ASSERT_EQ(model.seq_violations, 0);
}

TEST(test_driver_sleep_cycle_keeps_the_timing)
{
    setup();
    gc9a01_sleep(&dev, true);
    gc9a01_sleep(&dev, false);
    gc9a01_sleep(&dev, true);
    gc9a01_sleep(&dev, false);
    gc9a01_display(&dev, true);
    ASSERT_EQ(model.seq_violations, 0);
}

/* ========== runner ========== */

TEST_MAIN("gc9a01_hw_model")
    /* init */
    RUN(test_init_exits_sleep);
    RUN(test_init_display_on);
    RUN(test_init_no_sequence_violations);
    RUN(test_init_sets_rgb565);
    RUN(test_init_sets_madctl_rot0);
    RUN(test_init_inversion_on);
    RUN(test_init_triggers_hw_reset);
    RUN(test_init_delays_sufficient);
    /* window */
    RUN(test_set_window_updates_model);
    RUN(test_set_window_full_screen);
    /* pixel data */
    RUN(test_write_pixels_model);
    /* rotation */
    RUN(test_rotation_1_madctl_model);
    RUN(test_rotation_2_madctl_model);
    RUN(test_rotation_3_madctl_model);
    /* sleep / display */
    RUN(test_sleep_enter_model);
    RUN(test_sleep_exit_model);
    RUN(test_display_off_model);
    RUN(test_display_toggle_model);
    /* reset defaults and sleep timing */
    RUN(test_model_power_on_defaults);
    RUN(test_model_hw_reset_restores_the_defaults);
    RUN(test_model_sw_reset_keeps_madctl_and_colmod);
    RUN(test_model_sw_reset_window_follows_madctl_b5);
    RUN(test_model_wants_5ms_after_slpout);
    RUN(test_model_wants_5ms_after_slpin);
    RUN(test_model_wants_120ms_between_slpout_and_slpin);
    RUN(test_model_repeated_slpout_has_no_effect);
    RUN(test_driver_sleep_cycle_keeps_the_timing);
TEST_MAIN_END()
