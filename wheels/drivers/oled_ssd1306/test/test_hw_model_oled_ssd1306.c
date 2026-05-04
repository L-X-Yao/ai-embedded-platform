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
    ASSERT_EQ(model.gddram_pos, 1024);
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
    model.gddram_pos = 0;
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
TEST_MAIN_END()
