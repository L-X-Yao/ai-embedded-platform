// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_ds18b20.h"

/* Table 2, p.8: maximum conversion time at each resolution. */
#define TCONV_9BIT_US   93750u
#define TCONV_10BIT_US 187500u
#define TCONV_11BIT_US 375000u
#define TCONV_12BIT_US 750000u

/* Convert T, then the full tCONV of the default 12-bit resolution. */
static void convert_and_wait(ds18b20_model_t *m, ds18b20_t *dev)
{
    (void)ds18b20_start_conversion(dev);
    ds18b20_model_advance_us(m, TCONV_12BIT_US);
}

/* One raw transaction, with no help from the driver: p.10 steps 1 to 3. */
static void read_frame(ds18b20_model_t *m, uint8_t sp[9])
{
    (void)ds18b20_model_reset(m);
    (void)ds18b20_model_write_byte(m, 0xCC);
    (void)ds18b20_model_write_byte(m, 0xBE);
    for (int i = 0; i < 9; i++)
        (void)ds18b20_model_read_byte(m, &sp[i]);
}

static void write_frame(ds18b20_model_t *m, uint8_t th, uint8_t tl, uint8_t cfg)
{
    (void)ds18b20_model_reset(m);
    (void)ds18b20_model_write_byte(m, 0xCC);
    (void)ds18b20_model_write_byte(m, 0x4E);
    (void)ds18b20_model_write_byte(m, th);
    (void)ds18b20_model_write_byte(m, tl);
    (void)ds18b20_model_write_byte(m, cfg);
}

/* ---------- tests ---------- */

TEST(test_read_25c)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 25.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    convert_and_wait(&model, &dev);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 25.0f, 0.1f);
}

TEST(test_read_negative_10c)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, -10.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    convert_and_wait(&model, &dev);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, -10.0f, 0.1f);
}

TEST(test_read_85c_power_on_default)
{
    /* DS18B20 powers on with 85.0 C in scratchpad */
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    /* model defaults to 85.0 C, don't call set_temperature */
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 85.0f, 0.1f);
}

TEST(test_crc_matches_scratchpad)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 25.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);
    convert_and_wait(&model, &dev);

    /* verify model-generated CRC matches recomputed CRC */
    uint8_t stored_crc = ds18b20_model_get_scratchpad_crc(&model);
    uint8_t computed_crc = ds18b20_model_verify_crc(&model);
    ASSERT_EQ(stored_crc, computed_crc);

    /* test with a different temperature too */
    ds18b20_model_set_temperature(&model, -40.0f);
    convert_and_wait(&model, &dev);
    /* p.8: the CRC "changes when the data in the scratchpad changes" */
    ASSERT_TRUE(ds18b20_model_get_scratchpad_crc(&model) != stored_crc);
    stored_crc = ds18b20_model_get_scratchpad_crc(&model);
    computed_crc = ds18b20_model_verify_crc(&model);
    ASSERT_EQ(stored_crc, computed_crc);
}

TEST(test_reset_presence_pulse)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_present(&model, 1);
    ds18b20_model_set_temperature(&model, 25.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    /* start_conversion does a reset + skip_rom + convert_t */
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
    ASSERT_EQ(ds18b20_model_get_reset_count(&model), 1);
    ASSERT_EQ(ds18b20_model_get_convert_count(&model), 1);

    /* read_temperature does another reset + skip_rom + read_scratchpad */
    ds18b20_model_advance_us(&model, TCONV_12BIT_US);
    float temp;
    ds18b20_read_temperature(&dev, &temp);
    ASSERT_EQ(ds18b20_model_get_reset_count(&model), 2);
    ASSERT_EQ(ds18b20_model_get_read_scratchpad_count(&model), 1);
}

TEST(test_no_device_on_bus)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_present(&model, 0);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_ERR_NO_DEVICE);
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_ERR_NO_DEVICE);
}

TEST(test_read_zero_celsius)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 0.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    convert_and_wait(&model, &dev);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.0f, 0.1f);
}

TEST(test_read_max_125c)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 125.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    convert_and_wait(&model, &dev);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 125.0f, 0.1f);
}

/* Table 1, p.4: temperature and digital output at 12 bits. */
TEST(test_table1_digital_output)
{
    static const struct {
        float    celsius;
        uint16_t raw;
    } rows[] = {
        { 125.0f,    0x07D0 }, {  85.0f,    0x0550 }, {  25.0625f, 0x0191 },
        {  10.125f,  0x00A2 }, {   0.5f,    0x0008 }, {   0.0f,    0x0000 },
        {  -0.5f,    0xFFF8 }, { -10.125f,  0xFF5E }, { -25.0625f, 0xFE6F },
        { -55.0f,    0xFC90 },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        ds18b20_model_t model;
        ds18b20_model_init(&model);
        ds18b20_model_set_temperature(&model, rows[i].celsius);
        ds18b20_ow_t ow = ds18b20_model_ow(&model);

        ds18b20_t dev;
        ds18b20_init(&dev, &ow);
        convert_and_wait(&model, &dev);

        uint8_t sp[9];
        read_frame(&model, sp);
        ASSERT_EQ_HEX(sp[0], rows[i].raw & 0xFFu);
        ASSERT_EQ_HEX(sp[1], rows[i].raw >> 8);

        float temp;
        ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
        ASSERT_NEAR(temp, rows[i].celsius, 0.0001f);
    }
}

/* Figure 7, p.7: bytes 0, 1, 5 and 7 have fixed power-up values, and byte 4
 * has R1 = R0 = 1 by default (p.8). */
TEST(test_power_up_scratchpad)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    uint8_t sp[9];
    read_frame(&model, sp);
    ASSERT_EQ_HEX(sp[0], 0x50);
    ASSERT_EQ_HEX(sp[1], 0x05);
    ASSERT_EQ_HEX(sp[4], 0x7F);
    ASSERT_EQ_HEX(sp[5], 0xFF);
    ASSERT_EQ_HEX(sp[7], 0x10);
    /* p.8: with the CRC shifted in too, "the shift register will contain
     * all 0s" */
    ASSERT_EQ_HEX(ds18b20_model_crc8(sp, 9), 0x00);
}

/* p.3: "To initiate a temperature measurement and A-to-D conversion, the
 * master must issue a Convert T [44h] command." */
TEST(test_no_new_temperature_without_convert_t)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 25.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 85.0f, 0.001f);
}

/* p.11: "transmitting a 0 while the temperature conversion is in progress
 * and a 1 when the conversion is done" */
TEST(test_read_slots_give_conversion_status)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);

    uint8_t slots = 0xA5;
    ASSERT_TRUE(ds18b20_model_read_byte(&model, &slots));
    ASSERT_EQ_HEX(slots, 0x00);

    ds18b20_model_advance_us(&model, TCONV_12BIT_US - 1u);
    ASSERT_TRUE(ds18b20_model_read_byte(&model, &slots));
    ASSERT_EQ_HEX(slots, 0x00);

    ds18b20_model_advance_us(&model, 1u);
    ASSERT_TRUE(ds18b20_model_read_byte(&model, &slots));
    ASSERT_EQ_HEX(slots, 0xFF);
}

/* p.11: the data is stored "Following the conversion", so a driver that
 * reads too early gets the old value. */
TEST(test_old_value_until_conversion_ends)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 25.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
    ds18b20_model_advance_us(&model, TCONV_12BIT_US - 1u);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 85.0f, 0.001f);

    ds18b20_model_advance_us(&model, 1u);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 25.0f, 0.001f);
}

/* Table 2, p.8: resolution sets the conversion time. */
TEST(test_conversion_time_follows_resolution)
{
    static const struct {
        ds18b20_resolution_t res;
        uint32_t             tconv_us;
    } rows[] = {
        { DS18B20_RES_9BIT,  TCONV_9BIT_US  },
        { DS18B20_RES_10BIT, TCONV_10BIT_US },
        { DS18B20_RES_11BIT, TCONV_11BIT_US },
        { DS18B20_RES_12BIT, TCONV_12BIT_US },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        ds18b20_model_t model;
        ds18b20_model_init(&model);
        ds18b20_ow_t ow = ds18b20_model_ow(&model);

        ds18b20_t dev;
        ds18b20_init(&dev, &ow);
        ASSERT_EQ(ds18b20_set_resolution(&dev, rows[i].res), DS18B20_OK);
        ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);

        uint8_t slots = 0xA5;
        ds18b20_model_advance_us(&model, rows[i].tconv_us - 1u);
        ASSERT_TRUE(ds18b20_model_read_byte(&model, &slots));
        ASSERT_EQ_HEX(slots, 0x00);

        ds18b20_model_advance_us(&model, 1u);
        ASSERT_TRUE(ds18b20_model_read_byte(&model, &slots));
        ASSERT_EQ_HEX(slots, 0xFF);
    }
}

/* p.10: "Only the slave that exactly matches the 64-bit ROM code sequence
 * will respond to the function command issued by the master" */
TEST(test_match_rom_right_code_gets_an_answer)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_model_set_temperature(&model, 25.0f);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);
    ASSERT_EQ_HEX(dev.rom[0], 0x28);

    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
    ASSERT_EQ(ds18b20_model_get_convert_count(&model), 1);
    ds18b20_model_advance_us(&model, TCONV_12BIT_US);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 25.0f, 0.001f);
}

/* p.10: "all other slaves on the bus will wait for a reset pulse" */
TEST(test_match_rom_wrong_code_stays_silent)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);
    dev.rom[3] = (uint8_t)(dev.rom[3] ^ 0x01u);

    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
    ASSERT_EQ(ds18b20_model_get_convert_count(&model), 0);

    /* nobody drives the bus, so the master reads the pullup: 9 x FFh, and
     * FFh is not the CRC of eight FFh */
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_ERR_CRC);
    ASSERT_EQ(ds18b20_model_get_read_scratchpad_count(&model), 0);
}

/* p.10: 55h is "followed by a 64-bit ROM code sequence", so a serial number
 * byte that looks like a command is not a command. */
TEST(test_match_rom_code_bytes_are_not_commands)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    model.rom[1] = 0x44;
    model.rom[7] = ds18b20_model_crc8(model.rom, 7);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 85.0f, 0.001f);
    ASSERT_EQ(ds18b20_model_get_convert_count(&model), 0);
}

/* p.11: "the second byte is written into the TL register (byte 3), and the
 * third byte is written into the configuration register (byte 4)." */
TEST(test_write_scratchpad_order)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    write_frame(&model, 0x11, 0x22, 0x3F);

    uint8_t sp[9];
    read_frame(&model, sp);
    ASSERT_EQ_HEX(sp[2], 0x11);
    ASSERT_EQ_HEX(sp[3], 0x22);
    ASSERT_EQ_HEX(sp[4], 0x3F);
    ASSERT_EQ_HEX(ds18b20_model_crc8(sp, 9), 0x00);
}

/* p.8: "Bit 7 and bits 0 to 4 in the configuration register are reserved for
 * internal use by the device and cannot be overwritten." */
TEST(test_config_reserved_bits_cannot_be_written)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    uint8_t sp[9];
    write_frame(&model, 0x00, 0x00, 0xFF);
    read_frame(&model, sp);
    ASSERT_EQ_HEX(sp[4], 0x7F);

    write_frame(&model, 0x00, 0x00, 0x00);
    read_frame(&model, sp);
    ASSERT_EQ_HEX(sp[4], 0x1F);
}

/* p.7: bytes 0 and 1 "are read-only", and bytes 5, 6, and 7 "cannot be
 * overwritten". */
TEST(test_write_scratchpad_reaches_three_bytes_only)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    uint8_t before[9];
    read_frame(&model, before);
    write_frame(&model, 0xAA, 0xBB, 0x5F);

    uint8_t after[9];
    read_frame(&model, after);
    ASSERT_EQ_HEX(after[0], before[0]);
    ASSERT_EQ_HEX(after[1], before[1]);
    ASSERT_EQ_HEX(after[5], before[5]);
    ASSERT_EQ_HEX(after[6], before[6]);
    ASSERT_EQ_HEX(after[7], before[7]);
}

/* Bits 6 and 5 of the third byte, Table 2 p.8. */
TEST(test_set_resolution_reaches_the_config_register)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);

    uint8_t sp[9];
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_10BIT), DS18B20_OK);
    read_frame(&model, sp);
    ASSERT_EQ_HEX(sp[4], 0x3F);

    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_9BIT), DS18B20_OK);
    read_frame(&model, sp);
    ASSERT_EQ_HEX(sp[4], 0x1F);
}

/* p.3: "For 11-bit resolution, bit 0 is undefined. For 10-bit resolution,
 * bits 1 and 0 are undefined, and for 9-bit resolution bits 2, 1, and 0 are
 * undefined." The model sets them, so a driver that keeps them reads high. */
TEST(test_reduced_resolution_has_undefined_low_bits)
{
    static const struct {
        ds18b20_resolution_t res;
        uint32_t             tconv_us;
        float                celsius;
        uint8_t              undefined;
    } rows[] = {
        { DS18B20_RES_9BIT,   TCONV_9BIT_US,  25.5f,   0x07 },
        { DS18B20_RES_9BIT,   TCONV_9BIT_US,  -0.5f,   0x07 },
        { DS18B20_RES_10BIT,  TCONV_10BIT_US, 25.25f,  0x03 },
        { DS18B20_RES_11BIT,  TCONV_11BIT_US, 25.125f, 0x01 },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        ds18b20_model_t model;
        ds18b20_model_init(&model);
        ds18b20_model_set_temperature(&model, rows[i].celsius);
        ds18b20_ow_t ow = ds18b20_model_ow(&model);

        ds18b20_t dev;
        ds18b20_init(&dev, &ow);
        ASSERT_EQ(ds18b20_set_resolution(&dev, rows[i].res), DS18B20_OK);
        ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
        ds18b20_model_advance_us(&model, rows[i].tconv_us);

        uint8_t sp[9];
        read_frame(&model, sp);
        ASSERT_EQ_HEX(sp[0] & rows[i].undefined, rows[i].undefined);

        float temp;
        ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
        ASSERT_NEAR(temp, rows[i].celsius, 0.0001f);
    }
}

/* p.7: "at power-up the EEPROM data is reloaded", and the driver has no Copy
 * Scratchpad, so its setting lasts until power-down. */
TEST(test_resolution_lasts_until_power_down)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_9BIT), DS18B20_OK);

    ds18b20_model_power_cycle(&model);

    uint8_t sp[9];
    read_frame(&model, sp);
    ASSERT_EQ_HEX(sp[0], 0x50);
    ASSERT_EQ_HEX(sp[1], 0x05);
    ASSERT_EQ_HEX(sp[4], 0x7F);
}

/* p.11: "All three bytes MUST be written before the master issues a reset,
 * or the data may be corrupted." */
TEST(test_reset_inside_write_scratchpad_is_counted)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    (void)ds18b20_model_reset(&model);
    (void)ds18b20_model_write_byte(&model, 0xCC);
    (void)ds18b20_model_write_byte(&model, 0x4E);
    (void)ds18b20_model_write_byte(&model, 0x11);
    (void)ds18b20_model_write_byte(&model, 0x22);
    (void)ds18b20_model_reset(&model);
    ASSERT_EQ(ds18b20_model_get_partial_write_count(&model), 1);
}

TEST(test_set_resolution_writes_all_three_bytes)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);
    ds18b20_ow_t ow = ds18b20_model_ow(&model);

    ds18b20_t dev;
    ds18b20_init(&dev, &ow);
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_11BIT), DS18B20_OK);

    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_EQ(ds18b20_model_get_partial_write_count(&model), 0);
}

/* p.10: "the DS18B20 will not respond if any steps in the sequence are
 * missing or out of order." */
TEST(test_function_command_needs_a_rom_command)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    (void)ds18b20_model_reset(&model);
    (void)ds18b20_model_write_byte(&model, 0xBE);
    /* a late ROM command does not repair the order */
    (void)ds18b20_model_write_byte(&model, 0xCC);
    (void)ds18b20_model_write_byte(&model, 0xBE);

    uint8_t byte = 0;
    ASSERT_TRUE(ds18b20_model_read_byte(&model, &byte));
    ASSERT_EQ_HEX(byte, 0xFF);
    ASSERT_EQ(ds18b20_model_get_read_scratchpad_count(&model), 0);
}

/* p.10: "All transactions on the 1-Wire bus begin with an initialization
 * sequence." */
TEST(test_rom_command_needs_a_reset)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    (void)ds18b20_model_write_byte(&model, 0xCC);
    (void)ds18b20_model_write_byte(&model, 0xBE);

    uint8_t byte = 0;
    ASSERT_TRUE(ds18b20_model_read_byte(&model, &byte));
    ASSERT_EQ_HEX(byte, 0xFF);
    ASSERT_EQ(ds18b20_model_get_read_scratchpad_count(&model), 0);
}

/* Figure 12, p.14: "RETURN TO INITIALIZATION SEQUENCE (FIGURE 11) FOR NEXT
 * TRANSACTION". */
TEST(test_next_transaction_needs_a_reset)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    write_frame(&model, 0x11, 0x22, 0x7F);
    (void)ds18b20_model_write_byte(&model, 0xCC);
    (void)ds18b20_model_write_byte(&model, 0xBE);

    uint8_t byte = 0;
    ASSERT_TRUE(ds18b20_model_read_byte(&model, &byte));
    ASSERT_EQ_HEX(byte, 0xFF);
    ASSERT_EQ(ds18b20_model_get_read_scratchpad_count(&model), 0);
}

/* Figure 12, p.14: "MASTER RX 1s" once the CRC byte has gone. */
TEST(test_reads_past_the_crc_give_ones)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    uint8_t sp[9];
    read_frame(&model, sp);

    uint8_t byte = 0;
    ASSERT_TRUE(ds18b20_model_read_byte(&model, &byte));
    ASSERT_EQ_HEX(byte, 0xFF);
}

/* p.10 step 3 and Figure 11, p.13: the ROM command is followed by its data
 * exchange, then the function command. */
TEST(test_function_command_follows_read_rom)
{
    ds18b20_model_t model;
    ds18b20_model_init(&model);

    (void)ds18b20_model_reset(&model);
    (void)ds18b20_model_write_byte(&model, 0x33);
    uint8_t rom[8];
    for (int i = 0; i < 8; i++)
        (void)ds18b20_model_read_byte(&model, &rom[i]);
    ASSERT_EQ_HEX(rom[0], 0x28);

    (void)ds18b20_model_write_byte(&model, 0xBE);
    uint8_t lsb = 0;
    ASSERT_TRUE(ds18b20_model_read_byte(&model, &lsb));
    ASSERT_EQ_HEX(lsb, 0x50);
}

TEST_MAIN("ds18b20_hw_model")
    RUN(test_read_25c);
    RUN(test_read_negative_10c);
    RUN(test_read_85c_power_on_default);
    RUN(test_crc_matches_scratchpad);
    RUN(test_reset_presence_pulse);
    RUN(test_no_device_on_bus);
    RUN(test_read_zero_celsius);
    RUN(test_read_max_125c);
    RUN(test_table1_digital_output);
    RUN(test_power_up_scratchpad);
    RUN(test_no_new_temperature_without_convert_t);
    RUN(test_read_slots_give_conversion_status);
    RUN(test_old_value_until_conversion_ends);
    RUN(test_conversion_time_follows_resolution);
    RUN(test_match_rom_right_code_gets_an_answer);
    RUN(test_match_rom_wrong_code_stays_silent);
    RUN(test_match_rom_code_bytes_are_not_commands);
    RUN(test_write_scratchpad_order);
    RUN(test_config_reserved_bits_cannot_be_written);
    RUN(test_write_scratchpad_reaches_three_bytes_only);
    RUN(test_set_resolution_reaches_the_config_register);
    RUN(test_reduced_resolution_has_undefined_low_bits);
    RUN(test_resolution_lasts_until_power_down);
    RUN(test_reset_inside_write_scratchpad_is_counted);
    RUN(test_set_resolution_writes_all_three_bytes);
    RUN(test_function_command_needs_a_rom_command);
    RUN(test_rom_command_needs_a_reset);
    RUN(test_next_transaction_needs_a_reset);
    RUN(test_reads_past_the_crc_give_ones);
    RUN(test_function_command_follows_read_rom);
TEST_MAIN_END()
