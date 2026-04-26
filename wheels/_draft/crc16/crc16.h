/**
 * @file    crc16.h
 * @brief   CRC16-MODBUS computation (table-based).
 *
 * Polynomial: 0xA001 (reflected form of 0x8005)
 * Initial value: 0xFFFF
 * No final XOR.
 *
 * Compatible with Modbus RTU frame check. For other CRC variants
 * (CCITT-XMODEM, CCITT-FALSE, KERMIT, etc.) use VSF native vsf_crc()
 * with the appropriate poly — see "Why not VSF native?" in README.
 *
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
 * @brief Compute CRC16-MODBUS over a byte buffer.
 *
 * @param data  Input buffer pointer. May be NULL only when len == 0.
 * @param len   Number of bytes to absorb. 0 returns CRC16_MODBUS_INIT.
 * @return      16-bit CRC value.
 *
 * Equivalent to calling crc16_modbus_step() for every byte starting
 * with crc = CRC16_MODBUS_INIT.
 */
uint16_t crc16_modbus(const uint8_t *data, size_t len);

/**
 * @brief Incremental single-byte step for streaming computation.
 *
 * Use when bytes arrive in chunks (DMA, network) and a one-shot
 * call to crc16_modbus() is impractical:
 *
 *   uint16_t crc = CRC16_MODBUS_INIT;
 *   for (...) { crc = crc16_modbus_step(crc, byte); }
 *
 * Result is identical to crc16_modbus() over the same byte sequence.
 *
 * @param crc   Current CRC accumulator (CRC16_MODBUS_INIT on first call).
 * @param byte  Next byte to absorb.
 * @return      Updated CRC accumulator.
 */
uint16_t crc16_modbus_step(uint16_t crc, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* CRC16_H */
