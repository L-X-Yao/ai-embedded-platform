// SPDX-License-Identifier: MIT
/*
 * xgzp6816d — XGZP6816D barometric pressure sensor driver (I2C)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef XGZP6816D_H
#define XGZP6816D_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    XGZP6816D_OK         =  0,
    XGZP6816D_ERR_I2C    = -1,
    XGZP6816D_ERR_NO_ACK = -2,
} xgzp6816d_err_t;

/* ---------- I2C HAL (caller provides) ----------
 *
 * Generic write/read transactions (NOT a register-paired protocol — XGZP6816D
 * uses bare command bytes / streamed data reads, not "write reg → read N").
 *
 * write: START + (dev_addr<<1|W) + buf bytes + STOP
 * read:  START + (dev_addr<<1|R) + len bytes (last NACK) + STOP
 *
 * Returns true on success, false on bus error or address NACK.
 */
typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr, const uint8_t *buf, uint16_t len);
    bool (*read) (void *ctx, uint8_t dev_addr, uint8_t *buf, uint16_t len);
    void *ctx;
} xgzp6816d_i2c_t;

/* ---------- constants ---------- */

#define XGZP6816D_DEV_ADDR     0x78u   /* 7-bit I2C address */
#define XGZP6816D_CONV_MS      25u     /* typical PT conversion time (datasheet ~20ms) */

/* ---------- output ---------- */

typedef struct {
    float pressure_pa;     /* atmospheric pressure (Pa, range 30000–110000) */
    float temperature_c;   /* sensor die temperature (°C, range -40 to 150) */
} xgzp6816d_data_t;

/* ---------- API ----------
 *
 * Usage pattern:
 *
 *     xgzp6816d_probe(&i2c);              // once at startup
 *
 *     loop:
 *         xgzp6816d_trigger(&i2c);        // start measurement
 *         (caller waits XGZP6816D_CONV_MS milliseconds — wheel has no RTOS dep)
 *         xgzp6816d_read(&i2c, &data);
 *
 * The wheel deliberately splits trigger and read so callers can use whatever
 * delay primitive their environment provides (vTaskDelay, k_msleep, busy
 * loop, even queueing the read in a different task).
 */

/** Probe device on bus. @thread_safe: caller_must_lock @isr_safe: no */
xgzp6816d_err_t xgzp6816d_probe(const xgzp6816d_i2c_t *i2c);

/** Start one P+T conversion. @thread_safe: caller_must_lock @isr_safe: no */
xgzp6816d_err_t xgzp6816d_trigger(const xgzp6816d_i2c_t *i2c);

/** Read last conversion result. @thread_safe: caller_must_lock @isr_safe: no */
xgzp6816d_err_t xgzp6816d_read(const xgzp6816d_i2c_t *i2c,
                                xgzp6816d_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* XGZP6816D_H */
