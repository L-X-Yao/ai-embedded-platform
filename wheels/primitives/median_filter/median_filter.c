// SPDX-License-Identifier: MIT
/*
 * median_filter — sliding window median filter
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */
#include "median_filter.h"
#include <string.h>

int median_filter_init(median_filter_t *f, uint8_t size)
{
    if (!f || size == 0 || size > MEDIAN_FILTER_MAX_SIZE)
        return -1;
    memset(f, 0, sizeof(*f));
    f->size = size;
    return 0;
}

static void rebuild_sorted(median_filter_t *f)
{
    uint8_t n = f->count;
    memcpy(f->sorted, f->window, n * sizeof(int32_t));
    for (uint8_t i = 1; i < n; i++) {
        int32_t key = f->sorted[i];
        int j = (int)i - 1;
        while (j >= 0 && f->sorted[j] > key) {
            f->sorted[j + 1] = f->sorted[j];
            j--;
        }
        f->sorted[j + 1] = key;
    }
}

int32_t median_filter_update(median_filter_t *f, int32_t sample)
{
    if (!f) return 0;
    f->window[f->index] = sample;
    f->index++;
    if (f->index >= f->size)
        f->index = 0;
    if (f->count < f->size)
        f->count++;

    rebuild_sorted(f);
    return f->sorted[f->count / 2];
}

int32_t median_filter_get(const median_filter_t *f)
{
    if (!f || f->count == 0) return 0;
    return f->sorted[f->count / 2];
}

void median_filter_reset(median_filter_t *f)
{
    if (!f) return;
    uint8_t size = f->size;
    memset(f, 0, sizeof(*f));
    f->size = size;
}
