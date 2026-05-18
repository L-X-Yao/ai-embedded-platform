// SPDX-License-Identifier: MIT
/*
 * test_hw_model_ft6x36.c — tests using hardware model
 *
 * Validates FT6x36 driver against a datasheet-accurate model.
 * The model enforces register layout, touch point encoding,
 * event fields, coordinate extraction, and threshold write-back.
 */

#include "test_harness.h"
#include "hw_model_ft6x36.h"
#include "../ft6x36.h"

static ft6x36_model_t model;

static const ft6x36_i2c_t model_i2c = {
    .write_reg = ft6x36_model_write_reg,
    .read_regs = ft6x36_model_read_regs,
    .ctx       = &model,
};

static ft6x36_t dev;

static void setup(void)
{
    ft6x36_model_init(&model, FT6X36_CHIP_ID_6236);
    ft6x36_init(&dev, &model_i2c);
}

/* --- init --- */

TEST(test_init_reads_chip_id)
{
    ft6x36_model_init(&model, FT6X36_CHIP_ID_6236);
    ft6x36_t d;
    ft6x36_err_t err = ft6x36_init(&d, &model_i2c);
    ASSERT_EQ(err, FT6X36_OK);
    ASSERT_EQ(model.read_count, 1);
    ASSERT_EQ(d.chip_id, FT6X36_CHIP_ID_6236);
}

TEST(test_init_ft6336_variant)
{
    ft6x36_model_init(&model, FT6X36_CHIP_ID_6336);
    ft6x36_t d;
    ft6x36_err_t err = ft6x36_init(&d, &model_i2c);
    ASSERT_EQ(err, FT6X36_OK);
    ASSERT_EQ(d.chip_id, FT6X36_CHIP_ID_6336);
}

/* --- single touch round-trip --- */

TEST(test_single_touch_roundtrip)
{
    setup();
    ft6x36_model_set_touch1(&model, 240, 320, FT6X36_EVT_PRESS);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.touch_count, 1);
    ASSERT_EQ(touch.x1, 240);
    ASSERT_EQ(touch.y1, 320);
    ASSERT_EQ(touch.event1, FT6X36_EVT_PRESS);
}

TEST(test_single_touch_release_event)
{
    setup();
    ft6x36_model_set_touch1(&model, 100, 50, FT6X36_EVT_RELEASE);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.event1, FT6X36_EVT_RELEASE);
}

TEST(test_single_touch_contact_event)
{
    setup();
    ft6x36_model_set_touch1(&model, 100, 50, FT6X36_EVT_CONTACT);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.event1, FT6X36_EVT_CONTACT);
}

/* --- dual touch round-trip --- */

TEST(test_dual_touch_roundtrip)
{
    setup();
    ft6x36_model_set_touch1(&model, 100, 200, FT6X36_EVT_PRESS);
    ft6x36_model_set_touch2(&model, 300, 400, FT6X36_EVT_CONTACT);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.touch_count, 2);
    ASSERT_EQ(touch.x1, 100);
    ASSERT_EQ(touch.y1, 200);
    ASSERT_EQ(touch.event1, FT6X36_EVT_PRESS);
    ASSERT_EQ(touch.x2, 300);
    ASSERT_EQ(touch.y2, 400);
    ASSERT_EQ(touch.event2, FT6X36_EVT_CONTACT);
}

/* --- no touch --- */

TEST(test_no_touch)
{
    setup();
    ft6x36_model_clear_touch(&model);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.touch_count, 0);
}

/* --- threshold write-back --- */

TEST(test_threshold_roundtrip)
{
    setup();
    ft6x36_set_threshold(&dev, 80);
    ASSERT_EQ(model.threshold, 80);
}

TEST(test_threshold_min)
{
    setup();
    ft6x36_set_threshold(&dev, 0);
    ASSERT_EQ(model.threshold, 0);
}

TEST(test_threshold_max)
{
    setup();
    ft6x36_set_threshold(&dev, 255);
    ASSERT_EQ(model.threshold, 255);
}

/* --- edge coordinates --- */

TEST(test_max_coordinates)
{
    setup();
    ft6x36_model_set_touch1(&model, 4095, 4095, FT6X36_EVT_PRESS);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.x1, 4095);
    ASSERT_EQ(touch.y1, 4095);
}

TEST(test_zero_coordinates)
{
    setup();
    ft6x36_model_set_touch1(&model, 0, 0, FT6X36_EVT_PRESS);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.x1, 0);
    ASSERT_EQ(touch.y1, 0);
}

TEST(test_asymmetric_coordinates)
{
    setup();
    ft6x36_model_set_touch1(&model, 0x0ABC, 0x0DEF, FT6X36_EVT_PRESS);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.x1, 0x0ABC);
    ASSERT_EQ(touch.y1, 0x0DEF);
}

/* --- firmware version read-back --- */

TEST(test_firmware_version_readback)
{
    setup();
    uint8_t ver = 0;
    ft6x36_get_firmware_version(&dev, &ver);
    ASSERT_EQ(ver, 0x10);
}

/* --- chip ID read-back --- */

TEST(test_chip_id_readback)
{
    setup();
    uint8_t id = 0;
    ft6x36_get_chip_id(&dev, &id);
    ASSERT_EQ(id, FT6X36_CHIP_ID_6236);
}

/* --- touch then clear --- */

TEST(test_touch_then_clear)
{
    setup();
    ft6x36_model_set_touch1(&model, 150, 250, FT6X36_EVT_PRESS);

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.touch_count, 1);

    ft6x36_model_clear_touch(&model);
    ft6x36_read_touch(&dev, &touch);
    ASSERT_EQ(touch.touch_count, 0);
}

/* --- sequential reads --- */

TEST(test_sequential_touch_updates)
{
    setup();
    for (uint16_t i = 0; i < 5; i++) {
        ft6x36_model_set_touch1(&model, (uint16_t)(i * 100),
                                (uint16_t)(i * 200), FT6X36_EVT_CONTACT);
        model.touch_count = 1;

        ft6x36_touch_t touch;
        ft6x36_read_touch(&dev, &touch);
        ASSERT_EQ(touch.x1, (uint16_t)(i * 100));
        ASSERT_EQ(touch.y1, (uint16_t)(i * 200));
    }
}

TEST_MAIN("ft6x36 hardware model")
    RUN(test_init_reads_chip_id);
    RUN(test_init_ft6336_variant);
    RUN(test_single_touch_roundtrip);
    RUN(test_single_touch_release_event);
    RUN(test_single_touch_contact_event);
    RUN(test_dual_touch_roundtrip);
    RUN(test_no_touch);
    RUN(test_threshold_roundtrip);
    RUN(test_threshold_min);
    RUN(test_threshold_max);
    RUN(test_max_coordinates);
    RUN(test_zero_coordinates);
    RUN(test_asymmetric_coordinates);
    RUN(test_firmware_version_readback);
    RUN(test_chip_id_readback);
    RUN(test_touch_then_clear);
    RUN(test_sequential_touch_updates);
TEST_MAIN_END()
