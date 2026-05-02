/*
 * drv2605 -- TI DRV2605L I2C haptic motor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * Supports ERM and LRA actuators. Fixed I2C address 0x5A.
 * Waveform sequencer with 8 slots, real-time playback, and
 * internal effect library (1-123). Pure C99, no heap.
 */

#ifndef DRV2605_H
#define DRV2605_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    DRV2605_OK        =  0,
    DRV2605_ERR_I2C   = -1,
    DRV2605_ERR_ID    = -2,
    DRV2605_ERR_PARAM = -3,
} drv2605_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} drv2605_i2c_t;

/* ---------- constants ---------- */

#define DRV2605_ADDR          0x5Au

#define DRV2605_WAVEFORM_SLOTS  8u

/* Device ID field: bits [7:5] of status register (0x00).
 * DRV2605  = 3, DRV2605L = 7. We accept both. */
#define DRV2605_DEVID_2605    3u
#define DRV2605_DEVID_2605L   7u

/* ---------- mode enumeration ---------- */

typedef enum {
    DRV2605_MODE_INTERNAL_TRIGGER = 0x00,
    DRV2605_MODE_EXT_EDGE         = 0x01,
    DRV2605_MODE_EXT_LEVEL        = 0x02,
    DRV2605_MODE_PWM              = 0x03,
    DRV2605_MODE_AUDIO_TO_VIBE    = 0x04,
    DRV2605_MODE_REALTIME         = 0x05,
    DRV2605_MODE_DIAGNOSTICS      = 0x06,
    DRV2605_MODE_CALIBRATION      = 0x07,
} drv2605_mode_t;

/* ---------- actuator type ---------- */

typedef enum {
    DRV2605_ACTUATOR_ERM = 0,
    DRV2605_ACTUATOR_LRA = 1,
} drv2605_actuator_t;

/* ---------- device context ---------- */

typedef struct {
    const drv2605_i2c_t *i2c;
} drv2605_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Reset, verify device ID, enter standby. @thread_safe: caller_must_lock */
drv2605_err_t drv2605_init(drv2605_t *dev, const drv2605_i2c_t *i2c);

/** Set actuator type (ERM or LRA) in feedback control register. @thread_safe: caller_must_lock */
drv2605_err_t drv2605_set_mode(drv2605_t *dev, drv2605_actuator_t type);

/** Load an effect into a waveform slot (0-7). effect_id 1-123, 0 = stop. @thread_safe: caller_must_lock */
drv2605_err_t drv2605_set_waveform(drv2605_t *dev, uint8_t slot,
                                   uint8_t effect_id);

/** Trigger waveform sequencer playback (set GO bit). @thread_safe: caller_must_lock */
drv2605_err_t drv2605_go(drv2605_t *dev);

/** Stop playback (clear GO bit). @thread_safe: caller_must_lock */
drv2605_err_t drv2605_stop(drv2605_t *dev);

/** Set real-time playback value (signed, for RTP mode). @thread_safe: caller_must_lock */
drv2605_err_t drv2605_set_realtime_value(drv2605_t *dev, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* DRV2605_H */
