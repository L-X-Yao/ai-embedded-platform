/*
 * error_handler -- fault logging and recovery module for embedded systems
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "error_handler.h"
#include <string.h>

/* ---------- Public API ---------- */

error_handler_err_t error_handler_init(error_handler_t *eh,
                                       const error_handler_storage_t *storage)
{
    if (!eh) {
        return ERROR_HANDLER_ERR_PARAMS;
    }

    memset(eh, 0, sizeof(*eh));
    eh->storage = storage;
    return ERROR_HANDLER_OK;
}

error_handler_err_t error_handler_report(error_handler_t *eh, uint16_t code,
                                          error_severity_t severity,
                                          uint32_t timestamp, uint32_t context)
{
    if (!eh) {
        return ERROR_HANDLER_ERR_PARAMS;
    }

    /* Write record at head position (ring buffer) */
    error_record_t *rec = &eh->records[eh->head];
    rec->timestamp = timestamp;
    rec->code      = code;
    rec->severity  = severity;
    rec->context   = context;

    /* Advance head with wrap */
    eh->head = (uint8_t)((eh->head + 1U) % ERROR_HANDLER_MAX_RECORDS);

    /* Track count (capped at max) */
    if (eh->count < ERROR_HANDLER_MAX_RECORDS) {
        eh->count++;
    }

    /* Lifetime counters */
    eh->total_errors++;
    if (severity == ERROR_SEVERITY_FATAL) {
        eh->total_fatals++;
        if (eh->on_fatal) {
            eh->on_fatal(eh->fatal_ctx, rec);
        }
    }

    return ERROR_HANDLER_OK;
}

void error_handler_set_fatal_callback(error_handler_t *eh,
                                       error_handler_callback_t cb, void *ctx)
{
    if (!eh) {
        return;
    }
    eh->on_fatal  = cb;
    eh->fatal_ctx = ctx;
}

uint8_t error_handler_get_count(const error_handler_t *eh)
{
    if (!eh) {
        return 0;
    }
    return eh->count;
}

error_handler_err_t error_handler_get(const error_handler_t *eh, uint8_t index,
                                       error_record_t *record)
{
    if (!eh || !record) {
        return ERROR_HANDLER_ERR_PARAMS;
    }
    if (index >= eh->count) {
        return ERROR_HANDLER_ERR_PARAMS;
    }

    /*
     * Oldest record position:
     *   - If buffer not full: index 0 in array is oldest
     *   - If buffer full: head points to oldest (it was just overwritten next)
     */
    uint8_t oldest;
    if (eh->count < ERROR_HANDLER_MAX_RECORDS) {
        oldest = 0;
    } else {
        oldest = eh->head;  /* head is where the next write goes = oldest */
    }

    uint8_t actual = (uint8_t)((oldest + index) % ERROR_HANDLER_MAX_RECORDS);
    *record = eh->records[actual];
    return ERROR_HANDLER_OK;
}

error_handler_err_t error_handler_save(error_handler_t *eh)
{
    if (!eh) {
        return ERROR_HANDLER_ERR_PARAMS;
    }
    if (!eh->storage || !eh->storage->save) {
        return ERROR_HANDLER_ERR_STORAGE;
    }

    /*
     * Linearize the ring buffer into a contiguous sequence for storage.
     * We pass records in oldest-first order.
     */
    if (eh->count == 0) {
        return eh->storage->save(eh->storage->ctx, eh->records, 0);
    }

    error_record_t linear[ERROR_HANDLER_MAX_RECORDS];
    for (uint8_t i = 0; i < eh->count; i++) {
        error_record_t rec;
        error_handler_get(eh, i, &rec);
        linear[i] = rec;
    }

    return eh->storage->save(eh->storage->ctx, linear, eh->count);
}

error_handler_err_t error_handler_load(error_handler_t *eh)
{
    if (!eh) {
        return ERROR_HANDLER_ERR_PARAMS;
    }
    if (!eh->storage || !eh->storage->load) {
        return ERROR_HANDLER_ERR_STORAGE;
    }

    uint8_t loaded = 0;
    error_handler_err_t err = eh->storage->load(eh->storage->ctx,
                                                 eh->records,
                                                 ERROR_HANDLER_MAX_RECORDS,
                                                 &loaded);
    if (err != ERROR_HANDLER_OK) {
        return err;
    }

    /* After load, records[0..loaded-1] are valid, oldest first */
    eh->count = loaded;
    eh->head  = (loaded < ERROR_HANDLER_MAX_RECORDS) ? loaded : 0;
    return ERROR_HANDLER_OK;
}

void error_handler_clear(error_handler_t *eh)
{
    if (!eh) {
        return;
    }
    memset(eh->records, 0, sizeof(eh->records));
    eh->count        = 0;
    eh->head         = 0;
    eh->total_errors = 0;
    eh->total_fatals = 0;
}

uint32_t error_handler_total_errors(const error_handler_t *eh)
{
    if (!eh) {
        return 0;
    }
    return eh->total_errors;
}

uint32_t error_handler_total_fatals(const error_handler_t *eh)
{
    if (!eh) {
        return 0;
    }
    return eh->total_fatals;
}
