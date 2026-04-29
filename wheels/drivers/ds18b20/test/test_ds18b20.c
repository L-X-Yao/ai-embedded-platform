#include "ds18b20.h"
#include <stdio.h>
#include <string.h>

static int tests_pass = 0, tests_fail = 0;

#define TEST(name) static void name(void)
#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL %s:%d: %d != %d\n", __func__, __LINE__, \
               (int)(a), (int)(b)); \
        tests_fail++; return; \
    } } while(0)
#define ASSERT_NEAR(a, b, tol) do { \
    float _a = (a), _b = (b); \
    if ((_a - _b) > (tol) || (_b - _a) > (tol)) { \
        printf("  FAIL %s:%d: %.4f not near %.4f\n", __func__, __LINE__, \
               (double)_a, (double)_b); \
        tests_fail++; return; \
    } } while(0)
#define RUN(fn) do { int pf = tests_fail; fn(); if(tests_fail == pf) { \
    printf("  PASS %s\n", #fn); tests_pass++; } } while(0)

/* Mock 1-Wire bus */
static uint8_t mock_scratchpad[9];
static uint8_t mock_rom[8];
static int mock_read_idx;
static uint8_t mock_read_buf[32];
static int mock_read_buf_len;
static bool mock_presence;
static uint8_t mock_last_cmd;

static bool mock_reset(void *ctx)
{
    (void)ctx;
    mock_read_idx = 0;
    return mock_presence;
}

static void mock_write_byte(void *ctx, uint8_t byte)
{
    (void)ctx;
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
}

static uint8_t mock_read_byte(void *ctx)
{
    (void)ctx;
    if (mock_read_idx < mock_read_buf_len)
        return mock_read_buf[mock_read_idx++];
    return 0xFF;
}

static ds18b20_ow_t mock_ow = {
    .reset      = mock_reset,
    .write_byte = mock_write_byte,
    .read_byte  = mock_read_byte,
    .ctx        = NULL
};

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
    ds18b20_t dev;
    ds18b20_init(&dev, &mock_ow);
    ASSERT_EQ(ds18b20_start_conversion(&dev), DS18B20_OK);
}

int main(void)
{
    printf("ds18b20 tests:\n");
    RUN(test_init_null);
    RUN(test_read_25c);
    RUN(test_read_negative);
    RUN(test_read_85c_power_on_default);
    RUN(test_no_device);
    RUN(test_crc_failure);
    RUN(test_read_rom);
    RUN(test_crc8_known_vector);
    RUN(test_start_conversion);
    printf("\n%d passed, %d failed\n", tests_pass, tests_fail);
    return tests_fail ? 1 : 0;
}
