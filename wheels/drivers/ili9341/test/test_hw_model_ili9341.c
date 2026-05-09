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
    ASSERT_TRUE(data_bytes >= 32);
}

TEST(test_hw_draw_pixel_data)
{
    hw_model_reset();
    ili9341_init(&dev, &hw_spi);

    /* Reset data log to capture only pixel data */
    hw_data_log_idx = 0;
    ili9341_draw_pixel(&dev, 0, 0, 0xABCD);

    /* Should have CASET(4)+RASET(4)+RAMWR(2) = 10 data bytes */
    ASSERT_TRUE(hw_data_log_idx >= 10);
    /* Last 2 bytes should be the pixel color */
    ASSERT_EQ(hw_data_log[hw_data_log_idx - 2], 0xAB);
    ASSERT_EQ(hw_data_log[hw_data_log_idx - 1], 0xCD);
}

TEST_MAIN("ili9341_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_init_sends_correct_sequence);
    RUN(test_hw_rotation_madctl);
    RUN(test_hw_fill_rect_pixel_count);
    RUN(test_hw_draw_pixel_data);
TEST_MAIN_END()
