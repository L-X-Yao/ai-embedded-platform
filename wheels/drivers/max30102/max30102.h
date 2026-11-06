// SPDX-License-Identifier: MIT
/*
 * max30102 -- Maxim MAX30102 pulse oximeter and heart rate sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Supports heart-rate-only and SpO2
 * modes with configurable LED currents and sample rates.  FIFO readout
 * returns 18-bit ADC samples (RED + IR in SpO2 mode).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- error codes --- */

typedef enum {
    MAX30102_OK          =  0,
    MAX30102_ERR_I2C     = -1,
    MAX30102_ERR_PARAM   = -2,
    MAX30102_ERR_ID      = -3,
} max30102_err_t;

/* --- I2C HAL (injected) --- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} max30102_i2c_t;

/* --- constants --- */

#define MAX30102_ADDR       0x57u
#define MAX30102_PART_ID    0x15u

/* --- operating modes --- */

typedef enum {
    MAX30102_MODE_HR   = 0x02,   /* heart-rate only (RED LED) */
    MAX30102_MODE_SPO2 = 0x03,   /* SpO2 (RED + IR LEDs)     */
} max30102_mode_t;

/* --- FIFO sample --- */

typedef struct {
    uint32_t red;
    uint32_t ir;
} max30102_sample_t;

/* --- device context --- */

typedef struct {
    const max30102_i2c_t *i2c;
    max30102_mode_t       mode;
} max30102_t;

/* --- public API --- */

/** Initialize device: verify part ID, reset, configure defaults.
 *  All three callbacks are required, delay_ms included: the reset and the
 *  temperature conversion need real waits.
 *  @thread_safe: no  @isr_safe: no */
max30102_err_t max30102_init(max30102_t *dev, const max30102_i2c_t *i2c);

/** Set operating mode (HR-only or SpO2).  Empties the FIFO, so samples not
 *  yet read are lost, even if the mode stays the same.
 *  Returns MAX30102_ERR_PARAM, changing nothing, if the current sample rate
 *  is above the limit of the new mode: lower the rate first.
 *  @thread_safe: no @isr_safe: no */
max30102_err_t max30102_set_mode(max30102_t *dev, max30102_mode_t mode);

/** Set LED pulse amplitudes.
 *  red_ma_x10 / ir_ma_x10: current in 0.1 mA units (0..500 => 0..50.0 mA).
 *  @thread_safe: no @isr_safe: no */
max30102_err_t max30102_set_led_current(max30102_t *dev,
                                        uint8_t red_ma_x10,
                                        uint8_t ir_ma_x10);

/** Set sample rate (Hz): 50,100,200,400,800,1000,1600 or 3200, up to the
 *  limit for the current mode and LED pulse width (data sheet Tables 11 and
 *  12).  At the 411 us init sets, that is 400 in SpO2 mode, 1000 in HR mode.
 *  Above the limit returns MAX30102_ERR_PARAM and keeps the old rate.
 *  @thread_safe: no @isr_safe: no */
max30102_err_t max30102_set_sample_rate(max30102_t *dev, uint16_t rate);

/** Read available FIFO samples.
 *  @param samples    output buffer
 *  @param max_count  buffer capacity (max 32)
 *  @param out_count  number of samples actually read
 *  In HR mode only .red is populated; .ir is 0.
 *  @thread_safe: no @isr_safe: no */
max30102_err_t max30102_read_fifo(max30102_t *dev,
                                  max30102_sample_t *samples,
                                  uint8_t max_count,
                                  uint8_t *out_count);

/** Read die temperature (one-shot). @thread_safe: no @isr_safe: no */
max30102_err_t max30102_read_temp(max30102_t *dev, float *temp_c);

/** Enter/exit shutdown (low-power) mode. @thread_safe: no @isr_safe: no */
max30102_err_t max30102_shutdown(max30102_t *dev, bool enable);

/** Software reset.  Blocks ~100 ms via delay_ms callback.
 *  @thread_safe: no @isr_safe: no */
max30102_err_t max30102_reset(max30102_t *dev);

/** Read part ID register (0xFF).  Returns 0 on I2C failure.
 *  @thread_safe: no @isr_safe: no */
uint8_t max30102_read_part_id(max30102_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MAX30102_H */
