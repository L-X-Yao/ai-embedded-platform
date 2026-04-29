/*
 * median_filter — sliding window median filter
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Caller provides the window buffer. Uses insertion sort on each sample
 * for O(N) update. Window sizes 3-31 (odd recommended for true median).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MEDIAN_FILTER_H
#define MEDIAN_FILTER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MEDIAN_FILTER_MAX_SIZE
#define MEDIAN_FILTER_MAX_SIZE 31
#endif

typedef struct {
    int32_t  window[MEDIAN_FILTER_MAX_SIZE];
    int32_t  sorted[MEDIAN_FILTER_MAX_SIZE];
    uint8_t  size;
    uint8_t  count;
    uint8_t  index;
} median_filter_t;

int median_filter_init(median_filter_t *f, uint8_t size);

int32_t median_filter_update(median_filter_t *f, int32_t sample);

int32_t median_filter_get(const median_filter_t *f);

void median_filter_reset(median_filter_t *f);

#ifdef __cplusplus
}
#endif

#endif /* MEDIAN_FILTER_H */
