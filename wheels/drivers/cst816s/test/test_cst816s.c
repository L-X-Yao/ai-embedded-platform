// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "cst816s.h"

/* ---------- register addresses (mirrored from driver for test clarity) -- */

#define REG_GESTURE_ID       0x01u
#define REG_FINGER_NUM       0x02u
#define REG_XH               0x03u
#define REG_XL               0x04u
#define REG_YH               0x05u
#define REG_YL               0x06u
#define REG_CHIP_ID          0xA7u
#define REG_FW_VERSION       0xA9u
#define REG_MOTION_MASK      0xAAu
#define REG_DIS_AUTO_SLEEP   0xFEu

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* CST816S chip ID = 0xB5 at register 0xA7 */
    mock_regs[REG_CHIP_ID]    = 0xB5;
    /* Firmware version (arbitrary test value) */
    mock_regs[REG_FW_VERSION] = 0x03;
}

/* Plant touch data into mock registers */
static void plant_touch(uint16_t x, uint16_t y, uint8_t event,
                        uint8_t gesture, uint8_t fingers)
{
    mock_regs[REG_GESTURE_ID] = gesture;
    mock_regs[REG_FINGER_NUM] = fingers;
    mock_regs[REG_XH] = (uint8_t)((event << 6) | ((x >> 8) & 0x0Fu));
    mock_regs[REG_XL] = (uint8_t)(x & 0xFFu);
    mock_regs[REG_YH] = (uint8_t)((y >> 8) & 0x0Fu);
    mock_regs[REG_YL] = (uint8_t)(y & 0xFFu);
}

static const cst816s_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static cst816s_t dev;

/* ---------- init tests ---------- */

TEST(test_init_null)
{
    ASSERT_EQ(cst816s_init(NULL, &mock_i2c), CST816S_ERR_PARAM);
    ASSERT_EQ(cst816s_init(&dev, NULL), CST816S_ERR_PARAM);

    cst816s_touch_t touch;
    ASSERT_EQ(cst816s_read(NULL, &touch), CST816S_ERR_PARAM);
    ASSERT_EQ(cst816s_read(&dev, NULL), CST816S_ERR_PARAM);

    ASSERT_EQ(cst816s_set_auto_sleep(NULL, true), CST816S_ERR_PARAM);
    ASSERT_EQ(cst816s_set_motion_mask(NULL, false, false, false), CST816S_ERR_PARAM);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[REG_CHIP_ID] = 0x00;  /* not 0xB5 */
    ASSERT_EQ(cst816s_init(&dev, &mock_i2c), CST816S_ERR_ID);
}

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(cst816s_init(&dev, &mock_i2c), CST816S_OK);
    ASSERT_EQ(cst816s_get_chip_id(&dev), 0xB5);
    ASSERT_EQ(cst816s_get_firmware_version(&dev), 0x03);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* first I2C op (chip ID read) fails */
    ASSERT_EQ(cst816s_init(&dev, &mock_i2c), CST816S_ERR_I2C);
}

TEST(test_init_fw_read_i2c_fail)
{
    mock_reset();
    mock_fail_at = 1;  /* second I2C op (firmware version read) fails */
    ASSERT_EQ(cst816s_init(&dev, &mock_i2c), CST816S_ERR_I2C);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers -- init must reject it */
    cst816s_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    cst816s_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(cst816s_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)CST816S_ERR_I2C);
    ASSERT_EQ(-2, (int)CST816S_ERR_ID);
    ASSERT_EQ(-3, (int)CST816S_ERR_PARAM);
}

/* ---------- read tests ---------- */

TEST(test_read_touch)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    /* Plant touch at (120, 150), event=down, gesture=single_tap, 1 finger */
    plant_touch(120, 150, 0, CST816S_GESTURE_SINGLE_TAP, 1);

    cst816s_touch_t touch;
    ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
    ASSERT_EQ(touch.x, 120);
    ASSERT_EQ(touch.y, 150);
    ASSERT_EQ((int)touch.event, (int)CST816S_EVENT_DOWN);
    ASSERT_EQ((int)touch.gesture, (int)CST816S_GESTURE_SINGLE_TAP);
    ASSERT_EQ(touch.fingers, 1);
}

TEST(test_read_no_touch)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    /* No touch: fingers = 0 */
    plant_touch(0, 0, 0, CST816S_GESTURE_NONE, 0);

    cst816s_touch_t touch;
    ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
    ASSERT_EQ(touch.fingers, 0);
    ASSERT_EQ((int)touch.gesture, (int)CST816S_GESTURE_NONE);
}

TEST(test_read_i2c_fail)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    mock_fail_next = 1;
    cst816s_touch_t touch;
    ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_ERR_I2C);
}

TEST(test_read_large_coords)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    /* Max 12-bit coordinates: 0xFFF = 4095 */
    plant_touch(240, 320, 2, CST816S_GESTURE_NONE, 1);

    cst816s_touch_t touch;
    ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
    ASSERT_EQ(touch.x, 240);
    ASSERT_EQ(touch.y, 320);
    ASSERT_EQ((int)touch.event, (int)CST816S_EVENT_CONTACT);
}

TEST(test_read_event_up)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    plant_touch(50, 80, 1, CST816S_GESTURE_NONE, 0);

    cst816s_touch_t touch;
    ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
    ASSERT_EQ((int)touch.event, (int)CST816S_EVENT_UP);
}

/* ---------- gesture tests ---------- */

TEST(test_gesture_recognition)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);
    cst816s_touch_t touch;

    /* Test each gesture ID maps to correct enum */
    static const struct { uint8_t raw; cst816s_gesture_t expected; } cases[] = {
        { 0x00, CST816S_GESTURE_NONE },
        { 0x01, CST816S_GESTURE_SLIDE_UP },
        { 0x02, CST816S_GESTURE_SLIDE_DOWN },
        { 0x03, CST816S_GESTURE_SLIDE_LEFT },
        { 0x04, CST816S_GESTURE_SLIDE_RIGHT },
        { 0x05, CST816S_GESTURE_SINGLE_TAP },
        { 0x0B, CST816S_GESTURE_DOUBLE_TAP },
        { 0x0C, CST816S_GESTURE_LONG_PRESS },
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        plant_touch(100, 100, 0, cases[i].raw, 1);
        ASSERT_EQ(cst816s_read(&dev, &touch), CST816S_OK);
        ASSERT_EQ((int)touch.gesture, (int)cases[i].expected);
    }
}

/* ---------- auto-sleep tests ---------- */

TEST(test_set_auto_sleep)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    /* Enable auto-sleep: register 0xFE should be written with 0x00 */
    ASSERT_EQ(cst816s_set_auto_sleep(&dev, true), CST816S_OK);
    ASSERT_EQ(mock_regs[REG_DIS_AUTO_SLEEP], 0x00);

    /* Disable auto-sleep: register 0xFE should be written with 0x01 */
    ASSERT_EQ(cst816s_set_auto_sleep(&dev, false), CST816S_OK);
    ASSERT_EQ(mock_regs[REG_DIS_AUTO_SLEEP], 0x01);
}

TEST(test_set_auto_sleep_i2c_fail)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    mock_fail_next = 1;
    ASSERT_EQ(cst816s_set_auto_sleep(&dev, true), CST816S_ERR_I2C);
}

/* ---------- motion mask tests ---------- */

TEST(test_motion_mask)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    /* Enable all: continuous | double_click | long_press = 0x07 */
    ASSERT_EQ(cst816s_set_motion_mask(&dev, true, true, true), CST816S_OK);
    ASSERT_EQ(mock_regs[REG_MOTION_MASK], 0x07);

    /* Disable all: 0x00 */
    ASSERT_EQ(cst816s_set_motion_mask(&dev, false, false, false), CST816S_OK);
    ASSERT_EQ(mock_regs[REG_MOTION_MASK], 0x00);

    /* Only continuous: 0x01 */
    ASSERT_EQ(cst816s_set_motion_mask(&dev, true, false, false), CST816S_OK);
    ASSERT_EQ(mock_regs[REG_MOTION_MASK], 0x01);

    /* Only double_click: 0x02 */
    ASSERT_EQ(cst816s_set_motion_mask(&dev, false, true, false), CST816S_OK);
    ASSERT_EQ(mock_regs[REG_MOTION_MASK], 0x02);

    /* Only long_press: 0x04 */
    ASSERT_EQ(cst816s_set_motion_mask(&dev, false, false, true), CST816S_OK);
    ASSERT_EQ(mock_regs[REG_MOTION_MASK], 0x04);
}

TEST(test_motion_mask_i2c_fail)
{
    mock_reset();
    cst816s_init(&dev, &mock_i2c);

    mock_fail_next = 1;
    ASSERT_EQ(cst816s_set_motion_mask(&dev, true, true, true), CST816S_ERR_I2C);
}

/* ---------- getter tests ---------- */

TEST(test_get_chip_id_null)
{
    ASSERT_EQ(cst816s_get_chip_id(NULL), 0);
}

TEST(test_get_firmware_version_null)
{
    ASSERT_EQ(cst816s_get_firmware_version(NULL), 0);
}

TEST_MAIN("cst816s")
    RUN(test_init_null);
    RUN(test_init_wrong_id);
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_init_fw_read_i2c_fail);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_read_touch);
    RUN(test_read_no_touch);
    RUN(test_read_i2c_fail);
    RUN(test_read_large_coords);
    RUN(test_read_event_up);
    RUN(test_gesture_recognition);
    RUN(test_set_auto_sleep);
    RUN(test_set_auto_sleep_i2c_fail);
    RUN(test_motion_mask);
    RUN(test_motion_mask_i2c_fail);
    RUN(test_get_chip_id_null);
    RUN(test_get_firmware_version_null);
TEST_MAIN_END()
