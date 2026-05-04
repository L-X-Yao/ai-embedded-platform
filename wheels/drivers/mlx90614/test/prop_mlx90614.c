// SPDX-License-Identifier: MIT
/*
 * prop_mlx90614.c — property-based tests for MLX90614 IR temperature sensor
 *
 * MLX90614 uses SMBus command-based I2C (write/read), not register-based.
 */

#include "test_harness.h"
#include "property_test.h"
#include "mlx90614.h"

/* --- command-based I2C mock --- */

static uint8_t mock_response[4];
static int     mock_fail_next;

static bool mock_write(void *ctx, uint8_t dev_addr,
                       const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)data; (void)len;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    return true;
}

static bool mock_read(void *ctx, uint8_t dev_addr,
                      uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    for (uint16_t i = 0; i < len; i++)
        data[i] = (i < sizeof(mock_response)) ? mock_response[i] : 0x00;
    return true;
}

static void mock_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static const mlx90614_i2c_t prop_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

static void mock_reset(void)
{
    memset(mock_response, 0, sizeof(mock_response));
    mock_fail_next = 0;
}

PROPERTY(init_no_crash, 10000)
{
    mock_reset();
    mlx90614_t dev;
    mlx90614_cfg_t cfg = { .addr = MLX90614_ADDR_DEFAULT };
    mlx90614_init(&dev, &prop_i2c, &cfg);
}

PROPERTY(read_no_crash, 5000)
{
    mock_reset();
    mock_response[0] = ARBI_UINT8(seed); /* LSB */
    mock_response[1] = ARBI_UINT8(seed); /* MSB */
    mock_response[2] = ARBI_UINT8(seed); /* PEC */

    mlx90614_t dev;
    mlx90614_cfg_t cfg = { .addr = MLX90614_ADDR_DEFAULT };
    mlx90614_init(&dev, &prop_i2c, &cfg);

    float obj_temp, amb_temp;
    mlx90614_read_object_temp(&dev, &obj_temp);
    mlx90614_read_ambient_temp(&dev, &amb_temp);
}

TEST_MAIN("prop_mlx90614")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
