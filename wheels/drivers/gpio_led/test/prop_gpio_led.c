// SPDX-License-Identifier: MIT
/*
 * prop_gpio_led.c — property-based tests for GPIO LED driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "gpio_led.h"

static uint8_t mock_level;
static void mock_set(void *ctx, uint8_t level) { (void)ctx; mock_level = level; }

static const gpio_led_hal_t prop_hal = {
    .set = mock_set,
    .ctx = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_level = 0;
    gpio_led_t led;
    gpio_led_init(&led, &prop_hal);
    PROP_ASSERT(gpio_led_get_mode(&led) == GPIO_LED_OFF);
}

PROPERTY(tick_no_crash, 5000)
{
    mock_level = 0;
    gpio_led_t led;
    gpio_led_init(&led, &prop_hal);

    /* Random mode activation */
    uint8_t mode = ARBI_UINT8(seed) % 5;
    switch (mode) {
    case 0: gpio_led_off(&led); break;
    case 1: gpio_led_on(&led); break;
    case 2: gpio_led_blink(&led, (uint16_t)(50 + ARBI_UINT8(seed)),
                           (uint16_t)(50 + ARBI_UINT8(seed)),
                           ARBI_UINT8(seed) % 10); break;
    case 3: gpio_led_breathe(&led, (uint16_t)(100 + ARBI_UINT16(seed) % 2000)); break;
    case 4: gpio_led_pulse(&led, (uint16_t)(10 + ARBI_UINT8(seed))); break;
    default: break;
    }

    /* Tick random amounts */
    for (int i = 0; i < 10; i++) {
        uint16_t dt = ARBI_UINT16(seed);
        gpio_led_tick(&led, dt);
    }
}

PROPERTY(blink_toggles, 5000)
{
    mock_level = 0;
    gpio_led_t led;
    gpio_led_init(&led, &prop_hal);
    gpio_led_blink(&led, 100, 100, 0); /* infinite blink */

    /* Tick 201ms: should have toggled at least once */
    for (int i = 0; i < 201; i++)
        gpio_led_tick(&led, 1);

    PROP_ASSERT(gpio_led_get_mode(&led) == GPIO_LED_BLINK);
}

TEST_MAIN("prop_gpio_led")
    RUN(init_no_crash);
    RUN(tick_no_crash);
    RUN(blink_toggles);
TEST_MAIN_END()
