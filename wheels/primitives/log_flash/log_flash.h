/*
 * log_flash — persistent flash log sink with sector-ring management.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#ifndef LOG_FLASH_H
#define LOG_FLASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_FLASH_ENTRY_MAGIC  0xA5

typedef struct {
    int (*read)(void *ctx, uint32_t offset, uint8_t *buf, size_t len);
    int (*write)(void *ctx, uint32_t offset, const uint8_t *data, size_t len);
    int (*erase)(void *ctx, uint32_t sector_offset, size_t sector_size);
} log_flash_hal_t;

typedef struct {
    log_flash_hal_t  hal;
    void            *hal_ctx;
    uint32_t         base_offset;
    uint32_t         sector_size;
    uint32_t         sector_count;
    uint32_t         write_sector;
    uint32_t         write_pos;
    uint32_t         total_entries;
    uint32_t         dropped;
} log_flash_t;

/* Initialize flash log.
 *   base_offset: starting address in flash
 *   sector_size: bytes per erase sector (e.g. 4096)
 *   sector_count: number of sectors to use as ring
 * @thread_safe: no — call during init */
int log_flash_init(log_flash_t *f, const log_flash_hal_t *hal, void *hal_ctx,
                   uint32_t base_offset, uint32_t sector_size,
                   uint32_t sector_count);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_flash_t* as ctx when registering via log_add_sink().
 * Each entry stored as: [magic:1][len:2][data:len]
 * @thread_safe: no */
void log_flash_sink(void *ctx, const char *line, size_t len);

/* Read all stored entries into dst, oldest first.
 * Returns bytes written to dst.
 * @thread_safe: no */
size_t log_flash_read(log_flash_t *f, char *dst, size_t dst_size);

/* Erase all sectors and reset state.
 * Returns 0 on success, -1 if any erase failed (state not reset).
 * @thread_safe: no */
int log_flash_erase_all(log_flash_t *f);

/* Return total entries written since init.
 * @thread_safe: no */
uint32_t log_flash_total(const log_flash_t *f);

/* Return entries dropped (too large for sector or write error).
 * @thread_safe: no */
uint32_t log_flash_dropped(const log_flash_t *f);

#ifdef __cplusplus
}
#endif

#endif /* LOG_FLASH_H */
