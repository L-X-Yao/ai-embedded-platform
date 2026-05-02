// SPDX-License-Identifier: MIT
/*
 * crc8 — CRC-8 calculator with multiple polynomial support (bit-by-bit)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Supports: CRC8, CRC8/MAXIM (1-Wire), CRC8/SENSIRION (SHT30),
 *           CRC8/SMBUS (PEC), CRC8/ROHC.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CRC-8 algorithm configuration.
 *
 * All fields are public and const-safe. Users may define custom configs.
 */
typedef struct {
    uint8_t poly;       /**< Generator polynomial (unreflected, MSB-first). */
    uint8_t init;       /**< Initial CRC register value. */
    bool    ref_in;     /**< Reflect each input byte before processing. */
    bool    ref_out;    /**< Reflect the final CRC before XOR-out. */
    uint8_t xor_out;    /**< Value XORed with the final CRC. */
} crc8_config_t;

/* --- Pre-defined configurations ----------------------------------------- */

/** CRC-8 (ITU-T): poly=0x07, init=0x00. */
extern const crc8_config_t CRC8_DEFAULT;

/** CRC-8/MAXIM (Dallas 1-Wire ROM CRC): poly=0x31, init=0x00, refin/refout. */
extern const crc8_config_t CRC8_MAXIM;

/** CRC-8/SENSIRION (SHT30, SI7021): poly=0x31, init=0xFF. */
extern const crc8_config_t CRC8_SENSIRION;

/** CRC-8/SMBUS (SMBus PEC): poly=0x07, init=0x00. Same as CRC8_DEFAULT. */
extern const crc8_config_t CRC8_SMBUS;

/** CRC-8/ROHC: poly=0x07, init=0xFF, refin/refout. */
extern const crc8_config_t CRC8_ROHC;

/**
 * Compute CRC-8 over a byte buffer (one-shot).
 *
 * @param cfg   Configuration (polynomial, init, reflect, xor-out).
 *              Must not be NULL.
 * @param data  Input buffer pointer. May be NULL only when len == 0.
 * @param len   Number of bytes to process.
 * @return      Computed 8-bit CRC value.
 * @thread_safe: yes (pure function, no state)
 */
uint8_t crc8_compute(const crc8_config_t *cfg,
                     const uint8_t *data, size_t len);

/**
 * Update a running CRC-8 with additional data (streaming).
 *
 * First call: pass cfg->init as the crc argument.
 * Intermediate state is *not* finalized — call crc8_compute or
 * use crc8_update for the last chunk, then the return value is the
 * raw accumulator. For configs with ref_out or xor_out, the caller
 * must apply finalization manually, or simply use crc8_compute on the
 * concatenated data. Designed for simple non-reflected streaming;
 * for reflected configs, prefer crc8_compute on the full message.
 *
 * @param cfg   Configuration. Must not be NULL.
 * @param crc   Current CRC accumulator (cfg->init on first call).
 * @param data  Next chunk of data. May be NULL only when len == 0.
 * @param len   Number of bytes in this chunk.
 * @return      Updated CRC accumulator (without finalization).
 * @thread_safe: yes (pure function, no state)
 */
uint8_t crc8_update(const crc8_config_t *cfg,
                    uint8_t crc, const uint8_t *data, size_t len);

/**
 * Verify a CRC-8 against an expected value.
 *
 * Equivalent to: crc8_compute(cfg, data, len) == expected.
 *
 * @param cfg       Configuration. Must not be NULL.
 * @param data      Input buffer.
 * @param len       Number of bytes.
 * @param expected  Expected CRC value.
 * @return          true if CRC matches, false otherwise.
 * @thread_safe: yes (pure function, no state)
 */
bool crc8_verify(const crc8_config_t *cfg,
                 const uint8_t *data, size_t len, uint8_t expected);

#ifdef __cplusplus
}
#endif

#endif /* CRC8_H */
