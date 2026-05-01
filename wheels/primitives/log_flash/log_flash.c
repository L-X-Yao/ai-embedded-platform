/*
 * log_flash — persistent flash log sink with sector-ring management.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_flash.h"
#include <string.h>
#include <limits.h>

#define ENTRY_HEADER_SIZE  3  /* magic(1) + len(2) */
#define MAX_ENTRY_LEN      0xFFFF

static uint32_t sector_offset(const log_flash_t *f, uint32_t sector) {
    return f->base_offset + sector * f->sector_size;
}

static int erase_sector(log_flash_t *f, uint32_t sector) {
    if (!f->hal.erase) return -1;
    return f->hal.erase(f->hal_ctx, sector_offset(f, sector), f->sector_size);
}

static int write_at(log_flash_t *f, uint32_t offset, const uint8_t *data, size_t len) {
    if (!f->hal.write) return -1;
    return f->hal.write(f->hal_ctx, offset, data, len);
}

static int read_at(const log_flash_t *f, uint32_t offset, uint8_t *buf, size_t len) {
    if (!f->hal.read) return -1;
    return f->hal.read(f->hal_ctx, offset, buf, len);
}

int log_flash_init(log_flash_t *f, const log_flash_hal_t *hal, void *hal_ctx,
                   uint32_t base_offset, uint32_t sector_size,
                   uint32_t sector_count) {
    if (!f || !hal || sector_size == 0 || sector_count == 0) return -1;
    f->hal = *hal;
    f->hal_ctx = hal_ctx;
    f->base_offset = base_offset;
    f->sector_size = sector_size;
    f->sector_count = sector_count;
    f->write_sector = 0;
    f->write_pos = 0;
    f->total_entries = 0;
    f->dropped = 0;
    if (erase_sector(f, 0) != 0) return -1;
    return 0;
}

void log_flash_sink(void *ctx, const char *line, size_t len) {
    log_flash_t *f = (log_flash_t *)ctx;
    if (!f || !line || len == 0) return;

    if (len > MAX_ENTRY_LEN || ENTRY_HEADER_SIZE + len > f->sector_size) {
        if (f->dropped < UINT32_MAX) f->dropped++;
        return;
    }

    uint32_t needed = ENTRY_HEADER_SIZE + (uint32_t)len;

    if (f->write_pos + needed > f->sector_size) {
        f->write_sector = (f->write_sector + 1) % f->sector_count;
        f->write_pos = 0;
        if (erase_sector(f, f->write_sector) != 0) {
            if (f->dropped < UINT32_MAX) f->dropped++;
            return;
        }
    }

    uint32_t abs_offset = sector_offset(f, f->write_sector) + f->write_pos;

    uint8_t hdr[ENTRY_HEADER_SIZE];
    hdr[0] = LOG_FLASH_ENTRY_MAGIC;
    hdr[1] = (uint8_t)(len & 0xFF);
    hdr[2] = (uint8_t)((len >> 8) & 0xFF);

    if (write_at(f, abs_offset + ENTRY_HEADER_SIZE, (const uint8_t *)line, len) != 0) {
        if (f->dropped < UINT32_MAX) f->dropped++;
        return;
    }
    if (write_at(f, abs_offset, hdr, ENTRY_HEADER_SIZE) != 0) {
        if (f->dropped < UINT32_MAX) f->dropped++;
        return;
    }

    f->write_pos += needed;
    if (f->total_entries < UINT32_MAX) f->total_entries++;
}

size_t log_flash_read(const log_flash_t *f, char *dst, size_t dst_size) {
    if (!f || !dst || dst_size == 0) return 0;

    size_t out_pos = 0;

    uint32_t start_sector = (f->write_sector + 1) % f->sector_count;

    for (uint32_t i = 0; i < f->sector_count; i++) {
        uint32_t sec = (start_sector + i) % f->sector_count;
        uint32_t pos = 0;

        while (pos + ENTRY_HEADER_SIZE <= f->sector_size) {
            uint8_t hdr[ENTRY_HEADER_SIZE];
            uint32_t abs = sector_offset(f, sec) + pos;

            if (read_at(f, abs, hdr, ENTRY_HEADER_SIZE) != 0) break;
            if (hdr[0] != LOG_FLASH_ENTRY_MAGIC) break;

            uint16_t entry_len = (uint16_t)hdr[1] | ((uint16_t)hdr[2] << 8);
            if (entry_len == 0 || pos + ENTRY_HEADER_SIZE + entry_len > f->sector_size)
                break;

            size_t copy = entry_len;
            if (out_pos + copy > dst_size) copy = dst_size - out_pos;
            if (copy == 0) break;

            if (read_at(f, abs + ENTRY_HEADER_SIZE, (uint8_t *)dst + out_pos, copy) != 0)
                break;

            out_pos += copy;
            pos += ENTRY_HEADER_SIZE + entry_len;
        }
    }

    return out_pos;
}

int log_flash_erase_all(log_flash_t *f) {
    if (!f) return -1;
    for (uint32_t i = 0; i < f->sector_count; i++) {
        if (erase_sector(f, i) != 0) return -1;
    }
    f->write_sector = 0;
    f->write_pos = 0;
    f->total_entries = 0;
    f->dropped = 0;
    return 0;
}

uint32_t log_flash_total(const log_flash_t *f) {
    return f ? f->total_entries : 0;
}

uint32_t log_flash_dropped(const log_flash_t *f) {
    return f ? f->dropped : 0;
}
