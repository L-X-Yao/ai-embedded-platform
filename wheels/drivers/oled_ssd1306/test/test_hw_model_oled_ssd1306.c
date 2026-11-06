// SPDX-License-Identifier: MIT
/*
 * test_hw_model_oled_ssd1306.c — tests using SSD1306 command model
 *
 * Validates the SSD1306 driver against a command-processor model
 * that tracks internal display state (on/off, contrast, addressing
 * mode, charge pump, etc.) from the datasheet.
 */

#include "test_harness.h"
#include "hw_model_oled_ssd1306.h"
#include "../oled_ssd1306.h"

static oled_ssd1306_model_t model;

static const oled_ssd1306_i2c_t model_i2c = {
    .write_cmd  = oled_model_write_cmd,
    .write_data = oled_model_write_data,
    .ctx        = &model,
};

static oled_ssd1306_t dev;
static uint8_t framebuf[OLED_SSD1306_BUF_SIZE(128, 64)];
static const oled_ssd1306_cfg_t default_cfg = {
    .addr   = OLED_SSD1306_ADDR_A,
    .width  = 128,
    .height = 64,
};

static void setup(void)
{
    oled_ssd1306_model_init(&model);
    oled_ssd1306_init(&dev, &model_i2c, &default_cfg, framebuf);
}

/* Where a RAM cell sits: 128 columns per page */
static size_t ram(uint8_t page, uint8_t col)
{
    return (size_t)page * 128u + col;
}

static void send_cmds(const uint8_t *cmds, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        oled_model_write_cmd(&model, OLED_SSD1306_ADDR_A, cmds[i]);
    }
}

static void send_data(const uint8_t *data, uint16_t n)
{
    oled_model_write_data(&model, OLED_SSD1306_ADDR_A, data, n);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ends_with_display_on)
{
    setup();
    /* The init sequence should turn the display on at the end (0xAF) */
    ASSERT_TRUE(model.display_on);
}

TEST(test_init_enables_charge_pump)
{
    setup();
    /* Charge pump must be enabled (0x8D, 0x14) for internal Vcc OLEDs */
    ASSERT_TRUE(model.charge_pump);
}

TEST(test_init_sets_horizontal_addressing)
{
    setup();
    /* Driver sets horizontal addressing mode (0x20, 0x00) for page flush */
    ASSERT_EQ(model.addressing_mode, 0x00);
}

TEST(test_init_sets_contrast)
{
    setup();
    /* Init sets contrast to 0xCF */
    ASSERT_EQ(model.contrast, 0xCF);
}

TEST(test_init_sets_mux_ratio)
{
    setup();
    /* 64-line display: MUX ratio = 63 (0xA8, 0x3F) */
    ASSERT_EQ(model.mux_ratio, 63);
}

TEST(test_init_sets_seg_remap_and_com_scan)
{
    setup();
    /* Standard orientation: seg remap (0xA1) + COM scan remapped (0xC8) */
    ASSERT_TRUE(model.seg_remap);
    ASSERT_TRUE(model.com_scan_dir);
}

/* --- display on/off ---------------------------------------------------- */

TEST(test_display_off_clears_state)
{
    setup();
    ASSERT_TRUE(model.display_on);
    oled_ssd1306_display_off(&dev);
    ASSERT_FALSE(model.display_on);
}

TEST(test_display_on_sets_state)
{
    setup();
    oled_ssd1306_display_off(&dev);
    ASSERT_FALSE(model.display_on);
    oled_ssd1306_display_on(&dev);
    ASSERT_TRUE(model.display_on);
}

/* --- flush tests ------------------------------------------------------- */

TEST(test_flush_sends_column_page_window)
{
    setup();
    /* Reset model counters after init */
    int cmds_before = model.cmd_count;
    oled_ssd1306_flush(&dev);
    /* Flush should issue additional commands (col/page address) + data */
    ASSERT_TRUE(model.cmd_count > cmds_before);
    /* Should have set col_start=0, col_end=127 */
    ASSERT_EQ(model.col_start, 0);
    ASSERT_EQ(model.col_end, 127);
    /* page_start=0, page_end=7 (64/8 - 1) */
    ASSERT_EQ(model.page_start, 0);
    ASSERT_EQ(model.page_end, 7);
}

TEST(test_flush_sends_framebuffer_data)
{
    setup();
    int data_before = model.data_count;
    oled_ssd1306_flush(&dev);
    ASSERT_EQ(model.data_count, data_before + 1);
    /* GDDRAM should have received 1024 bytes */
    ASSERT_EQ(model.data_bytes, 1024);
}

TEST(test_flush_pixel_appears_in_gddram)
{
    setup();
    /* Set pixel at (0, 0) — maps to byte 0, bit 0 */
    oled_ssd1306_set_pixel(&dev, 0, 0, true);
    oled_ssd1306_flush(&dev);
    /* Byte 0 of GDDRAM should have bit 0 set */
    ASSERT_EQ(model.gddram[0] & 0x01, 0x01);
}

TEST(test_flush_clear_pixel_not_in_gddram)
{
    setup();
    oled_ssd1306_set_pixel(&dev, 0, 0, true);
    oled_ssd1306_flush(&dev);
    ASSERT_EQ(model.gddram[0] & 0x01, 0x01);

    /* Clear and flush again */
    oled_ssd1306_clear(&dev);
    oled_ssd1306_flush(&dev);
    ASSERT_EQ(model.gddram[0], 0x00);
}

/* --- 32-line display variant ------------------------------------------- */

TEST(test_init_32line_com_pin)
{
    oled_ssd1306_model_init(&model);
    oled_ssd1306_cfg_t cfg32 = { .addr = OLED_SSD1306_ADDR_A, .width = 128, .height = 32 };
    uint8_t fb32[OLED_SSD1306_BUF_SIZE(128, 32)];
    oled_ssd1306_t dev32;
    oled_ssd1306_init(&dev32, &model_i2c, &cfg32, fb32);
    /* 32-line: COM pin config = 0x02 (sequential) */
    ASSERT_EQ(model.com_pin_cfg, 0x02);
    /* MUX ratio = 31 */
    ASSERT_EQ(model.mux_ratio, 31);
}

/* --- command count tracking -------------------------------------------- */

TEST(test_init_sends_many_commands)
{
    oled_ssd1306_model_init(&model);
    oled_ssd1306_init(&dev, &model_i2c, &default_cfg, framebuf);
    /* SSD1306 init sequence has ~24 command bytes */
    ASSERT_TRUE(model.cmd_count >= 20);
}

/* --- GDDRAM addressing ------------------------------------------------- */

/* Rev 1.1 p.30 and p.31: "Page Addressing Mode (RESET)", column window 0 to
 * 127 and page window 0 to 7 */
TEST(test_model_resets_to_page_mode_and_the_whole_window)
{
    oled_ssd1306_model_init(&model);
    ASSERT_EQ(model.addressing_mode, 0x02);
    ASSERT_EQ(model.col_start, 0);
    ASSERT_EQ(model.col_end, 127);
    ASSERT_EQ(model.page_start, 0);
    ASSERT_EQ(model.page_end, 7);
}

/* p.34: "if the page address is set to B2h, lower column address is 03h and
 * upper column address is 10h, then that means the starting column is SEG3 of
 * PAGE2." */
TEST(test_model_page_mode_pointer_example)
{
    static const uint8_t cmds[] = {0xB2, 0x03, 0x10};
    static const uint8_t data[] = {0x5A};
    oled_ssd1306_model_init(&model);

    send_cmds(cmds, sizeof(cmds));
    send_data(data, 1);

    ASSERT_EQ(model.gddram[ram(2, 3)], 0x5A);
}

/* p.34: in page mode, at the column end "the column address pointer is reset
 * to column start address and page address pointer is not changed" */
TEST(test_model_page_mode_stays_on_the_page)
{
    static const uint8_t cmds[] = {0xB2, 0x00, 0x10};   /* page 2, column 0 */
    uint8_t data[129];
    memset(data, 0x11, sizeof(data));
    data[127] = 0x22;
    data[128] = 0x33;
    oled_ssd1306_model_init(&model);

    send_cmds(cmds, sizeof(cmds));
    send_data(data, 129);

    ASSERT_EQ(model.gddram[ram(2, 127)], 0x22);
    ASSERT_EQ(model.gddram[ram(2, 0)], 0x33);
    ASSERT_EQ(model.gddram[ram(3, 0)], 0x00);
}

/* p.35: in horizontal mode, at the column end "the column address pointer is
 * reset to column start address and page address pointer is increased by 1",
 * and "When both column and page address pointers reach the end address, the
 * pointers are reset to column start address and page start address" */
TEST(test_model_horizontal_mode_follows_the_window)
{
    static const uint8_t cmds[] = {0x20, 0x00, 0x21, 16, 18, 0x22, 1, 2};
    static const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7};
    oled_ssd1306_model_init(&model);

    send_cmds(cmds, sizeof(cmds));
    send_data(data, 7);

    ASSERT_EQ(model.gddram[ram(1, 16)], 7);    /* wrapped back over the 1 */
    ASSERT_EQ(model.gddram[ram(1, 17)], 2);
    ASSERT_EQ(model.gddram[ram(1, 18)], 3);
    ASSERT_EQ(model.gddram[ram(2, 16)], 4);
    ASSERT_EQ(model.gddram[ram(2, 18)], 6);
    ASSERT_EQ(model.gddram[ram(1, 19)], 0);    /* outside the window */
}

/* p.35: in vertical mode, "If the page address pointer reaches the page end
 * address, the page address pointer is reset to page start address and column
 * address pointer is increased by 1." */
TEST(test_model_vertical_mode_follows_the_window)
{
    static const uint8_t cmds[] = {0x20, 0x01, 0x21, 16, 17, 0x22, 1, 2};
    static const uint8_t data[] = {1, 2, 3, 4, 5};
    oled_ssd1306_model_init(&model);

    send_cmds(cmds, sizeof(cmds));
    send_data(data, 5);

    ASSERT_EQ(model.gddram[ram(1, 16)], 5);    /* wrapped back over the 1 */
    ASSERT_EQ(model.gddram[ram(2, 16)], 2);
    ASSERT_EQ(model.gddram[ram(1, 17)], 3);
    ASSERT_EQ(model.gddram[ram(2, 17)], 4);
}

/* p.35: 21h "also sets the column address pointer to column start address".
 * p.36: 22h "also sets the page address pointer to page start address". */
TEST(test_model_window_commands_set_the_pointers)
{
    static const uint8_t window[]  = {0x20, 0x00, 0x21, 4, 5, 0x22, 3, 4};
    static const uint8_t restart[] = {0x21, 5, 5, 0x22, 4, 4};
    static const uint8_t data[]    = {0xAA, 0xBB, 0xCC};
    oled_ssd1306_model_init(&model);

    send_cmds(window, sizeof(window));
    send_data(data, 1);
    send_cmds(restart, sizeof(restart));
    send_data(&data[1], 1);

    ASSERT_EQ(model.gddram[ram(3, 4)], 0xAA);
    ASSERT_EQ(model.gddram[ram(4, 5)], 0xBB);
}

/* p.30: "A[1:0] = 11b, Invalid". p.31: "A[5:0] from 0 to 14 are invalid entry." */
TEST(test_model_keeps_state_on_invalid_entries)
{
    static const uint8_t bad_mode[] = {0x20, 0x03};
    static const uint8_t bad_mux[]  = {0xA8, 0x0E};
    oled_ssd1306_model_init(&model);

    send_cmds(bad_mode, sizeof(bad_mode));
    ASSERT_EQ(model.addressing_mode, 0x02);
    send_cmds(bad_mux, sizeof(bad_mux));
    ASSERT_EQ(model.mux_ratio, 63);
    ASSERT_EQ(model.invalid_cmds, 2);
}

/* p.30 and p.31: 21h and 22h are "only for horizontal or vertical addressing
 * mode", and 00h to 1Fh and B0h to B7h "only for page addressing mode" */
TEST(test_model_counts_commands_of_the_wrong_mode)
{
    static const uint8_t window[]  = {0x21, 4, 5, 0x22, 1, 2};
    static const uint8_t to_horz[] = {0x20, 0x00};
    static const uint8_t pointer[] = {0xB2, 0x03, 0x10};
    oled_ssd1306_model_init(&model);

    send_cmds(window, sizeof(window));          /* in page mode */
    ASSERT_EQ(model.invalid_cmds, 2);
    ASSERT_EQ(model.col_start, 0);
    ASSERT_EQ(model.page_end, 7);

    send_cmds(to_horz, sizeof(to_horz));
    send_cmds(pointer, sizeof(pointer));
    ASSERT_EQ(model.invalid_cmds, 5);
    ASSERT_EQ(model.page_ptr, 0);
}

/* --- flush against the addressing -------------------------------------- */

/* Filling all 8 pages takes horizontal mode: in the page mode of reset, all
 * 1024 bytes would circle round page 0 */
TEST(test_flush_reaches_the_last_page)
{
    setup();
    oled_ssd1306_set_pixel(&dev, 127, 63, true);
    oled_ssd1306_flush(&dev);

    ASSERT_EQ(model.gddram[ram(7, 127)], 0x80);
}

TEST(test_flush_leaves_the_pointers_at_the_window_start)
{
    setup();
    oled_ssd1306_flush(&dev);
    ASSERT_EQ(model.col_ptr, 0);
    ASSERT_EQ(model.page_ptr, 0);
    ASSERT_EQ(model.invalid_cmds, 0);
}

/* A window narrower than the RAM: the second page starts at column 0 of page 1,
 * not 64 bytes on from the first */
TEST(test_flush_keeps_a_narrow_panel_on_its_columns)
{
    oled_ssd1306_cfg_t cfg = { .addr = OLED_SSD1306_ADDR_A, .width = 64, .height = 32 };
    uint8_t fb[OLED_SSD1306_BUF_SIZE(64, 32)];
    oled_ssd1306_t d;
    oled_ssd1306_model_init(&model);
    ASSERT_EQ(oled_ssd1306_init(&d, &model_i2c, &cfg, fb), OLED_SSD1306_OK);

    oled_ssd1306_set_pixel(&d, 5, 9, true);
    ASSERT_EQ(oled_ssd1306_flush(&d), OLED_SSD1306_OK);

    ASSERT_EQ(model.col_end, 63);
    ASSERT_EQ(model.page_end, 3);
    ASSERT_EQ(model.gddram[ram(1, 5)], 0x02);
    ASSERT_EQ(model.gddram[ram(0, 69)], 0x00);
}

/* p.31: 16MUX to 64MUX, so every height init takes has a valid entry */
TEST(test_init_sends_no_invalid_entry)
{
    for (uint8_t h = 16; h <= 64; h = (uint8_t)(h + 8U)) {
        oled_ssd1306_cfg_t cfg = { .addr = OLED_SSD1306_ADDR_A, .width = 128, .height = h };
        oled_ssd1306_model_init(&model);
        ASSERT_EQ(oled_ssd1306_init(&dev, &model_i2c, &cfg, framebuf), OLED_SSD1306_OK);
        ASSERT_EQ(model.mux_ratio, h - 1U);
        ASSERT_EQ(model.invalid_cmds, 0);
    }
}

TEST_MAIN("oled_ssd1306 hardware model")
    RUN(test_init_ends_with_display_on);
    RUN(test_init_enables_charge_pump);
    RUN(test_init_sets_horizontal_addressing);
    RUN(test_init_sets_contrast);
    RUN(test_init_sets_mux_ratio);
    RUN(test_init_sets_seg_remap_and_com_scan);
    RUN(test_display_off_clears_state);
    RUN(test_display_on_sets_state);
    RUN(test_flush_sends_column_page_window);
    RUN(test_flush_sends_framebuffer_data);
    RUN(test_flush_pixel_appears_in_gddram);
    RUN(test_flush_clear_pixel_not_in_gddram);
    RUN(test_init_32line_com_pin);
    RUN(test_init_sends_many_commands);
    RUN(test_model_resets_to_page_mode_and_the_whole_window);
    RUN(test_model_page_mode_pointer_example);
    RUN(test_model_page_mode_stays_on_the_page);
    RUN(test_model_horizontal_mode_follows_the_window);
    RUN(test_model_vertical_mode_follows_the_window);
    RUN(test_model_window_commands_set_the_pointers);
    RUN(test_model_keeps_state_on_invalid_entries);
    RUN(test_model_counts_commands_of_the_wrong_mode);
    RUN(test_flush_reaches_the_last_page);
    RUN(test_flush_leaves_the_pointers_at_the_window_start);
    RUN(test_flush_keeps_a_narrow_panel_on_its_columns);
    RUN(test_init_sends_no_invalid_entry);
TEST_MAIN_END()
