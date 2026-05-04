// SPDX-License-Identifier: MIT
/*
 * prop_dht11.c — property-based tests for DHT11 temp/humidity sensor
 *
 * DHT11 uses a GPIO bit-bang protocol with timing-sensitive reads.
 * We mock the pin to return random sequences — init+read must not crash.
 */

#include "test_harness.h"
#include "property_test.h"
#include "dht11.h"

static bool mock_pin_level;
static int  mock_read_count;

static void mock_set_output(void *ctx) { (void)ctx; }
static void mock_set_input(void *ctx)  { (void)ctx; }
static void mock_write_pin(void *ctx, bool level) { (void)ctx; mock_pin_level = level; }
static bool mock_read_pin(void *ctx) {
    (void)ctx;
    mock_read_count++;
    /* Toggle level to simulate some activity, preventing infinite loops */
    if (mock_read_count % 3 == 0) mock_pin_level = !mock_pin_level;
    return mock_pin_level;
}
static void mock_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static const dht11_gpio_t prop_gpio = {
    .set_output = mock_set_output,
    .set_input  = mock_set_input,
    .write_pin  = mock_write_pin,
    .read_pin   = mock_read_pin,
    .delay_us   = mock_delay_us,
    .ctx        = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_pin_level = true;
    mock_read_count = 0;
    dht11_t dev;
    dht11_init(&dev, &prop_gpio);
}

PROPERTY(read_no_crash, 5000)
{
    mock_pin_level = (ARBI_UINT8(seed) & 1) != 0;
    mock_read_count = 0;
    dht11_t dev;
    dht11_init(&dev, &prop_gpio);

    dht11_data_t data;
    dht11_read(&dev, &data);
    /* Read will likely fail (timeout or checksum) with mock — that is fine */
}

TEST_MAIN("prop_dht11")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
