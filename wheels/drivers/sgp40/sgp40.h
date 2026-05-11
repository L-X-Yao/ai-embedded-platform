// SPDX-License-Identifier: MIT
/*
 * sgp40 — Sensirion SGP40 VOC raw signal sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C command-based protocol with CRC-8 verification (poly 0x31, init 0xFF).
 * Returns raw SRAW signal (0-65535). VOC index calculation requires the
 * Sensirion VOC Algorithm library (not included — out of scope for driver).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef SGP40_H
#define SGP40_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SGP40_OK        =  0,
    SGP40_ERR_I2C   = -1,
    SGP40_ERR_CRC   = -2,
    SGP40_ERR_PARAM = -3,
    SGP40_ERR_SELF_TEST = -4,
} sgp40_err_t;

/* ---------- I2C HAL (caller provides) ---------- */
/*
 * SGP40 uses command-based I2C (not register-based).
 * Adaptation: write_reg(ctx, addr, cmd_msb, cmd_lsb) sends 2-byte command.
 * read_regs(ctx, addr, 0, buf, len) reads response.
 */
typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* required for measurement wait */
    void *ctx;
} sgp40_i2c_t;

/* ---------- constants ---------- */

#define SGP40_DEV_ADDR  0x59

/* ---------- data types ---------- */

typedef struct {
    uint16_t sraw;   /* raw VOC signal (0-65535) */
} sgp40_data_t;

/* ---------- device context ---------- */

typedef struct {
    const sgp40_i2c_t *i2c;
    uint8_t            addr;
} sgp40_t;

/* ---------- API ---------- */

/** Run self-test to verify chip. @thread_safe: no @isr_safe: no */
sgp40_err_t sgp40_init(sgp40_t *dev, const sgp40_i2c_t *i2c);

/** Measure raw VOC signal with optional humidity/temp compensation.
 *  Pass default_rh=0x8000 (50%) and default_t=0x6666 (25C) for no compensation.
 *  @thread_safe: caller_must_lock @isr_safe: no */
sgp40_err_t sgp40_measure_raw(sgp40_t *dev, uint16_t rh_ticks,
                              uint16_t t_ticks, sgp40_data_t *out);

/** CRC-8 used by Sensirion sensors (poly 0x31, init 0xFF). */
uint8_t sgp40_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SGP40_H */
