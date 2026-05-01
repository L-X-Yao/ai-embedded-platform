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
TEST_MAIN_END()
