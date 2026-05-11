// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_apa102.h"
#include "apa102.h"

static const apa102_spi_t hw_spi = {
    .cs_low  = hw_model_cs_low,
    .cs_high = hw_model_cs_high,
    .xfer    = hw_model_xfer,
    .ctx     = NULL,
};

static apa102_t dev;

TEST(test_hw_init_ok)
{
    hw_model_reset();
    ASSERT_EQ(apa102_init(&dev, &hw_spi, 8), APA102_OK);
}

TEST(test_hw_show_valid_frame)
{
    hw_model_reset();
    apa102_init(&dev, &hw_spi, 4);

    apa102_pixel_t buf[4];
    apa102_clear(buf, 4);
    apa102_set_pixel(buf, 4, 0, 255, 0, 0, 31);
    apa102_set_pixel(buf, 4, 1, 0, 255, 0, 20);
    apa102_set_pixel(buf, 4, 2, 0, 0, 255, 10);
    apa102_set_pixel(buf, 4, 3, 128, 128, 128, 5);

    apa102_show(&dev, buf);

    ASSERT_TRUE(hw_model_validate_frame(4));
}

TEST(test_hw_show_single_led)
{
    hw_model_reset();
    apa102_init(&dev, &hw_spi, 1);

    apa102_pixel_t buf[1];
    apa102_clear(buf, 1);
    apa102_set_pixel(buf, 1, 0, 0xFF, 0x80, 0x40, 16);

    apa102_show(&dev, buf);

    ASSERT_TRUE(hw_model_validate_frame(1));

    /* Verify specific LED data: after 4-byte start */
    ASSERT_EQ_HEX(hw_xfer_log[4], 0xE0 | 16); /* brightness */
    ASSERT_EQ_HEX(hw_xfer_log[5], 0x40);       /* blue */
    ASSERT_EQ_HEX(hw_xfer_log[6], 0x80);       /* green */
    ASSERT_EQ_HEX(hw_xfer_log[7], 0xFF);       /* red */
}

TEST(test_hw_cs_deasserted_after_show)
{
    hw_model_reset();
    apa102_init(&dev, &hw_spi, 1);

    apa102_pixel_t buf[1] = {{0, 0, 0, 0}};
    apa102_show(&dev, buf);

    ASSERT_EQ(hw_cs_state, 1);
}

TEST_MAIN("apa102_hw_model")
    RUN(test_hw_init_ok);
    RUN(test_hw_show_valid_frame);
    RUN(test_hw_show_single_led);
    RUN(test_hw_cs_deasserted_after_show);
TEST_MAIN_END()
