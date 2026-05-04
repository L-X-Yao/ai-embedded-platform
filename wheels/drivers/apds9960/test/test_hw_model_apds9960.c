/*
 * test_hw_model_apds9960.c — tests using hardware register model
 *
 * Validates APDS-9960 driver against a datasheet-accurate register model.
 * The model enforces read-only chip ID (0xAB), RGBC data, proximity,
 * gesture FIFO behavior, and enable register control.
 */

#include "test_harness.h"
#include "hw_model_apds9960.h"
#include "../apds9960.h"
#include <math.h>

static apds9960_model_t model;

static const apds9960_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = apds9960_model_i2c_read_regs,  /* custom for gesture FIFO */
    .delay_ms  = NULL,
    .ctx       = &model,
};

static void setup(void)
{
    apds9960_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    apds9960_t dev;
    apds9960_err_t err = apds9960_init(&dev, &model_i2c);
    ASSERT_EQ(err, APDS9960_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[APDS_REG_ID] = 0x00;
    hw_model_reset(&model.hw);

    apds9960_t dev;
    apds9960_err_t err = apds9960_init(&dev, &model_i2c);
    ASSERT_EQ(err, APDS9960_ERR_ID);
}

TEST(test_init_powers_on)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    /* Enable register should have PON bit set */
    ASSERT_TRUE(model.hw.regs[APDS_REG_ENABLE] & 0x01u);
}

TEST(test_init_sets_atime)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    ASSERT_EQ(model.hw.regs[APDS_REG_ATIME], 0xFFu);
}

TEST(test_init_sets_ppulse)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    ASSERT_EQ(model.hw.regs[APDS_REG_PPULSE], 0x87u);
}

/* --- proximity tests --------------------------------------------------- */

TEST(test_enable_proximity)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_proximity(&dev);
    /* Enable should have PEN + PON */
    ASSERT_TRUE(model.hw.regs[APDS_REG_ENABLE] & 0x04u);
    ASSERT_TRUE(model.hw.regs[APDS_REG_ENABLE] & 0x01u);
}

TEST(test_read_proximity)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_proximity(&dev);

    uint8_t prox = 0;
    apds9960_err_t err = apds9960_read_proximity(&dev, &prox);
    ASSERT_EQ(err, APDS9960_OK);
    ASSERT_EQ(prox, 128);
}

/* --- color tests ------------------------------------------------------- */

TEST(test_read_color)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_color(&dev);

    apds9960_color_t color;
    apds9960_err_t err = apds9960_read_color(&dev, &color);
    ASSERT_EQ(err, APDS9960_OK);
    ASSERT_EQ(color.c, 1200);
    ASSERT_EQ(color.r, 400);
    ASSERT_EQ(color.g, 500);
    ASSERT_EQ(color.b, 300);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, APDS_REG_ID, 0x00);
    ASSERT_EQ(model.hw.regs[APDS_REG_ID], 0xABu);
}

TEST(test_color_data_readonly)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    uint8_t orig = model.hw.regs[APDS_REG_CDATAL];
    hw_model__write_reg(&model.hw, APDS_REG_CDATAL, 0xFF);
    ASSERT_EQ(model.hw.regs[APDS_REG_CDATAL], orig);
}

TEST(test_disable_all)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_proximity(&dev);
    apds9960_disable_all(&dev);
    ASSERT_EQ(model.hw.regs[APDS_REG_ENABLE], 0x00u);
}

TEST(test_gesture_detection)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_gesture(&dev);

    /* Load gesture FIFO: U=200 D=50 L=100 R=100 -> UP gesture */
    uint8_t fifo_data[4] = { 200, 50, 100, 100 };
    apds9960_model_set_gesture(&model, 1, fifo_data);

    apds9960_gesture_t gest = APDS9960_GESTURE_NONE;
    apds9960_err_t err = apds9960_read_gesture(&dev, &gest);
    ASSERT_EQ(err, APDS9960_OK);
    ASSERT_EQ(gest, APDS9960_GESTURE_UP);
}

TEST_MAIN("apds9960 hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_powers_on);
    RUN(test_init_sets_atime);
    RUN(test_init_sets_ppulse);
    RUN(test_enable_proximity);
    RUN(test_read_proximity);
    RUN(test_read_color);
    RUN(test_chip_id_readonly);
    RUN(test_color_data_readonly);
    RUN(test_disable_all);
    RUN(test_gesture_detection);
TEST_MAIN_END()
