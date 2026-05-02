// SPDX-License-Identifier: MIT
/*
 * log_file — FILE* output sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      none
 * Instances: single
 */

#ifndef LOG_FILE_H
#define LOG_FILE_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FILE *fp;
    int   auto_flush;
    int   owned;
} log_file_t;

/* Open a log file for appending. Returns 0 on success, -1 on failure.
 * If auto_flush is non-zero, fflush() after every write.
 * @thread_safe: no — call during init */
int log_file_open(log_file_t *lf, const char *path, int auto_flush);

/* Attach to an existing FILE* (e.g. stderr). Does not own the FILE.
 * @thread_safe: no — call during init */
void log_file_attach(log_file_t *lf, FILE *fp, int auto_flush);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_file_t* as ctx when registering via log_add_sink().
 * @thread_safe: yes if underlying FILE* is thread-safe */
void log_file_sink(void *ctx, const char *line, size_t len);

/* Close the file (only if opened via log_file_open, not attach).
 * @thread_safe: no */
void log_file_close(log_file_t *lf);

#ifdef __cplusplus
}
#endif

#endif /* LOG_FILE_H */
