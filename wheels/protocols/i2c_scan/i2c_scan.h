// SPDX-License-Identifier: MIT
/*
 * i2c_scan — I2C bus scanner and device identifier
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#ifndef I2C_SCAN_H
#define I2C_SCAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────── */

/** Maximum devices storable in i2c_scan_result_t (compile-time). */
#ifndef I2C_SCAN_MAX_DEVICES
#define I2C_SCAN_MAX_DEVICES  16
#endif

/** Default scan range: 7-bit addresses 0x08..0x77 (skip reserved). */
#define I2C_SCAN_ADDR_MIN  0x08u
#define I2C_SCAN_ADDR_MAX  0x77u

/* ── Error codes ───────────────────────────────────────────────── */

typedef enum {
    I2C_SCAN_OK          =  0,
    I2C_SCAN_ERR_PARAM  = -1,
} i2c_scan_err_t;

/* ── Callbacks ─────────────────────────────────────────────────── */

/**
 * Probe callback: attempt to communicate with @p addr.
 * Return true if the device ACKs, false otherwise.
 * @thread_safe: caller responsibility
 */
typedef bool (*i2c_scan_probe_fn)(void *ctx, uint8_t addr);

/**
 * Found callback: invoked once for each address that ACKs.
 * @thread_safe: caller responsibility
 */
typedef void (*i2c_scan_found_fn)(void *ctx, uint8_t addr);

/* ── Result container ──────────────────────────────────────────── */

typedef struct {
    uint8_t addresses[I2C_SCAN_MAX_DEVICES];
    uint8_t count;
} i2c_scan_result_t;

/* ── Public API ────────────────────────────────────────────────── */

/**
 * Scan all valid 7-bit addresses (0x08..0x77).
 * Stores found addresses in @p result. @thread_safe: no (shared hw)
 */
i2c_scan_err_t i2c_scan(i2c_scan_probe_fn probe, void *probe_ctx,
                         i2c_scan_result_t *result);

/**
 * Scan a sub-range [@p start, @p end] inclusive.
 * @thread_safe: no (shared hw)
 */
i2c_scan_err_t i2c_scan_range(i2c_scan_probe_fn probe, void *probe_ctx,
                               uint8_t start, uint8_t end,
                               i2c_scan_result_t *result);

/**
 * Scan all valid addresses; invoke @p found for each ACK instead of
 * storing results. No overflow limit.
 * @thread_safe: no (shared hw)
 */
i2c_scan_err_t i2c_scan_callback(i2c_scan_probe_fn probe, void *probe_ctx,
                                  i2c_scan_found_fn found, void *found_ctx);

/**
 * Return a human-readable name for a well-known I2C address,
 * or NULL if the address is not in the built-in table.
 * @thread_safe: yes (pure function, const data)
 */
const char *i2c_scan_identify(uint8_t addr);

#ifdef __cplusplus
}
#endif

#endif /* I2C_SCAN_H */
