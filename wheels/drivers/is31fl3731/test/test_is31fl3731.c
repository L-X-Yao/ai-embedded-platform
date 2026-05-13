// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "is31fl3731.h"

/* ---------- chip-specific mock setup ---------- */

/* IS31FL3731 has a page-based register model. The mock tracks the
 * currently selected page (via writes to 0xFD) and stores register
 * values in a flat array per page. */

#define NUM_PAGES 12   /* frames 0-7 + function page 0x0B */
static uint8_t page_regs[NUM_PAGES][256];
static uint8_t current_page;

static void mock_reset_pages(void)
{
    memset(page_regs, 0, sizeof(page_regs));
    current_page = 0;
    mock_i2c_reset();
}

/* Page-aware write_reg: intercepts 0xFD for page select */
static bool page_write_reg(void *ctx, uint8_t dev_addr,
                            uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;

    if (reg == 0xFD) {
        current_page = val;
        mock_regs[0xFD] = val;
        return true;
    }

    /* Store in the current page */
    if (current_page < NUM_PAGES) {
        page_regs[current_page][reg] = val;
    }
    /* Also mirror to flat mock_regs for simple checks */
    mock_regs[reg] = val;
    return true;
}

/* Page-aware read_regs */
static bool page_read_regs(void *ctx, uint8_t dev_addr,
                            uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_read_count++;

    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        if (current_page < NUM_PAGES && idx < 256) {
            buf[i] = page_regs[current_page][idx];
        } else {
            buf[i] = 0;
        }
    }
    return true;
}

static const is31fl3731_i2c_t mock_i2c = {
    .write_reg = page_write_reg,
    .read_regs = page_read_regs,
    .ctx       = NULL,
};

static is31fl3731_t dev;
static is31fl3731_cfg_t default_cfg = {
    .addr = IS31FL3731_ADDR_BASE,
};

static void setup(void)
{
    mock_reset_pages();
    is31fl3731_init(&dev, &mock_i2c, &default_cfg);
    mock_op_count = 0;
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset_pages();
    is31fl3731_t d;
    ASSERT_EQ(is31fl3731_init(&d, &mock_i2c, &default_cfg), IS31FL3731_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset_pages();
    mock_fail_at = 0;
    is31fl3731_t d;
    ASSERT_EQ(is31fl3731_init(&d, &mock_i2c, &default_cfg), IS31FL3731_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(is31fl3731_init(NULL, &mock_i2c, &default_cfg), IS31FL3731_ERR_PARAM);
    is31fl3731_t d;
    ASSERT_EQ(is31fl3731_init(&d, NULL, &default_cfg), IS31FL3731_ERR_PARAM);
    ASSERT_EQ(is31fl3731_init(&d, &mock_i2c, NULL), IS31FL3731_ERR_PARAM);

    ASSERT_EQ(is31fl3731_set_frame(NULL, 0), IS31FL3731_ERR_PARAM);
    ASSERT_EQ(is31fl3731_set_pixel(NULL, 0, 0, 0), IS31FL3731_ERR_PARAM);
    ASSERT_EQ(is31fl3731_set_led_enable(NULL, 0, 0, true), IS31FL3731_ERR_PARAM);
    ASSERT_EQ(is31fl3731_fill(NULL, 0, 0), IS31FL3731_ERR_PARAM);
    ASSERT_EQ(is31fl3731_display_frame(NULL, 0), IS31FL3731_ERR_PARAM);
    ASSERT_EQ(is31fl3731_shutdown(NULL, true), IS31FL3731_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    is31fl3731_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    is31fl3731_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    is31fl3731_cfg_t cfg = default_cfg;
    ASSERT_TRUE(is31fl3731_init(&zero_dev, &zero_hal, &cfg) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)IS31FL3731_ERR_I2C);
    ASSERT_EQ(-2, (int)IS31FL3731_ERR_ID);
    ASSERT_EQ(-3, (int)IS31FL3731_ERR_PARAM);
}

/* ---------- init sequence verification ---------- */

TEST(test_init_sets_picture_mode)
{
    mock_reset_pages();
    is31fl3731_t d;
    is31fl3731_init(&d, &mock_i2c, &default_cfg);
    /* Function page (0x0B): CONFIG register (0x00) should be 0x00 (picture mode) */
    ASSERT_EQ(page_regs[0x0B][0x00], 0x00);
}

TEST(test_init_exits_shutdown)
{
    mock_reset_pages();
    is31fl3731_t d;
    is31fl3731_init(&d, &mock_i2c, &default_cfg);
    /* Function page: SHUTDOWN register (0x0A) should be 0x01 (normal) */
    ASSERT_EQ(page_regs[0x0B][0x0A], 0x01);
}

TEST(test_init_enables_all_leds_frame0)
{
    mock_reset_pages();
    is31fl3731_t d;
    is31fl3731_init(&d, &mock_i2c, &default_cfg);
    /* Frame 0 page: LED control bytes 0x00-0x11 should all be 0xFF */
    for (int i = 0; i < 18; i++) {
        ASSERT_EQ(page_regs[0][i], 0xFF);
    }
}

/* ---------- set_pixel tests ---------- */

TEST(test_set_pixel_led0)
{
    setup();
    ASSERT_EQ(is31fl3731_set_pixel(&dev, 0, 0, 128), IS31FL3731_OK);
    /* LED 0 -> PWM register 0x24 on frame 0 page */
    ASSERT_EQ(page_regs[0][0x24], 128);
}

TEST(test_set_pixel_led143)
{
    setup();
    ASSERT_EQ(is31fl3731_set_pixel(&dev, 0, 143, 255), IS31FL3731_OK);
    /* LED 143 -> PWM register 0x24 + 143 = 0xB3 */
    ASSERT_EQ(page_regs[0][0xB3], 255);
}

TEST(test_set_pixel_invalid_led)
{
    setup();
    ASSERT_EQ(is31fl3731_set_pixel(&dev, 0, 144, 128), IS31FL3731_ERR_PARAM);
    ASSERT_EQ(is31fl3731_set_pixel(&dev, 0, 255, 128), IS31FL3731_ERR_PARAM);
}

TEST(test_set_pixel_invalid_frame)
{
    setup();
    ASSERT_EQ(is31fl3731_set_pixel(&dev, 8, 0, 128), IS31FL3731_ERR_PARAM);
}

TEST(test_set_pixel_frame_switching)
{
    setup();
    ASSERT_EQ(is31fl3731_set_pixel(&dev, 3, 10, 200), IS31FL3731_OK);
    /* Verify it went to frame 3 page */
    ASSERT_EQ(page_regs[3][0x24 + 10], 200);
    /* Frame 0 should be unaffected */
    ASSERT_EQ(page_regs[0][0x24 + 10], 0);
}

/* ---------- fill tests ---------- */

TEST(test_fill_all_leds)
{
    setup();
    ASSERT_EQ(is31fl3731_fill(&dev, 0, 64), IS31FL3731_OK);
    /* All 144 PWM registers should be 64 */
    for (int i = 0; i < 144; i++) {
        ASSERT_EQ(page_regs[0][0x24 + i], 64);
    }
}

TEST(test_fill_invalid_frame)
{
    setup();
    ASSERT_EQ(is31fl3731_fill(&dev, 8, 64), IS31FL3731_ERR_PARAM);
}

/* ---------- display_frame tests ---------- */

TEST(test_display_frame)
{
    setup();
    ASSERT_EQ(is31fl3731_display_frame(&dev, 5), IS31FL3731_OK);
    /* Function page: PICTURE_DISP register (0x01) should be 5 */
    ASSERT_EQ(page_regs[0x0B][0x01], 5);
}

TEST(test_display_frame_invalid)
{
    setup();
    ASSERT_EQ(is31fl3731_display_frame(&dev, 8), IS31FL3731_ERR_PARAM);
}

/* ---------- LED enable/disable tests ---------- */

TEST(test_led_enable_disable)
{
    setup();
    /* After init, all LEDs on frame 0 are enabled (0xFF) */
    /* Disable LED 0 (byte 0, bit 0) */
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 0, 0, false), IS31FL3731_OK);
    ASSERT_EQ(page_regs[0][0x00], 0xFE);

    /* Re-enable LED 0 */
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 0, 0, true), IS31FL3731_OK);
    ASSERT_EQ(page_regs[0][0x00], 0xFF);
}

TEST(test_led_enable_mid_byte)
{
    setup();
    /* Disable LED 12 (byte 1, bit 4) */
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 0, 12, false), IS31FL3731_OK);
    ASSERT_EQ(page_regs[0][0x01] & 0x10, 0x00);
}

TEST(test_led_enable_invalid_idx)
{
    setup();
    ASSERT_EQ(is31fl3731_set_led_enable(&dev, 0, 144, true), IS31FL3731_ERR_PARAM);
}

/* ---------- shutdown tests ---------- */

TEST(test_shutdown_enable)
{
    setup();
    ASSERT_EQ(is31fl3731_shutdown(&dev, true), IS31FL3731_OK);
    /* shutdown=true -> write 0x00 to SHUTDOWN register */
    ASSERT_EQ(page_regs[0x0B][0x0A], 0x00);
}

TEST(test_shutdown_disable)
{
    setup();
    is31fl3731_shutdown(&dev, true);
    ASSERT_EQ(is31fl3731_shutdown(&dev, false), IS31FL3731_OK);
    /* shutdown=false -> write 0x01 to SHUTDOWN register */
    ASSERT_EQ(page_regs[0x0B][0x0A], 0x01);
}

/* ---------- address tests ---------- */

TEST(test_addr_0x75)
{
    mock_reset_pages();
    is31fl3731_t d;
    is31fl3731_cfg_t cfg = { .addr = 0x75 };
    ASSERT_EQ(is31fl3731_init(&d, &mock_i2c, &cfg), IS31FL3731_OK);
}

TEST(test_addr_0x77)
{
    mock_reset_pages();
    is31fl3731_t d;
    is31fl3731_cfg_t cfg = { .addr = 0x77 };
    ASSERT_EQ(is31fl3731_init(&d, &mock_i2c, &cfg), IS31FL3731_OK);
}

TEST(test_addr_invalid)
{
    mock_reset_pages();
    is31fl3731_t d;
    is31fl3731_cfg_t cfg = { .addr = 0x78 };
    ASSERT_EQ(is31fl3731_init(&d, &mock_i2c, &cfg), IS31FL3731_ERR_PARAM);
}

/* ---------- set_frame tests ---------- */

TEST(test_set_frame_valid)
{
    setup();
    ASSERT_EQ(is31fl3731_set_frame(&dev, 7), IS31FL3731_OK);
    ASSERT_EQ(dev.current_frame, 7);
}

TEST(test_set_frame_invalid)
{
    setup();
    ASSERT_EQ(is31fl3731_set_frame(&dev, 8), IS31FL3731_ERR_PARAM);
}

TEST_MAIN("is31fl3731")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_init_sets_picture_mode);
    RUN(test_init_exits_shutdown);
    RUN(test_init_enables_all_leds_frame0);
    RUN(test_set_pixel_led0);
    RUN(test_set_pixel_led143);
    RUN(test_set_pixel_invalid_led);
    RUN(test_set_pixel_invalid_frame);
    RUN(test_set_pixel_frame_switching);
    RUN(test_fill_all_leds);
    RUN(test_fill_invalid_frame);
    RUN(test_display_frame);
    RUN(test_display_frame_invalid);
    RUN(test_led_enable_disable);
    RUN(test_led_enable_mid_byte);
    RUN(test_led_enable_invalid_idx);
    RUN(test_shutdown_enable);
    RUN(test_shutdown_disable);
    RUN(test_addr_0x75);
    RUN(test_addr_0x77);
    RUN(test_addr_invalid);
    RUN(test_set_frame_valid);
    RUN(test_set_frame_invalid);
TEST_MAIN_END()
