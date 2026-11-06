// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ili9341.h"
#include "ili9341.h"

static const ili9341_spi_t hw_spi = {
    .cs_low   = hw_model_cs_low,
    .cs_high  = hw_model_cs_high,
    .xfer     = hw_model_xfer,
    .dc_set   = hw_model_dc_set,
    .rst_set  = hw_model_rst_set,
    .delay_ms = hw_model_delay_ms,
    .ctx      = NULL,
};

static ili9341_t dev;

/* One command byte, as the host sends it */
static void send_cmd(uint8_t cmd)
{
    hw_spi.dc_set(NULL, false);
    hw_spi.cs_low(NULL);
    hw_spi.xfer(NULL, cmd);
    hw_spi.cs_high(NULL);
}

static void pulse_reset(void)
{
    hw_spi.rst_set(NULL, false);
    hw_spi.delay_ms(NULL, 1);
    hw_spi.rst_set(NULL, true);
}

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    ASSERT_EQ(ili9341_init(&dev, &hw_spi), ILI9341_OK);
}

TEST(test_hw_init_sends_correct_sequence)
{
    hw_model_reset();
    ili9341_init(&dev, &hw_spi);

    /* Verify SWRESET was sent */
    ASSERT_TRUE(hw_model_cmd_sent(0x01));
    /* Verify SLPOUT was sent */
    ASSERT_TRUE(hw_model_cmd_sent(0x11));
    /* Verify COLMOD was sent */
    ASSERT_TRUE(hw_model_cmd_sent(0x3A));
    /* Verify MADCTL was sent */
    ASSERT_TRUE(hw_model_cmd_sent(0x36));
    /* Verify DISPON was sent */
    ASSERT_TRUE(hw_model_cmd_sent(0x29));
    /* Verify RGB565 format was set */
    ASSERT_EQ(hw_last_colmod, 0x55);
}

TEST(test_hw_rotation_madctl)
{
    hw_model_reset();
    ili9341_init(&dev, &hw_spi);

    ili9341_set_rotation(&dev, ILI9341_ROT_90);
    /* MADCTL for 90 should include MV bit */
    ASSERT_TRUE(hw_last_madctl & 0x20);  /* MV */
}

TEST(test_hw_fill_rect_pixel_count)
{
    hw_model_reset();
    ili9341_init(&dev, &hw_spi);

    int pre_data = hw_data_log_idx;
    ili9341_fill_rect(&dev, 0, 0, 4, 4, 0xF800);
    int data_bytes = hw_data_log_idx - pre_data;
    /* 4x4 = 16 pixels x 2 bytes = 32 data bytes + set_window data (8 bytes) */
    ASSERT_EQ(data_bytes, 40);
}

TEST(test_hw_draw_pixel_data)
{
    hw_model_reset();
    ili9341_init(&dev, &hw_spi);

    /* Reset data log to capture only pixel data */
    hw_data_log_idx = 0;
    ili9341_draw_pixel(&dev, 0, 0, 0xABCD);

    /* Should have CASET(4)+RASET(4)+RAMWR(2) = 10 data bytes */
    ASSERT_EQ(hw_data_log_idx, 10);
    /* Last 2 bytes should be the pixel color */
    ASSERT_EQ(hw_data_log[hw_data_log_idx - 2], 0xAB);
    ASSERT_EQ(hw_data_log[hw_data_log_idx - 1], 0xCD);
}

/* --- Chip select --- */

/* ILI9341 V1.11 p.33: "The serial interface is initialized when CSX is high
 * status. In this state, SCL clock pulse and SDA data are no effect." */
TEST(test_hw_model_ignores_bytes_with_csx_high)
{
    hw_model_reset();

    hw_spi.dc_set(NULL, false);
    hw_spi.xfer(NULL, 0x29);
    ASSERT_EQ(hw_cmd_log_idx, 0);
    ASSERT_FALSE(hw_display_on);
    ASSERT_EQ(hw_dropped_bytes, 1);

    send_cmd(0x29);
    ASSERT_TRUE(hw_model_cmd_sent(0x29));
    ASSERT_TRUE(hw_display_on);
}

TEST(test_hw_driver_asserts_csx_for_every_byte)
{
    hw_model_reset();
    ASSERT_EQ(ili9341_init(&dev, &hw_spi), ILI9341_OK);
    ASSERT_EQ(ili9341_set_rotation(&dev, ILI9341_ROT_90), ILI9341_OK);
    ASSERT_EQ(ili9341_fill_rect(&dev, 1, 2, 3, 4, 0x1234), ILI9341_OK);
    ASSERT_EQ(ili9341_draw_pixel(&dev, 5, 6, 0x5678), ILI9341_OK);

    ASSERT_EQ(hw_dropped_bytes, 0);
    ASSERT_EQ(hw_cs_state, 1);
}

/* --- The waits --- */

/* p.230 note 7: "It is necessary to wait 5msec after releasing RESX before
 * sending commands. Also Sleep Out command cannot be sent for 120msec."
 * p.90: after SWRESET "wait 5msec before sending new command", and p.101:
 * after SLPOUT "wait 5msec before sending next command". */
TEST(test_hw_init_keeps_every_wait)
{
    hw_model_reset();
    ASSERT_EQ(ili9341_init(&dev, &hw_spi), ILI9341_OK);

    ASSERT_EQ(hw_early_cmds, 0);
    ASSERT_EQ(hw_short_resets, 0);
    ASSERT_EQ(hw_reset_count, 1);
}

TEST(test_hw_model_counts_a_command_inside_the_reset_wait)
{
    hw_model_reset();
    pulse_reset();

    hw_spi.delay_ms(NULL, 4);
    send_cmd(0x29);
    ASSERT_EQ(hw_early_cmds, 1);

    hw_spi.delay_ms(NULL, 1);
    send_cmd(0x29);
    ASSERT_EQ(hw_early_cmds, 1);
}

TEST(test_hw_model_counts_a_command_during_reset)
{
    hw_model_reset();
    hw_spi.rst_set(NULL, false);
    hw_spi.delay_ms(NULL, 1);

    send_cmd(0x29);

    ASSERT_EQ(hw_early_cmds, 1);
}

TEST(test_hw_model_holds_slpout_for_120_ms_after_reset)
{
    hw_model_reset();
    pulse_reset();

    hw_spi.delay_ms(NULL, 119);
    send_cmd(0x11);
    ASSERT_EQ(hw_early_cmds, 1);

    pulse_reset();
    hw_spi.delay_ms(NULL, 120);
    send_cmd(0x11);
    ASSERT_EQ(hw_early_cmds, 1);
}

TEST(test_hw_model_counts_a_command_inside_the_swreset_wait)
{
    hw_model_reset();
    send_cmd(0x01);

    hw_spi.delay_ms(NULL, 4);
    send_cmd(0x29);
    ASSERT_EQ(hw_early_cmds, 1);

    hw_spi.delay_ms(NULL, 1);
    send_cmd(0x29);
    ASSERT_EQ(hw_early_cmds, 1);
}

/* p.90: "If Software Reset is applied during Sleep Out mode, it will be
 * necessary to wait 120msec before sending Sleep out command." */
TEST(test_hw_model_holds_slpout_after_swreset_in_sleep_out)
{
    hw_model_reset();
    send_cmd(0x11);
    hw_spi.delay_ms(NULL, 120);
    send_cmd(0x01);

    hw_spi.delay_ms(NULL, 119);
    send_cmd(0x11);
    ASSERT_EQ(hw_early_cmds, 1);
}

/* p.90: "Software Reset Command cannot be sent during Sleep Out sequence." and
 * p.101: "It takes 120msec to become Sleep Out mode" */
TEST(test_hw_model_keeps_swreset_out_of_the_sleep_out_sequence)
{
    hw_model_reset();
    send_cmd(0x11);

    hw_spi.delay_ms(NULL, 119);
    send_cmd(0x01);

    ASSERT_EQ(hw_early_cmds, 1);
}

TEST(test_hw_model_counts_a_command_inside_the_slpout_wait)
{
    hw_model_reset();
    send_cmd(0x11);

    hw_spi.delay_ms(NULL, 4);
    send_cmd(0x29);
    ASSERT_EQ(hw_early_cmds, 1);
}

/* p.230: tRW is 10 us at least, and "Shorter than 5us: Reset Rejected". The HAL
 * counts in ms, so what shows here is a pulse with no wait at all. */
TEST(test_hw_model_rejects_a_reset_with_no_width)
{
    hw_model_reset();
    hw_spi.rst_set(NULL, false);
    hw_spi.rst_set(NULL, true);

    ASSERT_EQ(hw_short_resets, 1);
    ASSERT_EQ(hw_reset_count, 0);
}

/* --- State after a reset --- */

/* p.128 and p.134: MADCTL "8'h00h" and COLMOD 3'b110 after HW Reset, and p.101:
 * Sleep IN Mode. Display OFF after HW Reset as well (p.108). */
TEST(test_hw_reset_restores_the_defaults)
{
    hw_model_reset();
    ASSERT_EQ(ili9341_init(&dev, &hw_spi), ILI9341_OK);
    ASSERT_TRUE(hw_last_madctl != 0);

    pulse_reset();

    ASSERT_EQ(hw_last_madctl, 0x00);
    ASSERT_EQ(hw_last_colmod, 0x66);
    ASSERT_FALSE(hw_sleep_out);
    ASSERT_FALSE(hw_display_on);
}

/* p.128 and p.134: "SW Reset: No Change" for MADCTL and COLMOD, and Sleep IN
 * Mode and Display OFF all the same */
TEST(test_hw_swreset_keeps_madctl_and_colmod)
{
    hw_model_reset();
    ASSERT_EQ(ili9341_init(&dev, &hw_spi), ILI9341_OK);
    uint8_t madctl = hw_last_madctl;
    hw_spi.delay_ms(NULL, 120);

    send_cmd(0x01);

    ASSERT_EQ(hw_last_madctl, madctl);
    ASSERT_EQ(hw_last_colmod, 0x55);
    ASSERT_FALSE(hw_sleep_out);
    ASSERT_FALSE(hw_display_on);
}

TEST(test_hw_init_ends_awake_with_the_display_on)
{
    hw_model_reset();
    ASSERT_FALSE(hw_sleep_out);
    ASSERT_FALSE(hw_display_on);

    ASSERT_EQ(ili9341_init(&dev, &hw_spi), ILI9341_OK);

    ASSERT_TRUE(hw_sleep_out);
    ASSERT_TRUE(hw_display_on);
}

/* --- MADCTL --- */

/* p.127: MY, MX and MV are D7, D6 and D5. Which rotation is 0 depends on the
 * module, but half a turn flips both axes and keeps the row/column exchange,
 * and only the quarter turns exchange. */
TEST(test_hw_rotation_pairs_flip_both_axes)
{
    static const ili9341_rotation_t rot[4] = {
        ILI9341_ROT_0, ILI9341_ROT_90, ILI9341_ROT_180, ILI9341_ROT_270,
    };
    uint8_t m[4];

    hw_model_reset();
    ASSERT_EQ(ili9341_init(&dev, &hw_spi), ILI9341_OK);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(ili9341_set_rotation(&dev, rot[i]), ILI9341_OK);
        m[i] = hw_last_madctl;
    }

    ASSERT_EQ(m[0] ^ m[2], 0xC0);
    ASSERT_EQ(m[1] ^ m[3], 0xC0);
    ASSERT_EQ(m[0] & 0x20U, 0x00);
    ASSERT_EQ(m[1] & 0x20U, 0x20);
}

TEST_MAIN("ili9341_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_init_sends_correct_sequence);
    RUN(test_hw_rotation_madctl);
    RUN(test_hw_fill_rect_pixel_count);
    RUN(test_hw_draw_pixel_data);
    RUN(test_hw_model_ignores_bytes_with_csx_high);
    RUN(test_hw_driver_asserts_csx_for_every_byte);
    RUN(test_hw_init_keeps_every_wait);
    RUN(test_hw_model_counts_a_command_inside_the_reset_wait);
    RUN(test_hw_model_counts_a_command_during_reset);
    RUN(test_hw_model_holds_slpout_for_120_ms_after_reset);
    RUN(test_hw_model_counts_a_command_inside_the_swreset_wait);
    RUN(test_hw_model_holds_slpout_after_swreset_in_sleep_out);
    RUN(test_hw_model_keeps_swreset_out_of_the_sleep_out_sequence);
    RUN(test_hw_model_counts_a_command_inside_the_slpout_wait);
    RUN(test_hw_model_rejects_a_reset_with_no_width);
    RUN(test_hw_reset_restores_the_defaults);
    RUN(test_hw_swreset_keeps_madctl_and_colmod);
    RUN(test_hw_init_ends_awake_with_the_display_on);
    RUN(test_hw_rotation_pairs_flip_both_axes);
TEST_MAIN_END()
