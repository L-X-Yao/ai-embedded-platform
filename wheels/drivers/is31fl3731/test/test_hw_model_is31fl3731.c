// SPDX-License-Identifier: MIT
/*
 * test_hw_model_is31fl3731.c — tests using hardware register model
 *
 * Validates IS31FL3731 driver against a datasheet-accurate register model
 * with page-based register architecture.
 */

#include "test_harness.h"
#include "hw_model_is31fl3731.h"
#include "../is31fl3731.h"

static is31fl3731_model_t model;

static const is31fl3731_i2c_t model_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model,
};

static const is31fl3731_cfg_t default_cfg = {
    .addr = IS31FL3731_ADDR_BASE,
};

static void setup(void)
{
    is31fl3731_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_hw_init_ok)
{
    setup();
    is31fl3731_t dev;
    ASSERT_EQ(is31fl3731_init(&dev, &model_i2c, &default_cfg), IS31FL3731_OK);
}

TEST(test_hw_init_exits_shutdown)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);
    ASSERT_FALSE(model.in_shutdown);
}

TEST(test_hw_init_picture_mode)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);
    /* Function page (0x0B) CONFIG register = 0x00 (picture mode) */
    ASSERT_EQ(model.pages[0x0B][0x00], 0x00);
}

/* Rev. G p.9 Note 10: "Please initialize the Frame Registers first". The model
 * starts every frame page as junk, so a byte init misses shows up here. */
TEST(test_hw_init_writes_every_frame)
{
    setup();
    is31fl3731_t dev;
    ASSERT_EQ(is31fl3731_init(&dev, &model_i2c, &default_cfg), IS31FL3731_OK);

    for (unsigned frame = 0; frame < IS31FL3731_NUM_FRAMES; frame++) {
        for (unsigned reg = 0x00; reg <= 0x11; reg++)   /* LED Control */
            ASSERT_EQ(model.pages[frame][reg], 0xFF);
        for (unsigned reg = 0x12; reg <= 0x23; reg++)   /* Blink Control */
            ASSERT_EQ(model.pages[frame][reg], 0x00);
        for (unsigned reg = 0x24; reg <= 0xB3; reg++)   /* PWM */
            ASSERT_EQ(model.pages[frame][reg], 0x00);
    }
    ASSERT_EQ(dev.current_frame, 0);
    ASSERT_FALSE(model.in_shutdown);
}

TEST(test_hw_init_enables_leds)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);
    /* Frame 0: LED control bytes should be 0xFF */
    for (int i = 0; i < 18; i++) {
        ASSERT_EQ(model.pages[0][i], 0xFF);
    }
}

/* --- pixel tests ------------------------------------------------------- */

TEST(test_hw_set_pixel_round_trip)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);

    is31fl3731_set_pixel(&dev, 0, 42, 200);
    ASSERT_EQ(model.pages[0][0x24 + 42], 200);
}

TEST(test_hw_fill_round_trip)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);

    is31fl3731_fill(&dev, 2, 100);
    for (int i = 0; i < 144; i++) {
        ASSERT_EQ(model.pages[2][0x24 + i], 100);
    }
}

TEST(test_hw_display_frame)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);

    is31fl3731_display_frame(&dev, 4);
    ASSERT_EQ(model.pages[0x0B][0x01], 4);
}

TEST(test_hw_shutdown_round_trip)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);
    ASSERT_FALSE(model.in_shutdown);

    is31fl3731_shutdown(&dev, true);
    ASSERT_TRUE(model.in_shutdown);

    is31fl3731_shutdown(&dev, false);
    ASSERT_FALSE(model.in_shutdown);
}

TEST(test_hw_led_enable_disable)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);

    /* LED 0 is enabled after init. Disable it. */
    is31fl3731_set_led_enable(&dev, 0, 0, false);
    ASSERT_EQ(model.pages[0][0x00] & 0x01, 0x00);

    /* Re-enable */
    is31fl3731_set_led_enable(&dev, 0, 0, true);
    ASSERT_EQ(model.pages[0][0x00] & 0x01, 0x01);
}

/* The model itself keeps to Rev. G p.7: a frame page read gives the stored data
 * only in software shutdown, while the Function Register reads in either state. */
TEST(test_hw_model_frame_read_rule)
{
    setup();
    is31fl3731_t dev;
    ASSERT_EQ(is31fl3731_init(&dev, &model_i2c, &default_cfg), IS31FL3731_OK);

    uint8_t val = 0;
    ASSERT_TRUE(hw_model_write_reg(&model, 0x74, 0xFD, 0x0B));
    ASSERT_TRUE(hw_model_read_regs(&model, 0x74, 0x0A, &val, 1));
    ASSERT_EQ_HEX(val, 0x01);

    ASSERT_TRUE(hw_model_write_reg(&model, 0x74, 0xFD, 0x00));
    ASSERT_TRUE(hw_model_read_regs(&model, 0x74, 0x00, &val, 1));
    ASSERT_EQ_HEX(val, HW_FRAME_READ_JUNK);
    ASSERT_EQ(model.frame_reads_running, 1);

    ASSERT_EQ(is31fl3731_shutdown(&dev, true), IS31FL3731_OK);
    ASSERT_TRUE(hw_model_write_reg(&model, 0x74, 0xFD, 0x00));
    ASSERT_TRUE(hw_model_read_regs(&model, 0x74, 0x00, &val, 1));
    ASSERT_EQ_HEX(val, 0xFF);
    ASSERT_EQ(model.frame_reads_running, 1);
}

/* Rev. G p.7: frame registers cannot be read while the chip runs, and init leaves
 * it running. LED 12 is C1-13: byte 01h, bit 4 (Table 4, Table 7). The bus must
 * carry the page select, then the new byte, and no read at all. */
TEST(test_hw_led_enable_exact_bus)
{
    setup();
    is31fl3731_t dev;
    ASSERT_EQ(is31fl3731_init(&dev, &model_i2c, &default_cfg), IS31FL3731_OK);
    model.read_count = 0;
    model.write_log_len = 0;

    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 3, 12, false), IS31FL3731_OK);

    ASSERT_EQ(model.read_count, 0);
    ASSERT_EQ(model.frame_reads_running, 0);
    ASSERT_EQ(model.write_log_len, 2);
    ASSERT_EQ_HEX(model.write_log[0].reg, 0xFD);
    ASSERT_EQ_HEX(model.write_log[0].val, 0x03);
    ASSERT_EQ_HEX(model.write_log[1].reg, 0x01);
    ASSERT_EQ_HEX(model.write_log[1].val, 0xEF);
}

/* The shadow always matches what the chip was sent */
TEST(test_hw_led_shadow_tracks_chip)
{
    setup();
    is31fl3731_t dev;
    ASSERT_EQ(is31fl3731_init(&dev, &model_i2c, &default_cfg), IS31FL3731_OK);
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 6, 143, false), IS31FL3731_OK);
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 1, 9, false), IS31FL3731_OK);

    for (unsigned frame = 0; frame < IS31FL3731_NUM_FRAMES; frame++) {
        for (unsigned i = 0; i < 18u; i++)
            ASSERT_EQ_HEX(dev.led_ctrl[frame][i], model.pages[frame][i]);
    }
    ASSERT_EQ_HEX(dev.led_ctrl[6][17], 0x7F);
    ASSERT_EQ_HEX(dev.led_ctrl[1][1], 0xFD);
}

/* One LED changes, the other seven of its byte stay as they were */
TEST(test_hw_led_enable_keeps_neighbours)
{
    setup();
    is31fl3731_t dev;
    ASSERT_EQ(is31fl3731_init(&dev, &model_i2c, &default_cfg), IS31FL3731_OK);

    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 5, 12, false), IS31FL3731_OK);
    ASSERT_EQ_HEX(model.pages[5][0x01], 0xEF);
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 5, 13, false), IS31FL3731_OK);
    ASSERT_EQ_HEX(model.pages[5][0x01], 0xCF);
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 5, 12, true), IS31FL3731_OK);
    ASSERT_EQ_HEX(model.pages[5][0x01], 0xDF);

    /* LED 143 is C9-16: byte 11h, bit 7 */
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 5, 143, false), IS31FL3731_OK);
    ASSERT_EQ_HEX(model.pages[5][0x11], 0x7F);
    ASSERT_EQ_HEX(model.pages[5][0x00], 0xFF);
}

/* Every frame has its own LED Control bytes */
TEST(test_hw_led_enable_frames_apart)
{
    setup();
    is31fl3731_t dev;
    ASSERT_EQ(is31fl3731_init(&dev, &model_i2c, &default_cfg), IS31FL3731_OK);

    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 2, 0, false), IS31FL3731_OK);
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 7, 1, false), IS31FL3731_OK);
    ASSERT_EQ_HEX(model.pages[2][0x00], 0xFE);
    ASSERT_EQ_HEX(model.pages[7][0x00], 0xFD);
    ASSERT_EQ_HEX(model.pages[0][0x00], 0xFF);
}

TEST(test_hw_frame_isolation)
{
    setup();
    is31fl3731_t dev;
    is31fl3731_init(&dev, &model_i2c, &default_cfg);

    is31fl3731_set_pixel(&dev, 1, 0, 77);
    is31fl3731_set_pixel(&dev, 2, 0, 88);

    /* Frame 1 and 2 should have different values */
    ASSERT_EQ(model.pages[1][0x24], 77);
    ASSERT_EQ(model.pages[2][0x24], 88);
    /* Frame 0 should be unaffected */
    ASSERT_EQ(model.pages[0][0x24], 0);
}

TEST_MAIN("is31fl3731 hardware model")
    RUN(test_hw_init_ok);
    RUN(test_hw_init_exits_shutdown);
    RUN(test_hw_init_picture_mode);
    RUN(test_hw_init_writes_every_frame);
    RUN(test_hw_init_enables_leds);
    RUN(test_hw_set_pixel_round_trip);
    RUN(test_hw_fill_round_trip);
    RUN(test_hw_display_frame);
    RUN(test_hw_shutdown_round_trip);
    RUN(test_hw_led_enable_disable);
    RUN(test_hw_model_frame_read_rule);
    RUN(test_hw_led_enable_exact_bus);
    RUN(test_hw_led_shadow_tracks_chip);
    RUN(test_hw_led_enable_keeps_neighbours);
    RUN(test_hw_led_enable_frames_apart);
    RUN(test_hw_frame_isolation);
TEST_MAIN_END()
