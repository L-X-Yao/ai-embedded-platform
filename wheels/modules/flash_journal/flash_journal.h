// SPDX-License-Identifier: MIT
/*
 * flash_journal -- append-only log journal for flash storage with wear leveling
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none -- self-contained)
 * Instances: multi
 *
 * Passive design: application provides read/write/erase sector callbacks.
 * Journal appends entries sequentially across sectors, wrapping around
 * when full (oldest entries overwritten). Each entry has a header with
 * magic, sequence number, payload length, followed by payload and CRC32.
 *
 * Format per entry:
 *   [magic:4][seq:4][len:2] [payload:len] [crc32:4]
 *   Header = 10 bytes, footer = 4 bytes, overhead = 14 bytes per entry.
 */

#ifndef FLASH_JOURNAL_H
#define FLASH_JOURNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Configuration macros (override before #include) ---------- */

#ifndef FLASH_JOURNAL_MAX_SECTORS
#define FLASH_JOURNAL_MAX_SECTORS   64
#endif

#ifndef FLASH_JOURNAL_MAX_ENTRY_SIZE
#define FLASH_JOURNAL_MAX_ENTRY_SIZE  256
#endif

/* ---------- Error codes ---------- */

typedef enum {
    FLASH_JOURNAL_OK            =  0,
    FLASH_JOURNAL_ERR_PARAM     = -1,
    FLASH_JOURNAL_ERR_STORAGE   = -2,
    FLASH_JOURNAL_ERR_FULL      = -3,  /* entry too large for sector */
    FLASH_JOURNAL_ERR_EMPTY     = -4,
    FLASH_JOURNAL_ERR_CORRUPT   = -5,
    FLASH_JOURNAL_ERR_SIZE      = -6,  /* output buffer too small */
} flash_journal_err_t;

/* ---------- Storage abstraction ---------- */

typedef struct {
    flash_journal_err_t (*read)(void *ctx, uint32_t sector, uint32_t offset,
                                uint8_t *buf, uint32_t len);
    flash_journal_err_t (*write)(void *ctx, uint32_t sector, uint32_t offset,
                                 const uint8_t *data, uint32_t len);
    flash_journal_err_t (*erase)(void *ctx, uint32_t sector);
    void *ctx;
} flash_journal_storage_t;

/* ---------- Entry header (on-flash format) ---------- */

#define FLASH_JOURNAL_MAGIC  0x4A524E4Cu  /* "JRNL" */

#define FLASH_JOURNAL_HDR_SIZE   10u  /* magic(4) + seq(4) + len(2) */
#define FLASH_JOURNAL_CRC_SIZE    4u
#define FLASH_JOURNAL_OVERHEAD   (FLASH_JOURNAL_HDR_SIZE + FLASH_JOURNAL_CRC_SIZE)

/* ---------- Journal context ---------- */

typedef struct {
    const flash_journal_storage_t *storage;
    uint32_t  sector_count;           /* total sectors */
    uint32_t  sector_size;            /* bytes per sector */
    uint32_t  write_sector;           /* current sector for appends */
    uint32_t  write_offset;           /* offset within write_sector */
    uint32_t  next_seq;               /* next sequence number */
    uint32_t  entry_count;            /* total valid entries */
    uint32_t  oldest_sector;          /* sector containing oldest entry */
    uint32_t  oldest_offset;          /* offset of oldest entry in sector */
} flash_journal_t;

/* ---------- API ---------- */

/**
 * Initialize journal with storage backend and geometry.
 *
 * Scans all sectors to recover write position and entry count.
 *
 * @param jrnl          Journal context (caller-allocated).
 * @param storage       Storage callbacks (must outlive jrnl).
 * @param sector_count  Number of sectors (1..FLASH_JOURNAL_MAX_SECTORS).
 * @param sector_size   Bytes per sector (must be > FLASH_JOURNAL_OVERHEAD).
 * @return              FLASH_JOURNAL_OK on success.
 * @thread_safe: no (caller must serialize)
 */
flash_journal_err_t flash_journal_init(flash_journal_t *jrnl,
                                       const flash_journal_storage_t *storage,
                                       uint32_t sector_count,
                                       uint32_t sector_size);

/**
 * Append a new entry to the journal.
 *
 * If the current sector is full, advances to the next sector (erasing it).
 * Wraps around when all sectors have been used.
 *
 * @param jrnl  Journal context.
 * @param data  Payload data.
 * @param len   Payload length (0 < len <= sector_size - FLASH_JOURNAL_OVERHEAD).
 * @return      FLASH_JOURNAL_OK on success.
 * @thread_safe: no (caller must serialize)
 */
flash_journal_err_t flash_journal_append(flash_journal_t *jrnl,
                                         const uint8_t *data, uint16_t len);

/**
 * Read the most recently appended entry.
 *
 * @param jrnl  Journal context.
 * @param buf   Output buffer.
 * @param size  Buffer size; updated with actual payload length on success.
 * @return      FLASH_JOURNAL_OK on success, ERR_EMPTY if no entries.
 * @thread_safe: no (caller must serialize)
 */
flash_journal_err_t flash_journal_read_latest(const flash_journal_t *jrnl,
                                              uint8_t *buf, uint32_t *size);

/**
 * Read the n-th entry (0 = oldest).
 *
 * @param jrnl  Journal context.
 * @param n     Entry index (0-based, 0 = oldest).
 * @param buf   Output buffer.
 * @param size  Buffer size; updated with actual payload length on success.
 * @return      FLASH_JOURNAL_OK on success.
 * @thread_safe: no (caller must serialize)
 */
flash_journal_err_t flash_journal_read_nth(const flash_journal_t *jrnl,
                                           uint32_t n,
                                           uint8_t *buf, uint32_t *size);

/**
 * Get the number of valid entries in the journal.
 *
 * @param jrnl  Journal context.
 * @return      Number of entries, or 0 if jrnl is NULL.
 * @thread_safe: no (caller must serialize)
 */
uint32_t flash_journal_count(const flash_journal_t *jrnl);

/**
 * Erase all sectors and reset journal to empty state.
 *
 * @param jrnl  Journal context.
 * @return      FLASH_JOURNAL_OK on success.
 * @thread_safe: no (caller must serialize)
 */
flash_journal_err_t flash_journal_erase_all(flash_journal_t *jrnl);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_JOURNAL_H */
