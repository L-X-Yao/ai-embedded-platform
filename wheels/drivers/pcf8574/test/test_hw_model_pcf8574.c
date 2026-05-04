/*
 * test_hw_model_pcf8574.c -- tests using PCF8574 hardware model
 *
 * Validates PCF8574 driver against a hardware model that enforces
 * quasi-bidirectional I/O behavior, output latch caching, and
 * single-pin read/write/toggle operations.
 */

#include "test_harness.h"
#include "hw_model_pcf8574.h"
#include "../pcf8574.h"

static pcf8574_model_t model;

static const pcf8574_i2c_t model_i2c = {
    .write    = pcf8574_model_i2c_write,
    .read     = pcf8574_model_i2c_read,
    .delay_ms = NULL,
    .ctx      = &model,
};

static void setup(void)
{
    pcf8574_model_init(&model);
}

/* --- init tests --- */

TEST(test_init_success)
{
    setup();
    pcf8574_t dev;
    pcf8574_err_t err = pcf8574_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(err, PCF8574_OK);
    /* Init writes 0xFF (all high) */
    ASSERT_EQ(model.output_latch, 0xFF);
    ASSERT_EQ(model.write_count, 1);
}

/* --- write tests --- */

TEST(test_write_all_low)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);

    pcf8574_err_t err = pcf8574_write(&dev, 0x00);
    ASSERT_EQ(err, PCF8574_OK);
    ASSERT_EQ(model.output_latch, 0x00);
}

TEST(test_write_pattern)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);

    pcf8574_write(&dev, 0xA5);
    ASSERT_EQ(model.output_latch, 0xA5);
}

/* --- read tests --- */

TEST(test_read_all_high)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);

    /* All outputs high, all externals high -> reads 0xFF */
    uint8_t val = 0;
    pcf8574_err_t err = pcf8574_read(&dev, &val);
    ASSERT_EQ(err, PCF8574_OK);
    ASSERT_EQ_HEX(val, 0xFF);
}

TEST(test_read_external_low)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);

    /* Set external pin 3 low */
    pcf8574_model_set_external(&model, 0xF7);

    uint8_t val = 0xFF;
    pcf8574_read(&dev, &val);
    /* Output latch is 0xFF (all high=input), external has pin 3 low */
    ASSERT_EQ_HEX(val, 0xF7);
}

TEST(test_read_output_low_overrides_external)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);

    /* Write pin 0 low (strong pull-down) */
    pcf8574_write(&dev, 0xFE);

    /* Even if external pin 0 is high, output low wins */
    pcf8574_model_set_external(&model, 0xFF);
    uint8_t val = 0xFF;
    pcf8574_read(&dev, &val);
    ASSERT_EQ_HEX(val, 0xFE);
}

/* --- single pin tests --- */

TEST(test_write_pin_high_to_low)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);

    /* All start high. Set pin 4 low. */
    pcf8574_err_t err = pcf8574_write_pin(&dev, 4, false);
    ASSERT_EQ(err, PCF8574_OK);
    ASSERT_EQ_HEX(model.output_latch, 0xEF);
}

TEST(test_read_pin)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);

    pcf8574_model_set_external(&model, 0xFD);  /* pin 1 externally low */

    bool level = true;
    pcf8574_err_t err = pcf8574_read_pin(&dev, 1, &level);
    ASSERT_EQ(err, PCF8574_OK);
    ASSERT_FALSE(level);

    /* Pin 0 should still be high */
    pcf8574_read_pin(&dev, 0, &level);
    ASSERT_TRUE(level);
}

TEST(test_toggle_pin)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);

    /* All start at 0xFF. Toggle pin 7. */
    pcf8574_err_t err = pcf8574_toggle_pin(&dev, 7);
    ASSERT_EQ(err, PCF8574_OK);
    ASSERT_EQ_HEX(model.output_latch, 0x7F);

    /* Toggle again -> back to 0xFF */
    pcf8574_toggle_pin(&dev, 7);
    ASSERT_EQ_HEX(model.output_latch, 0xFF);
}

/* --- error cases --- */

TEST(test_write_pin_invalid)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);
    ASSERT_EQ(pcf8574_write_pin(&dev, 8, true), PCF8574_ERR_PARAM);
}

TEST(test_read_pin_invalid)
{
    setup();
    pcf8574_t dev;
    pcf8574_init(&dev, &model_i2c, 0x20);
    bool level;
    ASSERT_EQ(pcf8574_read_pin(&dev, 8, &level), PCF8574_ERR_PARAM);
}

TEST_MAIN("pcf8574 hardware model")
    RUN(test_init_success);
    RUN(test_write_all_low);
    RUN(test_write_pattern);
    RUN(test_read_all_high);
    RUN(test_read_external_low);
    RUN(test_read_output_low_overrides_external);
    RUN(test_write_pin_high_to_low);
    RUN(test_read_pin);
    RUN(test_toggle_pin);
    RUN(test_write_pin_invalid);
    RUN(test_read_pin_invalid);
TEST_MAIN_END()
