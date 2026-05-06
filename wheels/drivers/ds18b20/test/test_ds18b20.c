// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ds18b20.h"
#include <string.h>

/* Mock 1-Wire bus */
static uint8_t mock_scratchpad[9];
static uint8_t mock_rom[8];
static int mock_read_idx;
static uint8_t mock_read_buf[32];
static int mock_read_buf_len;
static bool mock_presence;
static uint8_t mock_last_cmd;
static int mock_write_fail_after;
static int mock_read_fail_after;
static int mock_write_count;
static int mock_read_count;

static bool mock_reset(void *ctx)
{
    (void)ctx;
    mock_read_idx = 0;
    return mock_presence;
}

static bool mock_write_byte(void *ctx, uint8_t byte)
{
    (void)ctx;
    mock_write_count++;
    if (mock_write_fail_after > 0 && mock_write_count > mock_write_fail_after)
        return false;
    mock_last_cmd = byte;
    if (byte == 0x33) {
        memcpy(mock_read_buf, mock_rom, 8);
        mock_read_buf_len = 8;
        mock_read_idx = 0;
    } else if (byte == 0xBE) {
        memcpy(mock_read_buf, mock_scratchpad, 9);
        mock_read_buf_len = 9;
        mock_read_idx = 0;
    }
    return true;
}

static bool mock_read_byte(void *ctx, uint8_t *byte)
{
    (void)ctx;
    mock_read_count++;
    if (mock_read_fail_after > 0 && mock_read_count > mock_read_fail_after)
        return false;
    if (mock_read_idx < mock_read_buf_len)
        *byte = mock_read_buf[mock_read_idx++];
    else
        *byte = 0xFF;
    return true;
}

static ds18b20_ow_t mock_ow = {
    .reset      = mock_reset,
    .write_byte = mock_write_byte,
    .read_byte  = mock_read_byte,
    .ctx        = NULL
};

static void mock_bus_reset(void)
{
    mock_write_fail_after = 0;
    mock_read_fail_after = 0;
    mock_write_count = 0;
    mock_read_count = 0;
}

static void setup_scratchpad(int16_t raw_temp)
{
    memset(mock_scratchpad, 0, 9);
    mock_scratchpad[0] = (uint8_t)(raw_temp & 0xFF);
    mock_scratchpad[1] = (uint8_t)((uint16_t)raw_temp >> 8);
    mock_scratchpad[4] = 0x7F;
    mock_scratchpad[8] = ds18b20_crc8(mock_scratchpad, 8);
}

static void setup_rom(void)
{
    mock_rom[0] = 0x28;
    mock_rom[1] = 0xAA;
    mock_rom[2] = 0xBB;
    mock_rom[3] = 0xCC;
    mock_rom[4] = 0xDD;
    mock_rom[5] = 0xEE;
    mock_rom[6] = 0xFF;
    mock_rom[7] = ds18b20_crc8(mock_rom, 7);
}

TEST(test_init_null)
{
    ASSERT_EQ(ds18b20_init(NULL, &mock_ow), DS18B20_ERR_PARAM);
    ds18b20_t dev;
    ASSERT_EQ(ds18b20_init(&dev, NULL), DS18B20_ERR_PARAM);
}

TEST(test_read_25c)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x0190);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

TEST(test_read_negative)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad((int16_t)0xFF5E);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, -10.125f, 0.01f);
}

TEST(test_read_85c_power_on_default)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x0550);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 85.0f, 0.01f);
}

TEST(test_no_device)
{
    mock_presence = false;
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_ERR_NO_DEVICE);
}

TEST(test_crc_failure)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x0190);
    mock_scratchpad[8] = 0x00;
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_ERR_CRC);
}

TEST(test_read_rom)
{
    mock_presence = true;
    mock_bus_reset();
    setup_rom();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);
    ASSERT_EQ(dev.rom[0], 0x28);
    ASSERT_EQ(dev.use_rom, 1);
}

TEST(test_crc8_known_vector)
{
    uint8_t data[] = {0x28, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t crc = ds18b20_crc8(data, 7);
    setup_rom();
    ASSERT_EQ(crc, mock_rom[7]);
}

TEST(test_start_conversion)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
}

TEST(test_set_resolution)
{
    ds18b20_t dev;
    mock_presence = true;
    mock_bus_reset();
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_9BIT), DS18B20_OK);
    ASSERT_EQ(ds18b20_set_resolution(NULL, DS18B20_RES_12BIT), DS18B20_ERR_PARAM);
    mock_presence = false;
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_12BIT), DS18B20_ERR_NO_DEVICE);
}

TEST(test_bus_error_write)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x0190);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    mock_write_fail_after = 1;
    mock_write_count = 0;
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_ERR_BUS);
}

TEST(test_bus_error_read)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x0190);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    mock_read_fail_after = 1;
    mock_read_count = 0;
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_ERR_BUS);
}

TEST(test_bus_error_start_conversion)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    mock_write_fail_after = 1;
    mock_write_count = 0;
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_ERR_BUS);
}

TEST(test_bus_error_read_rom)
{
    mock_presence = true;
    mock_bus_reset();
    setup_rom();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    mock_read_fail_after = 1;
    mock_read_count = 0;
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_ERR_BUS);
}

/* --- null-param guards not reached by prior tests --- */

TEST(test_read_rom_null)
{
    ASSERT_EQ(ds18b20_read_rom(NULL), DS18B20_ERR_PARAM);
}

TEST(test_start_conversion_null)
{
    ASSERT_EQ(ds18b20_start_conversion(NULL), DS18B20_ERR_PARAM);
}

TEST(test_read_temperature_null_dev)
{
    float t;
    ASSERT_EQ(ds18b20_read_temperature(NULL, &t), DS18B20_ERR_PARAM);
}

TEST(test_read_temperature_null_temp)
{
    ds18b20_t dev;
    mock_presence = true;
    mock_bus_reset();
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_read_temperature(&dev, NULL), DS18B20_ERR_PARAM);
}

/* --- no_device on read_rom and start_conversion --- */

TEST(test_read_rom_no_device)
{
    mock_presence = false;
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_ERR_NO_DEVICE);
}

TEST(test_start_conversion_no_device)
{
    mock_presence = false;
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_ERR_NO_DEVICE);
}

/* --- read_rom CRC failure --- */

TEST(test_read_rom_crc_fail)
{
    mock_presence = true;
    mock_bus_reset();
    setup_rom();
    /* corrupt the CRC byte */
    mock_rom[7] = (uint8_t)(mock_rom[7] ^ 0xFF);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_ERR_CRC);
}

/* --- read_rom bus error on write_byte (CMD_READ_ROM, line 82).
 * Pre-seed mock_write_count so the first write triggers the fail check:
 * condition is (fail_after > 0 && count > fail_after).
 * Set fail_after=1 and count=1 before the call; the write increments
 * count to 2, check is (1 > 0 && 2 > 1) = true → fail. */
TEST(test_read_rom_bus_error_write)
{
    mock_presence = true;
    mock_bus_reset();
    setup_rom();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    mock_write_fail_after = 1;
    mock_write_count = 1;  /* pre-seed so first write triggers the check */
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_ERR_BUS);
}

/* --- address_device with use_rom=true (CMD_MATCH_ROM path, lines 51-55) --- */

TEST(test_start_conversion_with_rom)
{
    mock_presence = true;
    mock_bus_reset();
    setup_rom();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    /* First read the ROM so use_rom=true is set */
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);
    ASSERT_TRUE(dev.use_rom);

    /* Now start_conversion will call address_device with use_rom=true */
    mock_bus_reset();
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
}

TEST(test_read_temperature_with_rom)
{
    mock_presence = true;
    mock_bus_reset();
    setup_rom();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);

    /* set up scratchpad for a read */
    mock_bus_reset();
    setup_scratchpad(0x0190);  /* 25.0°C */
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

/* --- bus error in address_device when skip_rom fails (line 58).
 * Pre-seed write_count=1, fail_after=1: first write increments to 2 → fails. */

TEST(test_start_conversion_bus_error_skip_rom)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    mock_write_fail_after = 1;
    mock_write_count = 1;
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_ERR_BUS);
}

TEST(test_read_temperature_bus_error_skip_rom)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x0190);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    mock_write_fail_after = 1;
    mock_write_count = 1;
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_ERR_BUS);
}

/* --- bus error in address_device with use_rom (CMD_MATCH_ROM fails, line 52).
 * Pre-seed write_count so CMD_MATCH_ROM is the triggering write. */

TEST(test_address_device_match_rom_fail)
{
    mock_presence = true;
    mock_bus_reset();
    setup_rom();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);

    mock_bus_reset();
    mock_write_fail_after = 1;
    mock_write_count = 1;
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_ERR_BUS);
}

/* --- bus error in address_device: ROM byte write fails (line 54-55) --- */

TEST(test_address_device_rom_byte_fail)
{
    mock_presence = true;
    mock_bus_reset();
    setup_rom();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);

    mock_bus_reset();
    /* Let CMD_MATCH_ROM succeed (write 1), fail on second write (first ROM byte) */
    mock_write_fail_after = 1;
    mock_write_count = 0;
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_ERR_BUS);
}

/* --- set_resolution error paths (lines 138-146) --- */

TEST(test_set_resolution_bus_errors)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);

    /* Fail write on CMD_WRITE_SCRATCHPAD (2nd write after skip_rom) */
    mock_write_fail_after = 1;
    mock_write_count = 0;
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_12BIT), DS18B20_ERR_BUS);

    /* Fail on first 0x00 data byte */
    mock_bus_reset();
    mock_write_fail_after = 2;
    mock_write_count = 0;
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_12BIT), DS18B20_ERR_BUS);

    /* Fail on second 0x00 data byte */
    mock_bus_reset();
    mock_write_fail_after = 3;
    mock_write_count = 0;
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_12BIT), DS18B20_ERR_BUS);

    /* Fail on resolution byte */
    mock_bus_reset();
    mock_write_fail_after = 4;
    mock_write_count = 0;
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_12BIT), DS18B20_ERR_BUS);
}

/* --- set_resolution address_device failure (line 138) --- */

TEST(test_set_resolution_address_fail)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    /* Pre-seed count so first write (CMD_SKIP_ROM in address_device) fails */
    mock_write_fail_after = 1;
    mock_write_count = 1;
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_12BIT), DS18B20_ERR_BUS);
}

/* --- ds18b20_init with partial ow struct (null function pointer, line 68) --- */

TEST(test_init_null_fn_ptr)
{
    ds18b20_t dev;
    ds18b20_ow_t bad_ow = { .reset = NULL, .write_byte = mock_write_byte,
                            .read_byte = mock_read_byte, .ctx = NULL };
    ASSERT_EQ(ds18b20_init(&dev, &bad_ow), DS18B20_ERR_PARAM);

    bad_ow.reset = mock_reset;
    bad_ow.write_byte = NULL;
    ASSERT_EQ(ds18b20_init(&dev, &bad_ow), DS18B20_ERR_PARAM);
}

/* --- all resolution values --- */

TEST(test_set_resolution_all_values)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);

    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_9BIT),  DS18B20_OK);
    mock_bus_reset();
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_10BIT), DS18B20_OK);
    mock_bus_reset();
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_11BIT), DS18B20_OK);
    mock_bus_reset();
    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_12BIT), DS18B20_OK);
}

/* --- temperature extremes --- */

TEST(test_read_min_temperature)
{
    mock_presence = true;
    mock_bus_reset();
    /* -55°C = raw 0xFC90 */
    setup_scratchpad((int16_t)0xFC90);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, -55.0f, 0.1f);
}

TEST(test_read_zero_celsius)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x0000);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

TEST_MAIN("ds18b20")
    RUN(test_init_null);
    RUN(test_read_25c);
    RUN(test_read_negative);
    RUN(test_read_85c_power_on_default);
    RUN(test_no_device);
    RUN(test_crc_failure);
    RUN(test_read_rom);
    RUN(test_crc8_known_vector);
    RUN(test_start_conversion);
    RUN(test_set_resolution);
    RUN(test_bus_error_write);
    RUN(test_bus_error_read);
    RUN(test_bus_error_start_conversion);
    RUN(test_bus_error_read_rom);
    RUN(test_read_rom_null);
    RUN(test_start_conversion_null);
    RUN(test_read_temperature_null_dev);
    RUN(test_read_temperature_null_temp);
    RUN(test_read_rom_no_device);
    RUN(test_start_conversion_no_device);
    RUN(test_read_rom_crc_fail);
    RUN(test_read_rom_bus_error_write);
    RUN(test_start_conversion_with_rom);
    RUN(test_read_temperature_with_rom);
    RUN(test_start_conversion_bus_error_skip_rom);
    RUN(test_read_temperature_bus_error_skip_rom);
    RUN(test_address_device_match_rom_fail);
    RUN(test_address_device_rom_byte_fail);
    RUN(test_set_resolution_address_fail);
    RUN(test_init_null_fn_ptr);
    RUN(test_set_resolution_bus_errors);
    RUN(test_set_resolution_all_values);
    RUN(test_read_min_temperature);
    RUN(test_read_zero_celsius);
TEST_MAIN_END()
