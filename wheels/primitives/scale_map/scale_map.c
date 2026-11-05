// SPDX-License-Identifier: MIT
/*
 * scale_map — linear value mapping with optional clamping
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "scale_map.h"
#include <stdbool.h>

static uint64_t magnitude(int64_t v)
{
    return v < 0 ? (uint64_t)(-v) : (uint64_t)v;
}

int32_t scale_map_i32(int32_t x, int32_t in_min, int32_t in_max,
                       int32_t out_min, int32_t out_max)
{
    if (in_max == in_min)
        return out_min;

    /* Each difference can take 33 bits and their product 64, so multiply the
     * magnitudes, which fit in uint64_t, and put the sign back afterwards.
     * Division still truncates toward zero. */
    int64_t dx   = (int64_t)x - in_min;
    int64_t dout = (int64_t)out_max - out_min;
    int64_t din  = (int64_t)in_max - in_min;
    bool negative = ((dx < 0) != (dout < 0)) != (din < 0);
    uint64_t mag = magnitude(dx) * magnitude(dout) / magnitude(din);

    /* A result past int32_t saturates. Above this limit that holds whatever
     * out_min is, and below it the sum fits in int64_t. */
    if (mag > (uint64_t)INT32_MAX * 3u)
        return negative ? INT32_MIN : INT32_MAX;
    int64_t y = (negative ? -(int64_t)mag : (int64_t)mag) + out_min;
    if (y > INT32_MAX) return INT32_MAX;
    if (y < INT32_MIN) return INT32_MIN;
    return (int32_t)y;
}

int32_t scale_map_i32_clamp(int32_t x, int32_t in_min, int32_t in_max,
                             int32_t out_min, int32_t out_max)
{
    int32_t y = scale_map_i32(x, in_min, in_max, out_min, out_max);
    int32_t lo = out_min < out_max ? out_min : out_max;
    int32_t hi = out_min < out_max ? out_max : out_min;
    if (y < lo) return lo;
    if (y > hi) return hi;
    return y;
}

float scale_map_f(float x, float in_min, float in_max,
                   float out_min, float out_max)
{
    float denom = in_max - in_min;
    if (denom == 0.0f)
        return out_min;
    return (x - in_min) * (out_max - out_min) / denom + out_min;
}

float scale_map_f_clamp(float x, float in_min, float in_max,
                         float out_min, float out_max)
{
    float y = scale_map_f(x, in_min, in_max, out_min, out_max);
    float lo = out_min < out_max ? out_min : out_max;
    float hi = out_min < out_max ? out_max : out_min;
    if (y < lo) return lo;
    if (y > hi) return hi;
    return y;
}
