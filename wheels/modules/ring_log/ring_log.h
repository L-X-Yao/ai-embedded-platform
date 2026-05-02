// SPDX-License-Identifier: MIT
/*
 * ring_log — ring buffer structured log storage with read-back
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#ifndef RING_LOG_H
#define RING_LOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Error codes ---- */
typedef enum {
    RING_LOG_OK           =  0,
    RING_LOG_ERR_PARAM   = -1,
    RING_LOG_ERR_FULL     = -2,   /* (reserved — current impl overwrites) */
    RING_LOG_ERR_EMPTY    = -3,
    RING_LOG_ERR_FILTERED = -4,   /* entry below min_level, not stored   */
} ring_log_err_t;

/* ---- Log levels ---- */
typedef enum {
    RING_LOG_DEBUG = 0,
    RING_LOG_INFO,
    RING_LOG_WARN,
    RING_LOG_ERROR,
} ring_log_level_t;

/* ---- Stored entry ---- */
typedef struct {
    uint32_t          timestamp;
    ring_log_level_t  level;
    char              message[64];
} ring_log_entry_t;

/* ---- Compile-time capacity ---- */
#ifndef RING_LOG_MAX_ENTRIES
#define RING_LOG_MAX_ENTRIES 32
#endif

/* ---- Instance (caller-allocated, no malloc) ---- */
typedef struct {
    ring_log_entry_t  entries[RING_LOG_MAX_ENTRIES]; /* internal */
    uint16_t          head;          /* internal: next write slot */
    uint16_t          count;         /* internal: entries currently stored */
    uint32_t          total_logged;  /* internal: lifetime writes */
    uint32_t          dropped;       /* internal: writes rejected by level */
    ring_log_level_t  min_level;     /* internal: current filter threshold */
} ring_log_t;

/* ---- API (all @thread_safe: no — caller must serialize) ---- */

/** Initialize ring log instance. @thread_safe: no */
ring_log_err_t ring_log_init(ring_log_t *rl, ring_log_level_t min_level);

/** Write a formatted log entry. @thread_safe: no */
ring_log_err_t ring_log_write(ring_log_t *rl, ring_log_level_t level,
                              uint32_t timestamp, const char *fmt, ...);

/** Read entry by absolute index [0..count-1], 0 = newest. @thread_safe: no */
ring_log_err_t ring_log_read(const ring_log_t *rl, uint16_t index,
                             ring_log_entry_t *entry);

/** Read nth oldest entry (0 = oldest, count-1 = newest). @thread_safe: no */
ring_log_err_t ring_log_read_oldest(const ring_log_t *rl, uint16_t nth,
                                    ring_log_entry_t *entry);

/** Number of entries currently stored. @thread_safe: no */
uint16_t ring_log_count(const ring_log_t *rl);

/** Lifetime total writes (including overwrites). @thread_safe: no */
uint32_t ring_log_total(const ring_log_t *rl);

/** Writes rejected because level < min_level. @thread_safe: no */
uint32_t ring_log_dropped(const ring_log_t *rl);

/** Clear all entries and counters. @thread_safe: no */
void ring_log_clear(ring_log_t *rl);

/** Change minimum log level at runtime. @thread_safe: no */
ring_log_err_t ring_log_set_level(ring_log_t *rl, ring_log_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* RING_LOG_H */
