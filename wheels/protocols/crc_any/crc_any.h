// SPDX-License-Identifier: MIT
/*
 * crc_any — configurable CRC calculator supporting any width 1-32 bit
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Bit-by-bit implementation (~100 lines). No lookup tables, minimal RAM.
 * Supports reflected/non-reflected input and output, arbitrary polynomial,
 * init value, and final XOR — covers any CRC algorithm in the catalogue.
 *
 * Thread safety: all functions are pure / reentrant (no global state).
 */

#ifndef CRC_ANY_H
#define CRC_ANY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CRC algorithm configuration.
 *
 * Fields follow the Rocksoft model (width/poly/init/refin/refout/xorout).
 * Pre-defined configurations provided below for common algorithms.
 */
typedef struct {
    uint8_t  width;     /**< CRC width in bits (1..32) */
    uint32_t poly;      /**< Generator polynomial (MSB-first, high bit implicit) */
    uint32_t init;      /**< Initial CRC register value */
    bool     refin;     /**< Reflect each input byte before processing */
    bool     refout;    /**< Reflect the final CRC before XOR */
    uint32_t xorout;    /**< XOR applied to final CRC */
} crc_any_config_t;

typedef enum {
    CRC_ANY_OK        =  0,
    CRC_ANY_ERR_PARAM = -1,
} crc_any_err_t;

/* ── Pre-defined configurations (Rocksoft catalogue vectors) ──────── */

#define CRC_ANY_CRC16_MODBUS  ((crc_any_config_t){ \
    .width = 16, .poly = 0x8005, .init = 0xFFFF,   \
    .refin = true, .refout = true, .xorout = 0x0000 })

#define CRC_ANY_CRC16_CCITT   ((crc_any_config_t){ \
    .width = 16, .poly = 0x1021, .init = 0xFFFF,   \
    .refin = false, .refout = false, .xorout = 0x0000 })

#define CRC_ANY_CRC32_ISO     ((crc_any_config_t){ \
    .width = 32, .poly = 0x04C11DB7, .init = 0xFFFFFFFF, \
    .refin = true, .refout = true, .xorout = 0xFFFFFFFF })

#define CRC_ANY_CRC8_MAXIM    ((crc_any_config_t){ \
    .width = 8, .poly = 0x31, .init = 0x00,        \
    .refin = true, .refout = true, .xorout = 0x00 })

/**
 * Compute CRC over a complete buffer in one call.
 *
 * @param cfg   Algorithm configuration. Must not be NULL.
 *              Width must be 1..32.
 * @param data  Input buffer. May be NULL only when len == 0.
 * @param len   Number of bytes.
 * @param out   Receives the computed CRC value. Must not be NULL.
 * @return      CRC_ANY_OK on success, CRC_ANY_ERR_PARAM on bad input.
 * @thread_safe: yes (pure function)
 */
crc_any_err_t crc_any_compute(const crc_any_config_t *cfg,
                              const uint8_t *data, size_t len,
                              uint32_t *out);

/**
 * Feed data into a running CRC calculation (streaming / incremental).
 *
 * First call: pass cfg->init as crc.
 * Subsequent calls: pass the value returned by the previous call.
 * After all data fed: call crc_any_finalize() to get the final CRC.
 *
 * @param cfg   Algorithm configuration. Must not be NULL.
 * @param crc   Running CRC accumulator.
 * @param data  Input buffer. May be NULL only when len == 0.
 * @param len   Number of bytes.
 * @return      Updated CRC accumulator (not yet finalized).
 * @thread_safe: yes (pure function)
 */
uint32_t crc_any_update(const crc_any_config_t *cfg, uint32_t crc,
                        const uint8_t *data, size_t len);

/**
 * Verify that data + appended CRC is consistent.
 *
 * Computes CRC over the buffer and compares with expected value.
 *
 * @param cfg      Algorithm configuration. Must not be NULL.
 * @param data     Input buffer. Must not be NULL.
 * @param len      Number of bytes (payload only, excluding CRC).
 * @param expected Expected CRC value.
 * @return         true if computed CRC matches expected.
 * @thread_safe: yes (pure function)
 */
bool crc_any_verify(const crc_any_config_t *cfg,
                    const uint8_t *data, size_t len,
                    uint32_t expected);

#ifdef __cplusplus
}
#endif

#endif /* CRC_ANY_H */
