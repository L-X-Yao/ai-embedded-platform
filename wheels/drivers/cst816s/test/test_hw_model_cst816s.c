/*
 * test_hw_model_cst816s.c — tests using hardware register model
 *
 * Validates CST816S driver against a datasheet-accurate register model,
 * not just a mock. The model enforces read-only registers and POR defaults.
 */

#include "test_harness.h"
#include "hw_model_cst816s.h"
#include "../cst816s.h"

static cst816s_model_t model;

static const cst816s_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .ctx       = &model.hw,
};

static void setup(void)
{
    cst816s_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_hw_init_por_defaults)
{
    setup();
    cst816s_t dev;
    cst816s_err_t err = cst816s_init(&dev, &model_i2c);
    ASSERT_EQ(err, CST816S_OK);
    ASSERT_EQ(cst816s_get_chip_id(&dev), 0xB5);
    ASSERT_EQ(cst816s_get_firmware_version(&dev), 0x03);
}

TEST(test_hw_init_wrong_chip_id)
{
    setup();
    /* Corrupt chip ID before reset */
    model.hw.reset_values[CST816S_REG_CHIP_ID] = 0xFF;
    hw_model_reset(&model.hw);

    cst816s_t dev;
    cst816s_err_t err = cst816s_init(&dev, &model_i2c);
    ASSERT_EQ(err, CST816S_ERR_ID);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_hw_read_round_trip)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    /* Inject touch: (120, 150), event=down, gesture=single_tap, 1 finger */
    cst816s_model_inject_touch(&model, 120, 150, 0, 0x05, 1);

    cst816s_touch_t touch;
    cst816s_err_t err = cst816s_read(&dev, &touch);
    ASSERT_EQ(err, CST816S_OK);
    ASSERT_EQ(touch.x, 120);
    ASSERT_EQ(touch.y, 150);
    ASSERT_EQ((int)touch.event, (int)CST816S_EVENT_DOWN);
    ASSERT_EQ((int)touch.gesture, (int)CST816S_GESTURE_SINGLE_TAP);
    ASSERT_EQ(touch.fingers, 1);
}

TEST(test_hw_read_no_touch)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    cst816s_model_clear_touch(&model);

    cst816s_touch_t touch;
    cst816s_err_t err = cst816s_read(&dev, &touch);
    ASSERT_EQ(err, CST816S_OK);
    ASSERT_EQ(touch.fingers, 0);
    ASSERT_EQ((int)touch.gesture, (int)CST816S_GESTURE_NONE);
}

TEST(test_hw_read_all_gestures)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    static const struct { uint8_t raw; cst816s_gesture_t expected; } cases[] = {
        { 0x01, CST816S_GESTURE_SLIDE_UP },
        { 0x02, CST816S_GESTURE_SLIDE_DOWN },
        { 0x03, CST816S_GESTURE_SLIDE_LEFT },
        { 0x04, CST816S_GESTURE_SLIDE_RIGHT },
        { 0x05, CST816S_GESTURE_SINGLE_TAP },
        { 0x0B, CST816S_GESTURE_DOUBLE_TAP },
        { 0x0C, CST816S_GESTURE_LONG_PRESS },
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        cst816s_model_inject_touch(&model, 100, 100, 0, cases[i].raw, 1);
        cst816s_touch_t touch;
        ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
        ASSERT_EQ((int)touch.gesture, (int)cases[i].expected);
    }
}

TEST(test_hw_read_all_events)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    /* down=0 */
    cst816s_model_inject_touch(&model, 50, 50, 0, 0, 1);
    cst816s_touch_t touch;
    ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
    ASSERT_EQ((int)touch.event, (int)CST816S_EVENT_DOWN);

    /* up=1 */
    cst816s_model_inject_touch(&model, 50, 50, 1, 0, 0);
    ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
    ASSERT_EQ((int)touch.event, (int)CST816S_EVENT_UP);

    /* contact=2 */
    cst816s_model_inject_touch(&model, 50, 50, 2, 0, 1);
    ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
    ASSERT_EQ((int)touch.event, (int)CST816S_EVENT_CONTACT);
}

/* --- config register tests --------------------------------------------- */

TEST(test_hw_auto_sleep_register)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    /* Disable auto-sleep */
    cst816s_set_auto_sleep(&dev, false);
    ASSERT_EQ(model.hw.regs[CST816S_REG_DIS_AUTO_SLEEP], 0x01);

    /* Enable auto-sleep */
    cst816s_set_auto_sleep(&dev, true);
    ASSERT_EQ(model.hw.regs[CST816S_REG_DIS_AUTO_SLEEP], 0x00);
}

TEST(test_hw_motion_mask_register)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    cst816s_set_motion_mask(&dev, true, true, true);
    ASSERT_EQ(model.hw.regs[CST816S_REG_MOTION_MASK], 0x07);

    cst816s_set_motion_mask(&dev, false, false, false);
    ASSERT_EQ(model.hw.regs[CST816S_REG_MOTION_MASK], 0x00);

    cst816s_set_motion_mask(&dev, true, false, true);
    ASSERT_EQ(model.hw.regs[CST816S_REG_MOTION_MASK], 0x05);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_hw_chip_id_readonly)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    /* Try to write chip ID register -- model should prevent it */
    hw_model__write_reg(&model.hw, CST816S_REG_CHIP_ID, 0x00);
    ASSERT_EQ(model.hw.regs[CST816S_REG_CHIP_ID], 0xB5);
}

TEST(test_hw_fw_version_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, CST816S_REG_FW_VERSION, 0xFF);
    ASSERT_EQ(model.hw.regs[CST816S_REG_FW_VERSION], 0x03);
}

TEST(test_hw_touch_regs_readonly)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    /* Inject a touch so registers are non-zero */
    cst816s_model_inject_touch(&model, 200, 300, 0, 0x05, 1);
    uint8_t original_xh = model.hw.regs[CST816S_REG_XH];

    /* Try to write touch XH register -- should be blocked */
    hw_model__write_reg(&model.hw, CST816S_REG_XH, 0xFF);
    ASSERT_EQ(model.hw.regs[CST816S_REG_XH], original_xh);
}

TEST(test_hw_multiple_reads_consistent)
{
    setup();
    cst816s_t dev;
    cst816s_init(&dev, &model_i2c);

    cst816s_model_inject_touch(&model, 80, 160, 2, 0x03, 1);

    cst816s_touch_t t1, t2;
    cst816s_read(&dev, &t1);
    cst816s_read(&dev, &t2);

    ASSERT_EQ(t1.x, t2.x);
    ASSERT_EQ(t1.y, t2.y);
    ASSERT_EQ((int)t1.event, (int)t2.event);
    ASSERT_EQ((int)t1.gesture, (int)t2.gesture);
    ASSERT_EQ(t1.fingers, t2.fingers);
}

TEST_MAIN("cst816s hardware model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_init_wrong_chip_id);
    RUN(test_hw_read_round_trip);
    RUN(test_hw_read_no_touch);
    RUN(test_hw_read_all_gestures);
    RUN(test_hw_read_all_events);
    RUN(test_hw_auto_sleep_register);
    RUN(test_hw_motion_mask_register);
    RUN(test_hw_chip_id_readonly);
    RUN(test_hw_fw_version_readonly);
    RUN(test_hw_touch_regs_readonly);
    RUN(test_hw_multiple_reads_consistent);
TEST_MAIN_END()
