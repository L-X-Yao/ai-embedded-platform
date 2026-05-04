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

static boot_counter_err_t save_record(boot_counter_t *ctx)
{
    if (ctx->storage.write(
            (const uint8_t *)&ctx->record,
            sizeof(ctx->record),
            ctx->storage.user_ctx) != 0) {
        return BOOT_COUNTER_ERR_STORAGE;
    }
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

boot_counter_err_t boot_counter_load(boot_counter_t *ctx)
{
    if (!ctx) {
        return BOOT_COUNTER_ERR_PARAM;
    }

    if (ctx->storage.read(
            (uint8_t *)&ctx->record,
            sizeof(ctx->record),
            ctx->storage.user_ctx) != 0) {
        /* Storage read failed -- zero the record as safe default */
        memset(&ctx->record, 0, sizeof(ctx->record));
        return BOOT_COUNTER_ERR_STORAGE;
    }

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

    ctx->record.boot_count++;
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
        ctx->uptime_seconds++;
    }
}
