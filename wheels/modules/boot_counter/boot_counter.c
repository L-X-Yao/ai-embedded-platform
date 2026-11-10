// SPDX-License-Identifier: MIT
/*
 * boot_counter -- persistent boot count and reset reason tracker
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "boot_counter.h"
#include "contract.h"
#include <string.h>

/* ---------- Internal helpers ---------- */

/* CRC-32 (IEEE 802.3): the record is too small to justify a dependency */
static uint32_t record_crc(const boot_counter_record_t *r)
{
    const uint8_t *data = (const uint8_t *)r;
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < sizeof(*r) - sizeof(r->crc); i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
        }
    }
    return ~crc;
}

static bool record_valid(const boot_counter_record_t *r)
{
    return r->magic == BOOT_COUNTER_MAGIC && r->crc == record_crc(r);
}

/* seq counts up and wraps, and the two copies stay within one of each other. */
static bool seq_is_newer(uint32_t seq, uint32_t than)
{
    uint32_t ahead = seq - than;
    return ahead != 0 && ahead < 0x80000000u;
}

static boot_counter_err_t save_record(boot_counter_t *ctx)
{
    boot_counter_storage_write_fn write = ctx->storage.write;

    /* Never touch the copy that holds the newest good record. A retry after
     * a failed write reuses the same seq. */
    if (ctx->write2) {
        if (!ctx->stored_in_2) write = ctx->write2;
        ctx->record.seq = ctx->stored_seq + 1u;
    }

    ctx->record.magic = BOOT_COUNTER_MAGIC;
    ctx->record.crc = record_crc(&ctx->record);
    if (write((const uint8_t *)&ctx->record,
              sizeof(ctx->record),
              ctx->storage.user_ctx) != 0) {
        return BOOT_COUNTER_ERR_STORAGE;
    }

    if (ctx->write2) {
        ctx->stored_in_2 = !ctx->stored_in_2;
        ctx->stored_seq = ctx->record.seq;
    }
    return BOOT_COUNTER_OK;
}

static boot_counter_err_t read_copy(const boot_counter_t *ctx,
                                    boot_counter_storage_read_fn read,
                                    boot_counter_record_t *rec, bool *valid)
{
    int rc = read((uint8_t *)rec, sizeof(*rec), ctx->storage.user_ctx);

    *valid = false;
    if (rc == BOOT_COUNTER_READ_NO_RECORD) {
        return BOOT_COUNTER_OK;   /* nothing stored yet, and that is no fault */
    }
    if (rc != 0) {
        return BOOT_COUNTER_ERR_STORAGE;
    }
    *valid = record_valid(rec);
    return BOOT_COUNTER_OK;
}

/* ---------- Public API ---------- */

boot_counter_err_t boot_counter_init(boot_counter_t *ctx,
                                     const boot_counter_storage_t *storage)
{
    if (!ctx || !storage || !storage->read || !storage->write) {
        return BOOT_COUNTER_ERR_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->storage = *storage;
    ENSURE(ctx->record.boot_count == 0 && ctx->uptime_seconds == 0);
    return BOOT_COUNTER_OK;
}

boot_counter_err_t boot_counter_set_second_copy(boot_counter_t *ctx,
                                                boot_counter_storage_read_fn read2,
                                                boot_counter_storage_write_fn write2)
{
    if (!ctx || !read2 || !write2 || ctx->loaded) {
        return BOOT_COUNTER_ERR_PARAM;
    }

    ctx->read2 = read2;
    ctx->write2 = write2;
    return BOOT_COUNTER_OK;
}

boot_counter_err_t boot_counter_load(boot_counter_t *ctx)
{
    boot_counter_record_t second;
    bool first_ok;
    bool second_ok = false;

    if (!ctx) {
        return BOOT_COUNTER_ERR_PARAM;
    }

    /* Which copy is newest cannot be known with one of them unread. */
    if (read_copy(ctx, ctx->storage.read, &ctx->record, &first_ok) != BOOT_COUNTER_OK ||
        (ctx->read2 &&
         read_copy(ctx, ctx->read2, &second, &second_ok) != BOOT_COUNTER_OK)) {
        /* Storage read failed -- zero the record as safe default, and refuse
         * to write it: a save now would wipe a count that is still stored. */
        memset(&ctx->record, 0, sizeof(ctx->record));
        ctx->loaded = false;
        return BOOT_COUNTER_ERR_STORAGE;
    }

    ctx->stored_in_2 = second_ok &&
                       (!first_ok || seq_is_newer(second.seq, ctx->record.seq));
    if (ctx->stored_in_2) {
        ctx->record = second;
    } else if (!first_ok) {
        memset(&ctx->record, 0, sizeof(ctx->record));
    }
    ctx->stored_seq = ctx->record.seq;

    ctx->loaded = true;
    return BOOT_COUNTER_OK;
}

boot_counter_err_t boot_counter_increment(boot_counter_t *ctx)
{
    if (!ctx) {
        return BOOT_COUNTER_ERR_PARAM;
    }
    if (!ctx->loaded) {
        return BOOT_COUNTER_ERR_PARAM;
    }

    if (ctx->record.boot_count < UINT32_MAX) ctx->record.boot_count++;
    return save_record(ctx);
}

uint32_t boot_counter_get_count(const boot_counter_t *ctx)
{
    if (!ctx) {
        return 0;
    }
    return ctx->record.boot_count;
}

uint32_t boot_counter_get_reset_reason(const boot_counter_t *ctx)
{
    if (!ctx) {
        return 0;
    }
    return ctx->record.reset_reason;
}

boot_counter_err_t boot_counter_set_reset_reason(boot_counter_t *ctx,
                                                  uint32_t reason)
{
    if (!ctx) {
        return BOOT_COUNTER_ERR_PARAM;
    }
    if (!ctx->loaded) {
        return BOOT_COUNTER_ERR_PARAM;
    }

    ctx->record.reset_reason = reason;
    return save_record(ctx);
}

uint32_t boot_counter_get_uptime_seconds(const boot_counter_t *ctx)
{
    if (!ctx) {
        return 0;
    }
    return ctx->uptime_seconds;
}

void boot_counter_tick_second(boot_counter_t *ctx)
{
    if (ctx) {
        if (ctx->uptime_seconds < UINT32_MAX) ctx->uptime_seconds++;
    }
}
