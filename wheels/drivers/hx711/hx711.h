// SPDX-License-Identifier: MIT
/*
 * hx711 — GPIO-based 24-bit ADC driver for load cells
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 * Thread safety: caller owns the hx711_t struct.
 */

#ifndef HX711_H
#define HX711_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HX711_OK            =  0,
    HX711_ERR_PARAM    = -1,
    HX711_ERR_TIMEOUT   = -2,
    HX711_ERR_NOT_READY = -3,
} hx711_err_t;

typedef enum {
    HX711_GAIN_128 = 1,    /* Channel A, gain 128 (25 pulses) */
    HX711_GAIN_32  = 2,    /* Channel B, gain 32  (26 pulses) */
    HX711_GAIN_64  = 3,    /* Channel A, gain 64  (27 pulses) */
} hx711_gain_t;

typedef struct {
    bool (*read_dout)(void *ctx);
    void (*set_sck)(void *ctx, bool level);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} hx711_hal_t;

typedef struct {
    const hx711_hal_t *hal;
    hx711_gain_t       gain;
    int32_t            offset;  /* tare offset */
    float              scale;   /* raw-to-unit conversion factor */
} hx711_t;

/** Initialize device. @thread_safe: no @isr_safe: no */
hx711_err_t hx711_init(hx711_t *dev, const hx711_hal_t *hal, hx711_gain_t gain);

/** Check if data is ready (DOUT low). @thread_safe: no @isr_safe: no */
bool hx711_is_ready(const hx711_t *dev);

/** Read raw 24-bit signed value. @thread_safe: no @isr_safe: no */
hx711_err_t hx711_read_raw(hx711_t *dev, int32_t *value);

/** Read with offset and scale applied: (raw - offset) / scale. @thread_safe: no @isr_safe: yes */
hx711_err_t hx711_read(hx711_t *dev, float *value);

/** Average N readings and store as tare offset. @thread_safe: no @isr_safe: yes */
hx711_err_t hx711_tare(hx711_t *dev, uint8_t samples);

/** Set scale factor for raw-to-unit conversion. @thread_safe: no @isr_safe: yes */
void hx711_set_scale(hx711_t *dev, float scale);

/** Set tare offset directly. @thread_safe: no @isr_safe: yes */
void hx711_set_offset(hx711_t *dev, int32_t offset);

/** Power down (SCK high >60us). @thread_safe: no @isr_safe: no */
void hx711_power_down(hx711_t *dev);

/** Power up (SCK low). @thread_safe: no @isr_safe: no */
void hx711_power_up(hx711_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* HX711_H */
