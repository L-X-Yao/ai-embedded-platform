// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "dht11.h"
#include <string.h>

static bool pin_seq[256];
static int  pin_seq_idx;
static int  pin_seq_len;

static void mock_set_output(void *ctx) { (void)ctx; }
static void mock_set_input(void *ctx) { (void)ctx; }
static uint32_t mock_first_delay_us;
static int      mock_high_writes;

static void mock_write_pin(void *ctx, bool level)
{
    (void)ctx;
    if (level)
        mock_high_writes++;
}

static void mock_delay_us(void *ctx, uint32_t us)
{
    (void)ctx;
    if (mock_first_delay_us == 0)
        mock_first_delay_us = us;
}

static bool mock_read_pin(void *ctx)
{
    (void)ctx;
    if (pin_seq_idx < pin_seq_len)
        return pin_seq[pin_seq_idx++];
    return false;
}

static void build_pin_sequence(uint8_t bytes[5])
{
    pin_seq_idx = 0;
    pin_seq_len = 0;
    mock_first_delay_us = 0;
    mock_high_writes = 0;
    pin_seq[pin_seq_len++] = false;
    pin_seq[pin_seq_len++] = true;
    pin_seq[pin_seq_len++] = false;

    for (int i = 0; i < 40; i++) {
        int byte_n = i / 8;
        int bit_n  = 7 - (i % 8);
        bool bit_val = (bytes[byte_n] >> bit_n) & 1;
        pin_seq[pin_seq_len++] = true;
        pin_seq[pin_seq_len++] = bit_val;
        if (bit_val)
            pin_seq[pin_seq_len++] = false;
    }
}

static dht11_gpio_t mock_gpio = {
    .set_output = mock_set_output,
    .set_input  = mock_set_input,
    .write_pin  = mock_write_pin,
    .read_pin   = mock_read_pin,
    .delay_us   = mock_delay_us,
    .ctx        = NULL
};

static void setup_mock(uint8_t hum_i, uint8_t hum_d,
                       uint8_t tmp_i, uint8_t tmp_d, bool good_crc)
{
    uint8_t bytes[5];
    bytes[0] = hum_i;
    bytes[1] = hum_d;
    bytes[2] = tmp_i;
    bytes[3] = tmp_d;
    bytes[4] = good_crc ? (uint8_t)(hum_i + hum_d + tmp_i + tmp_d) : 0x00;
    build_pin_sequence(bytes);
}

TEST(test_init_null)
{
    ASSERT_EQ(dht11_init(NULL, &mock_gpio), DHT11_ERR_PARAM);
    dht11_t dev;
    ASSERT_EQ(dht11_init(&dev, NULL), DHT11_ERR_PARAM);
}

TEST(test_init_missing_callback)
{
    dht11_t dev;
    dht11_gpio_t bad = mock_gpio;
    bad.read_pin = NULL;
    ASSERT_EQ(dht11_init(&dev, &bad), DHT11_ERR_PARAM);
}

TEST(test_read_normal)
{
    setup_mock(45, 0, 25, 3, true);
    dht11_t dev;
    ASSERT_EQ(dht11_init(&dev, &mock_gpio), DHT11_OK);
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(data.humidity_int, 45);
    ASSERT_EQ(data.humidity_dec, 0);
    ASSERT_EQ(data.temperature_int, 25);
    ASSERT_EQ(data.temperature_dec, 3);
}

TEST(test_read_high_humidity)
{
    setup_mock(90, 0, 30, 0, true);
    dht11_t dev;
    ASSERT_EQ(dht11_init(&dev, &mock_gpio), DHT11_OK);
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(data.humidity_int, 90);
    ASSERT_EQ(data.temperature_int, 30);
}

TEST(test_read_zero)
{
    setup_mock(0, 0, 0, 0, true);
    dht11_t dev;
    ASSERT_EQ(dht11_init(&dev, &mock_gpio), DHT11_OK);
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(data.humidity_int, 0);
    ASSERT_EQ(data.temperature_int, 0);
}

TEST(test_checksum_fail)
{
    setup_mock(45, 0, 25, 3, false);
    dht11_t dev;
    ASSERT_EQ(dht11_init(&dev, &mock_gpio), DHT11_OK);
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_ERR_CHECKSUM);
}

TEST(test_read_null)
{
    dht11_t dev;
    ASSERT_EQ(dht11_init(&dev, &mock_gpio), DHT11_OK);
    ASSERT_EQ(dht11_read(&dev, NULL), DHT11_ERR_PARAM);
    ASSERT_EQ(dht11_read(NULL, NULL), DHT11_ERR_PARAM);
}

TEST(test_all_ones)
{
    setup_mock(0xFF, 0xFF, 0xFF, 0xFF, true);
    dht11_t dev;
    ASSERT_EQ(dht11_init(&dev, &mock_gpio), DHT11_OK);
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_EQ(data.humidity_int, 0xFF);
    ASSERT_EQ(data.temperature_int, 0xFF);
}

/* Pin stuck high: sensor never responds, first wait_pin(false) times out */
static bool mock_read_pin_stuck_high(void *ctx)
{
    (void)ctx;
    return true;
}

TEST(test_read_timeout)
{
    dht11_t dev;
    dht11_gpio_t timeout_gpio = mock_gpio;
    timeout_gpio.read_pin = mock_read_pin_stuck_high;
    ASSERT_EQ(dht11_init(&dev, &timeout_gpio), DHT11_OK);
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_ERR_TIMEOUT);
}

/* p.6: the low start signal is ">18ms" in the figure, and after it the host is
 * "set to enter the state": it goes to input, and the pull-up raises the line. */
TEST(test_start_signal_is_long_enough_and_releases_the_bus)
{
    setup_mock(50, 0, 25, 0, true);
    dht11_t dev;
    dht11_init(&dev, &mock_gpio);
    dht11_data_t data;
    ASSERT_EQ(dht11_read(&dev, &data), DHT11_OK);
    ASSERT_GT(mock_first_delay_us, 18000u);
    ASSERT_EQ(mock_high_writes, 0);
}

/* ---------- a device init refused ---------- */

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const dht11_gpio_t no_callbacks;
    dht11_t d;
    dht11_data_t data;

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(dht11_init(&d, NULL), DHT11_ERR_PARAM);
    ASSERT_EQ(dht11_read(&d, &data), DHT11_ERR_PARAM);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(dht11_init(&d, &no_callbacks), DHT11_ERR_PARAM);
    ASSERT_EQ(dht11_read(&d, &data), DHT11_ERR_PARAM);
}

TEST_MAIN("dht11")
    RUN(test_init_null);
    RUN(test_init_missing_callback);
    RUN(test_read_normal);
    RUN(test_start_signal_is_long_enough_and_releases_the_bus);
    RUN(test_read_high_humidity);
    RUN(test_read_zero);
    RUN(test_checksum_fail);
    RUN(test_read_null);
    RUN(test_all_ones);
    RUN(test_read_timeout);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
