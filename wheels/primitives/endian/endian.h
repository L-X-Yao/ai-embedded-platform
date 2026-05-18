// SPDX-License-Identifier: MIT
/*
 * endian — portable byte-order conversion utilities
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Host/big/little-endian conversion, unaligned buffer read/write,
 * byte swap.  All shift-based for portability across architectures.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef ENDIAN_H
#define ENDIAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Byte swap ─────────────────────────────────────────────────── */

/** Swap bytes of a 16-bit value. @thread_safe: yes (pure) @isr_safe: yes */
uint16_t endian_swap16(uint16_t val);

/** Swap bytes of a 32-bit value. @thread_safe: yes (pure) @isr_safe: yes */
uint32_t endian_swap32(uint32_t val);

/** Swap bytes of a 64-bit value. @thread_safe: yes (pure) @isr_safe: yes */
uint64_t endian_swap64(uint64_t val);

/* ── Runtime detection ─────────────────────────────────────────── */

/** Return true if the host is little-endian. @thread_safe: yes (pure) @isr_safe: yes */
bool endian_is_little(void);

/* ── Host ↔ big-endian ─────────────────────────────────────────── */

/** Host to big-endian 16-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint16_t endian_htobe16(uint16_t val);

/** Host to big-endian 32-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint32_t endian_htobe32(uint32_t val);

/** Host to big-endian 64-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint64_t endian_htobe64(uint64_t val);

/** Big-endian to host 16-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint16_t endian_be16toh(uint16_t val);

/** Big-endian to host 32-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint32_t endian_be32toh(uint32_t val);

/** Big-endian to host 64-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint64_t endian_be64toh(uint64_t val);

/* ── Host ↔ little-endian ──────────────────────────────────────── */

/** Host to little-endian 16-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint16_t endian_htole16(uint16_t val);

/** Host to little-endian 32-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint32_t endian_htole32(uint32_t val);

/** Host to little-endian 64-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint64_t endian_htole64(uint64_t val);

/** Little-endian to host 16-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint16_t endian_le16toh(uint16_t val);

/** Little-endian to host 32-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint32_t endian_le32toh(uint32_t val);

/** Little-endian to host 64-bit. @thread_safe: yes (pure) @isr_safe: yes */
uint64_t endian_le64toh(uint64_t val);

/* ── Unaligned buffer read (big-endian wire order) ─────────────── */

/** Read big-endian 16-bit from buffer (unaligned safe). @thread_safe: yes (pure) @isr_safe: yes */
uint16_t endian_read_be16(const uint8_t *buf);

/** Read big-endian 32-bit from buffer (unaligned safe). @thread_safe: yes (pure) @isr_safe: yes */
uint32_t endian_read_be32(const uint8_t *buf);

/* ── Unaligned buffer read (little-endian wire order) ──────────── */

/** Read little-endian 16-bit from buffer (unaligned safe). @thread_safe: yes (pure) @isr_safe: yes */
uint16_t endian_read_le16(const uint8_t *buf);

/** Read little-endian 32-bit from buffer (unaligned safe). @thread_safe: yes (pure) @isr_safe: yes */
uint32_t endian_read_le32(const uint8_t *buf);

/* ── Unaligned buffer write (big-endian wire order) ────────────── */

/** Write big-endian 16-bit to buffer (unaligned safe). @thread_safe: yes (pure) @isr_safe: yes */
void endian_write_be16(uint8_t *buf, uint16_t val);

/** Write big-endian 32-bit to buffer (unaligned safe). @thread_safe: yes (pure) @isr_safe: yes */
void endian_write_be32(uint8_t *buf, uint32_t val);

/* ── Unaligned buffer write (little-endian wire order) ─────────── */

/** Write little-endian 16-bit to buffer (unaligned safe). @thread_safe: yes (pure) @isr_safe: yes */
void endian_write_le16(uint8_t *buf, uint16_t val);

/** Write little-endian 32-bit to buffer (unaligned safe). @thread_safe: yes (pure) @isr_safe: yes */
void endian_write_le32(uint8_t *buf, uint32_t val);

#ifdef __cplusplus
}
#endif

#endif /* ENDIAN_H */
