// SPDX-License-Identifier: MIT
/*
 * dht11 — DHT11 temperature and humidity sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * Single-wire protocol: host pulls low 18ms, sensor responds with 40 bits.
 * HAL callbacks provide pin control and microsecond timing.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the dht11_t struct.
 */

#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DHT11_OK           =  0,
    DHT11_ERR_PARAM    = -1,
    DHT11_ERR_TIMEOUT  = -2,
    DHT11_ERR_CHECKSUM = -3,
} dht11_err_t;

typedef struct {
    void (*set_output)(void *ctx);
    void (*set_input)(void *ctx);
    void (*write_pin)(void *ctx, bool level);
    bool (*read_pin)(void *ctx);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} dht11_gpio_t;

typedef struct {
    const dht11_gpio_t *gpio;
} dht11_t;

typedef struct {
    uint8_t humidity_int;     /* relative humidity integer part (%RH) */
    uint8_t humidity_dec;     /* relative humidity decimal part */
    uint8_t temperature_int;  /* temperature integer part (C) */
    uint8_t temperature_dec;  /* temperature decimal part */
} dht11_data_t;

/** @thread_safe: no */
dht11_err_t dht11_init(dht11_t *dev, const dht11_gpio_t *gpio);

/** Read temperature and humidity from sensor. @thread_safe: no */
dht11_err_t dht11_read(dht11_t *dev, dht11_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */
