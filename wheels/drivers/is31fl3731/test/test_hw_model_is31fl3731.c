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
    RUN(test_hw_init_enables_leds);
    RUN(test_hw_set_pixel_round_trip);
    RUN(test_hw_fill_round_trip);
    RUN(test_hw_display_frame);
    RUN(test_hw_shutdown_round_trip);
    RUN(test_hw_led_enable_disable);
    RUN(test_hw_frame_isolation);
TEST_MAIN_END()
