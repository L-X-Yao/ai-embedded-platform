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
static int mock_write_scratchpad_left;

/*
 * Reference CRC, written from the manual so that it shares nothing with the
 * driver. DS18B20 REV 042208 p.8: CRC = X8 + X5 + X4 + 1, shift register
 * bits initialized to 0, least significant bit shifted in first.
 */
static uint8_t ref_crc8(const uint8_t *data, size_t len)
{
    uint8_t sr = 0;
    for (size_t i = 0; i < len; i++) {
        for (unsigned bit = 0; bit < 8u; bit++) {
            unsigned in = ((unsigned)data[i] >> bit) & 1u;
            unsigned feedback = (sr ^ in) & 1u;
            sr = (uint8_t)(sr >> 1);
            if (feedback)
                sr = (uint8_t)(sr ^ 0x8Cu);
        }
    }
    return sr;
}

static bool mock_reset(void *ctx)
{
    (void)ctx;
    mock_read_idx = 0;
    mock_write_scratchpad_left = 0;
    return mock_presence;
}

/* p.11: TH goes to byte 2, TL to byte 3, the configuration to byte 4, where
 * only R1 and R0 can change (p.8). */
static void mock_write_scratchpad(uint8_t byte)
{
    int slot = 5 - mock_write_scratchpad_left;
    mock_scratchpad[slot] =
        (slot == 4) ? (uint8_t)((byte & 0x60u) | 0x1Fu) : byte;
    mock_scratchpad[8] = ref_crc8(mock_scratchpad, 8);
    mock_write_scratchpad_left--;
}

static bool mock_write_byte(void *ctx, uint8_t byte)
{
    (void)ctx;
    mock_write_count++;
    if (mock_write_fail_after > 0 && mock_write_count > mock_write_fail_after)
        return false;
    mock_last_cmd = byte;
    if (mock_write_scratchpad_left > 0) {
        mock_write_scratchpad(byte);
    } else if (byte == 0x4E) {
        mock_write_scratchpad_left = 3;
    } else if (byte == 0x33) {
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
    mock_write_scratchpad_left = 0;
}

static void setup_scratchpad(int16_t raw_temp)
{
    memset(mock_scratchpad, 0, 9);
    mock_scratchpad[0] = (uint8_t)(raw_temp & 0xFF);
    mock_scratchpad[1] = (uint8_t)((uint16_t)raw_temp >> 8);
    mock_scratchpad[4] = 0x7F;
    mock_scratchpad[5] = 0xFF;
    mock_scratchpad[7] = 0x10;
    mock_scratchpad[8] = ref_crc8(mock_scratchpad, 8);
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
    mock_rom[7] = ref_crc8(mock_rom, 7);
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

/* The worked example of Maxim Application Note 27. */
TEST(test_crc8_an27_example)
{
    const uint8_t rom[7] = {0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00};
    ASSERT_EQ_HEX(ds18b20_crc8(rom, 7), 0xA2);
}

/* p.8: once the CRC is shifted in too, "the shift register will contain
 * all 0s". */
TEST(test_crc8_good_frame_leaves_zero)
{
    const uint8_t rom[8] = {0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xA2};
    ASSERT_EQ_HEX(ds18b20_crc8(rom, 8), 0x00);
}

TEST(test_crc8_every_byte_follows_polynomial)
{
    for (unsigned v = 0; v < 256u; v++) {
        uint8_t b = (uint8_t)v;
        ASSERT_EQ_HEX(ds18b20_crc8(&b, 1), ref_crc8(&b, 1));
    }
}

TEST(test_read_rom_accepts_an27_code)
{
    const uint8_t rom[8] = {0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xA2};
    mock_presence = true;
    mock_bus_reset();
    memcpy(mock_rom, rom, 8);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_read_rom(&dev), DS18B20_OK);
    ASSERT_TRUE(dev.use_rom);
}

/* p.7 power-up state: 50h 05h, TH, TL and config from EEPROM, FFh, reserved,
 * 10h. The CRC byte of this frame comes from the polynomial. */
TEST(test_read_power_up_frame)
{
    const uint8_t frame[9] = {0x50, 0x05, 0x4B, 0x46, 0x7F,
                              0xFF, 0x0C, 0x10, 0x1C};
    mock_presence = true;
    mock_bus_reset();
    memcpy(mock_scratchpad, frame, 9);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 85.0f, 0.01f);
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

/* --- reduced resolution ---
 * p.3: "For 11-bit resolution, bit 0 is undefined. For 10-bit resolution,
 * bits 1 and 0 are undefined, and for 9-bit resolution bits 2, 1, and 0 are
 * undefined." Each frame below has those bits set. Table 1 gives the rest:
 * 0008h is +0.5 and FFF8h is -0.5. */

static void setup_scratchpad_config(uint16_t raw, uint8_t config)
{
    setup_scratchpad((int16_t)raw);
    mock_scratchpad[4] = config;
    mock_scratchpad[8] = ref_crc8(mock_scratchpad, 8);
}

TEST(test_9bit_drops_undefined_bits)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;

    setup_scratchpad_config(0x000F, 0x1F);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.5f, 0.001f);

    setup_scratchpad_config(0xFFFF, 0x1F);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, -0.5f, 0.001f);
}

TEST(test_10bit_drops_undefined_bits)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;

    setup_scratchpad_config(0x000B, 0x3F);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.5f, 0.001f);

    /* bit 2 is defined at 10 bits and worth 0.25 */
    setup_scratchpad_config(0x0007, 0x3F);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.25f, 0.001f);
}

TEST(test_11bit_drops_undefined_bit)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;

    setup_scratchpad_config(0x0009, 0x5F);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.5f, 0.001f);

    /* bit 1 is defined at 11 bits and worth 0.125 */
    setup_scratchpad_config(0x0003, 0x5F);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.125f, 0.001f);
}

TEST(test_12bit_keeps_every_bit)
{
    mock_presence = true;
    mock_bus_reset();
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    float temp;

    setup_scratchpad_config(0x0191, 0x7F);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 25.0625f, 0.001f);

    setup_scratchpad_config(0xFE6F, 0x7F);
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, -25.0625f, 0.001f);
}

/* Figure 7, p.7: "Byte 5 Reserved (FFh) Byte 6 Reserved Byte 7 Reserved
 * (10h)". The frames the tests feed the driver must look like the chip's. */
TEST(test_mock_frame_follows_figure_7)
{
    setup_scratchpad(0x0190);
    ASSERT_EQ_HEX(mock_scratchpad[5], 0xFF);
    ASSERT_EQ_HEX(mock_scratchpad[7], 0x10);
}

/* p.11: "the third byte is written into the configuration register (byte
 * 4)". */
TEST(test_set_resolution_reaches_config_byte)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x0000);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);

    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_10BIT), DS18B20_OK);
    ASSERT_EQ_HEX(mock_scratchpad[4], 0x3F);
    ASSERT_EQ_HEX(ref_crc8(mock_scratchpad, 9), 0x00);
}

/* The resolution set through the driver decides which bits count, with no
 * test writing the configuration byte by hand: 000Fh at 9 bits is +0.5. */
TEST(test_set_resolution_changes_the_reading)
{
    mock_presence = true;
    mock_bus_reset();
    setup_scratchpad(0x000F);
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);

    ASSERT_EQ(ds18b20_set_resolution(&dev, DS18B20_RES_9BIT), DS18B20_OK);
    float temp;
    ASSERT_EQ(ds18b20_read_temperature(&dev, &temp), DS18B20_OK);
    ASSERT_NEAR(temp, 0.5f, 0.001f);
}

TEST_MAIN("ds18b20")
    RUN(test_mock_frame_follows_figure_7);
    RUN(test_set_resolution_reaches_config_byte);
    RUN(test_set_resolution_changes_the_reading);
    RUN(test_init_null);
    RUN(test_read_25c);
    RUN(test_read_negative);
    RUN(test_read_85c_power_on_default);
    RUN(test_no_device);
    RUN(test_crc_failure);
    RUN(test_read_rom);
    RUN(test_crc8_an27_example);
    RUN(test_crc8_good_frame_leaves_zero);
    RUN(test_crc8_every_byte_follows_polynomial);
    RUN(test_read_rom_accepts_an27_code);
    RUN(test_read_power_up_frame);
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
    RUN(test_9bit_drops_undefined_bits);
    RUN(test_10bit_drops_undefined_bits);
    RUN(test_11bit_drops_undefined_bit);
    RUN(test_12bit_keeps_every_bit);
TEST_MAIN_END()
