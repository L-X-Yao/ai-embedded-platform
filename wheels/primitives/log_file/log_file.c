// SPDX-License-Identifier: MIT
/*
 * log_file — FILE* output sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_file.h"
#include <stddef.h>

int log_file_open(log_file_t *lf, const char *path, int auto_flush) {
    if (!lf || !path) return -1;
    lf->fp = fopen(path, "ab");
    if (!lf->fp) return -1;
    lf->auto_flush = auto_flush;
    lf->owned = 1;
    return 0;
}

void log_file_attach(log_file_t *lf, FILE *fp, int auto_flush) {
    if (!lf) return;
    lf->fp = fp;
    lf->auto_flush = auto_flush;
    lf->owned = 0;
}

void log_file_sink(void *ctx, const char *line, size_t len) {
    log_file_t *lf = (log_file_t *)ctx;
    if (!lf || !lf->fp || !line || len == 0) return;
    fwrite(line, 1, len, lf->fp);
    if (lf->auto_flush) fflush(lf->fp);
}

void log_file_close(log_file_t *lf) {
    if (!lf || !lf->fp) return;
    if (lf->owned) fclose(lf->fp);
    lf->fp = NULL;
}
