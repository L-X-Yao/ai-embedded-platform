// SPDX-License-Identifier: MIT
/*
 * endian — portable byte-order conversion utilities
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * All conversions use arithmetic shifts — no pointer casts, no
 * alignment assumptions, portable across all C99 targets.
 */

#include "endian.h"

/* ── Compile-time endianness detection ─────────────────────────── */

/*
 * Try GCC/Clang built-in first, then common predefined macros.
 * Fall back to runtime probe if nothing is available.
 */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define ENDIAN_HOST_LITTLE  1
  #else
    #define ENDIAN_HOST_LITTLE  0
  #endif
#elif defined(_WIN32) || defined(__x86_64__) || defined(__i386__) \
   || defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__)
  #define ENDIAN_HOST_LITTLE  1
#else
  /* runtime fallback — see endian_is_little() */
#endif

/* ── Byte swap ─────────────────────────────────────────────────── */

uint16_t endian_swap16(uint16_t val)
{
    return (uint16_t)((val >> 8) | (val << 8));
}

uint32_t endian_swap32(uint32_t val)
{
    return ((val >> 24) & 0x000000FFu)
         | ((val >>  8) & 0x0000FF00u)
         | ((val <<  8) & 0x00FF0000u)
         | ((val << 24) & 0xFF000000u);
}

uint64_t endian_swap64(uint64_t val)
{
    return ((val >> 56) & 0x00000000000000FFull)
         | ((val >> 40) & 0x000000000000FF00ull)
         | ((val >> 24) & 0x0000000000FF0000ull)
         | ((val >>  8) & 0x00000000FF000000ull)
         | ((val <<  8) & 0x000000FF00000000ull)
         | ((val << 24) & 0x0000FF0000000000ull)
         | ((val << 40) & 0x00FF000000000000ull)
         | ((val << 56) & 0xFF00000000000000ull);
}

/* ── Runtime detection ─────────────────────────────────────────── */

bool endian_is_little(void)
{
#ifdef ENDIAN_HOST_LITTLE
    return ENDIAN_HOST_LITTLE != 0;
#else
    const uint16_t probe = 1u;
    return *(const uint8_t *)&probe == 1u;
#endif
}

/* ── Host ↔ big-endian ─────────────────────────────────────────── */

uint16_t endian_htobe16(uint16_t val)
{
    return endian_is_little() ? endian_swap16(val) : val;
}

uint32_t endian_htobe32(uint32_t val)
{
    return endian_is_little() ? endian_swap32(val) : val;
}

uint64_t endian_htobe64(uint64_t val)
{
    return endian_is_little() ? endian_swap64(val) : val;
}

uint16_t endian_be16toh(uint16_t val) { return endian_htobe16(val); }
uint32_t endian_be32toh(uint32_t val) { return endian_htobe32(val); }
uint64_t endian_be64toh(uint64_t val) { return endian_htobe64(val); }

/* ── Host ↔ little-endian ──────────────────────────────────────── */

uint16_t endian_htole16(uint16_t val)
{
    return endian_is_little() ? val : endian_swap16(val);
}

uint32_t endian_htole32(uint32_t val)
{
    return endian_is_little() ? val : endian_swap32(val);
}

uint64_t endian_htole64(uint64_t val)
{
    return endian_is_little() ? val : endian_swap64(val);
}

uint16_t endian_le16toh(uint16_t val) { return endian_htole16(val); }
uint32_t endian_le32toh(uint32_t val) { return endian_htole32(val); }
uint64_t endian_le64toh(uint64_t val) { return endian_htole64(val); }

/* ── Unaligned buffer read ─────────────────────────────────────── */

uint16_t endian_read_be16(const uint8_t *buf)
{
    if (!buf) { return 0; }
    return (uint16_t)((uint16_t)buf[0] << 8 | (uint16_t)buf[1]);
}

uint32_t endian_read_be32(const uint8_t *buf)
{
    if (!buf) { return 0; }
    return (uint32_t)buf[0] << 24
         | (uint32_t)buf[1] << 16
         | (uint32_t)buf[2] <<  8
         | (uint32_t)buf[3];
}

uint16_t endian_read_le16(const uint8_t *buf)
{
    if (!buf) { return 0; }
    return (uint16_t)((uint16_t)buf[1] << 8 | (uint16_t)buf[0]);
}

uint32_t endian_read_le32(const uint8_t *buf)
{
    if (!buf) { return 0; }
    return (uint32_t)buf[3] << 24
         | (uint32_t)buf[2] << 16
         | (uint32_t)buf[1] <<  8
         | (uint32_t)buf[0];
}

/* ── Unaligned buffer write ────────────────────────────────────── */

void endian_write_be16(uint8_t *buf, uint16_t val)
{
    if (!buf) { return; }
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val);
}

void endian_write_be32(uint8_t *buf, uint32_t val)
{
    if (!buf) { return; }
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >>  8);
    buf[3] = (uint8_t)(val);
}

void endian_write_le16(uint8_t *buf, uint16_t val)
{
    if (!buf) { return; }
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >> 8);
}

void endian_write_le32(uint8_t *buf, uint32_t val)
{
    if (!buf) { return; }
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >>  8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);
}
