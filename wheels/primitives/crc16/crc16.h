/*
 * crc16 — CRC16-MODBUS computation (table-based)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Polynomial: 0xA001 (reflected form of 0x8005)
 * Initial value: 0xFFFF, no final XOR.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initial accumulator value for CRC16-MODBUS streaming. */
#define CRC16_MODBUS_INIT  0xFFFFu

/**
 * Compute CRC16-MODBUS over a byte buffer.
 *
 * @param data  Input buffer pointer. May be NULL only when len == 0.
 * @param len   Number of bytes to absorb. 0 returns CRC16_MODBUS_INIT.
 * @return      16-bit CRC value.
 * @thread_safe: yes (pure function, no state)
 */
uint16_t crc16_modbus(const uint8_t *data, size_t len);

/**
 * Incremental single-byte step for streaming computation.
 *
 * @param crc   Current CRC accumulator (CRC16_MODBUS_INIT on first call).
 * @param byte  Next byte to absorb.
 * @return      Updated CRC accumulator.
 * @thread_safe: yes (pure function, no state)
 */
uint16_t crc16_modbus_step(uint16_t crc, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* CRC16_H */
