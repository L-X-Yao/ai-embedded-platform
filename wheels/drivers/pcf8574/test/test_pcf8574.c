// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "pcf8574.h"

/*
 * Local command-based I2C mock.
 *
 * The PCF8574 has no register map — it uses raw I2C read/write of a
 * single byte, so the shared register-based mock_i2c.h does not apply.
 * This mock stores one byte (the "chip latch") and supports fail injection.
 */

static uint8_t mock_latch;       /* simulates the PCF8574 output latch */
static int     mock_fail_next;   /* if >0, next op fails and decrements */
static int     mock_write_count;
static int     mock_read_count;

static void mock_reset(void)
{
    mock_latch       = 0xFF;
    mock_fail_next   = 0;
    mock_write_count = 0;
    mock_read_count  = 0;
}

static bool mock_write(void *ctx, uint8_t dev_addr,
                        const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next > 0) { mock_fail_next--; return false; }
    if (len != 1) return false;
    mock_latch = data[0];
    mock_write_count++;
    return true;
}

static bool mock_read(void *ctx, uint8_t dev_addr,
                       uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next > 0) { mock_fail_next--; return false; }
    if (len != 1) return false;
    data[0] = mock_latch;
    mock_read_count++;
    return true;
}

static const pcf8574_i2c_t mock_i2c = {
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL,
};

static pcf8574_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(pcf8574_init(&dev, &mock_i2c, 0x20), PCF8574_OK);
    /* init should write 0xFF (all high) to chip */
    ASSERT_EQ(mock_latch, 0xFF);
    ASSERT_EQ(mock_write_count, 1);
    ASSERT_EQ(dev.dev_addr, 0x20);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_next = 1;
    ASSERT_EQ(pcf8574_init(&dev, &mock_i2c, 0x20), PCF8574_ERR_I2C);
}

TEST(test_init_null_params)
{
    mock_reset();
    ASSERT_EQ(pcf8574_init(NULL, &mock_i2c, 0x20), PCF8574_ERR_PARAM);
    ASSERT_EQ(pcf8574_init(&dev, NULL, 0x20), PCF8574_ERR_PARAM);
}

TEST(test_write_port)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    ASSERT_EQ(pcf8574_write(&dev, 0xA5), PCF8574_OK);
    ASSERT_EQ(mock_latch, 0xA5);
    ASSERT_EQ(dev.output_state, 0xA5);
}

TEST(test_read_port)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    mock_latch = 0x3C;
    uint8_t val = 0;
    ASSERT_EQ(pcf8574_read(&dev, &val), PCF8574_OK);
    ASSERT_EQ(val, 0x3C);
}

TEST(test_write_pin_set)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    pcf8574_write(&dev, 0x00);  /* all low */
    ASSERT_EQ(pcf8574_write_pin(&dev, 3, true), PCF8574_OK);
    ASSERT_EQ(mock_latch, 0x08);
    ASSERT_EQ(dev.output_state, 0x08);
}

TEST(test_write_pin_clear)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    /* output_state starts at 0xFF after init */
    ASSERT_EQ(pcf8574_write_pin(&dev, 5, false), PCF8574_OK);
    ASSERT_EQ(mock_latch, 0xDF);   /* bit 5 cleared: 0xFF & ~0x20 = 0xDF */
}

TEST(test_read_pin)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    mock_latch = 0x42;   /* bits 1 and 6 set */
    bool level = false;
    ASSERT_EQ(pcf8574_read_pin(&dev, 1, &level), PCF8574_OK);
    ASSERT_TRUE(level);
    ASSERT_EQ(pcf8574_read_pin(&dev, 0, &level), PCF8574_OK);
    ASSERT_FALSE(level);
}

TEST(test_toggle_pin)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    pcf8574_write(&dev, 0x00);
    ASSERT_EQ(pcf8574_toggle_pin(&dev, 7), PCF8574_OK);
    ASSERT_EQ(mock_latch, 0x80);
    ASSERT_EQ(pcf8574_toggle_pin(&dev, 7), PCF8574_OK);
    ASSERT_EQ(mock_latch, 0x00);
}

TEST(test_pin_out_of_range)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    bool level = false;
    ASSERT_EQ(pcf8574_write_pin(&dev, 8, true), PCF8574_ERR_PARAM);
    ASSERT_EQ(pcf8574_read_pin(&dev, 8, &level), PCF8574_ERR_PARAM);
    ASSERT_EQ(pcf8574_toggle_pin(&dev, 8), PCF8574_ERR_PARAM);
    ASSERT_EQ(pcf8574_write_pin(&dev, 255, true), PCF8574_ERR_PARAM);
}

TEST(test_i2c_write_fail)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    mock_fail_next = 1;
    ASSERT_EQ(pcf8574_write(&dev, 0x55), PCF8574_ERR_I2C);
    /* output_state must NOT update on failure */
    ASSERT_EQ(dev.output_state, 0xFF);
}

TEST(test_i2c_read_fail)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    mock_fail_next = 1;
    uint8_t val = 0xCC;
    ASSERT_EQ(pcf8574_read(&dev, &val), PCF8574_ERR_I2C);
}

TEST(test_read_modify_write_preserves_other_pins)
{
    mock_reset();
    pcf8574_init(&dev, &mock_i2c, 0x20);
    pcf8574_write(&dev, 0xA5);           /* 1010_0101 */
    pcf8574_write_pin(&dev, 2, false);   /* clear bit 2 → 1010_0001 = 0xA1 */
    ASSERT_EQ(mock_latch, 0xA1);
    pcf8574_write_pin(&dev, 6, true);    /* set bit 6 → 1110_0001 = 0xE1 */
    ASSERT_EQ(mock_latch, 0xE1);
    /* Bits 0,5,7 (from 0xA5) are preserved through both pin ops */
    ASSERT_TRUE((mock_latch & 0x01) != 0);  /* bit 0 */
    ASSERT_TRUE((mock_latch & 0x20) != 0);  /* bit 5 */
    ASSERT_TRUE((mock_latch & 0x80) != 0);  /* bit 7 */
}

TEST(test_address_range)
{
    mock_reset();
    /* PCF8574 range: 0x20..0x27 */
    ASSERT_EQ(pcf8574_init(&dev, &mock_i2c, 0x20), PCF8574_OK);
    ASSERT_EQ(dev.dev_addr, 0x20);
    ASSERT_EQ(pcf8574_init(&dev, &mock_i2c, 0x27), PCF8574_OK);
    ASSERT_EQ(dev.dev_addr, 0x27);
    /* PCF8574A range: 0x38..0x3F */
    ASSERT_EQ(pcf8574_init(&dev, &mock_i2c, 0x38), PCF8574_OK);
    ASSERT_EQ(dev.dev_addr, 0x38);
    ASSERT_EQ(pcf8574_init(&dev, &mock_i2c, 0x3F), PCF8574_OK);
    ASSERT_EQ(dev.dev_addr, 0x3F);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(pcf8574_t *d)
{
    uint8_t value;
    bool level;
    ASSERT_EQ(pcf8574_write(d, 0x00), PCF8574_ERR_PARAM);
    ASSERT_EQ(pcf8574_read(d, &value), PCF8574_ERR_PARAM);
    ASSERT_EQ(pcf8574_write_pin(d, 0, true), PCF8574_ERR_PARAM);
    ASSERT_EQ(pcf8574_read_pin(d, 0, &level), PCF8574_ERR_PARAM);
    ASSERT_EQ(pcf8574_toggle_pin(d, 0), PCF8574_ERR_PARAM);
}

TEST(test_device_refused_after_failed_link_check)
{
    pcf8574_t d;
    mock_reset();
    mock_fail_next = 1;
    ASSERT_EQ(pcf8574_init(&d, &mock_i2c, 0x20), PCF8574_ERR_I2C);
    assert_device_refused(&d);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const pcf8574_i2c_t zeroed;
    pcf8574_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(pcf8574_init(&d, NULL, 0x20), PCF8574_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(pcf8574_init(&d, &zeroed, 0x20), PCF8574_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("pcf8574")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_init_null_params);
    RUN(test_write_port);
    RUN(test_read_port);
    RUN(test_write_pin_set);
    RUN(test_write_pin_clear);
    RUN(test_read_pin);
    RUN(test_toggle_pin);
    RUN(test_pin_out_of_range);
    RUN(test_i2c_write_fail);
    RUN(test_i2c_read_fail);
    RUN(test_read_modify_write_preserves_other_pins);
    RUN(test_address_range);
    RUN(test_device_refused_after_failed_link_check);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
