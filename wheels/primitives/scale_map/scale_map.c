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

int32_t scale_map_i32(int32_t x, int32_t in_min, int32_t in_max,
                       int32_t out_min, int32_t out_max)
{
    if (in_max == in_min)
        return out_min;
    int64_t num = (int64_t)(x - in_min) * (int64_t)(out_max - out_min);
    return (int32_t)(num / (int64_t)(in_max - in_min)) + out_min;
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
