// SPDX-License-Identifier: MIT
/*
 * scale_map — linear value mapping with optional clamping
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Maps a value from one range to another: out = (x - in_min) * (out_max - out_min)
 * / (in_max - in_min) + out_min. Integer and float variants. Clamp variant
 * constrains output to [out_min, out_max].
 * Pure C99, no heap, no OS.
 */

#ifndef SCALE_MAP_H
#define SCALE_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Map int32 from [in_min,in_max] to [out_min,out_max]. @thread_safe: yes @isr_safe: yes */
int32_t scale_map_i32(int32_t x, int32_t in_min, int32_t in_max,
                       int32_t out_min, int32_t out_max);

/** Map int32 with output clamping. @thread_safe: yes @isr_safe: yes */
int32_t scale_map_i32_clamp(int32_t x, int32_t in_min, int32_t in_max,
                             int32_t out_min, int32_t out_max);

/** Map float. Returns 0 if in_min == in_max. @thread_safe: yes @isr_safe: yes */
float scale_map_f(float x, float in_min, float in_max,
                   float out_min, float out_max);

/** Map float with output clamping. @thread_safe: yes @isr_safe: yes */
float scale_map_f_clamp(float x, float in_min, float in_max,
                         float out_min, float out_max);

#ifdef __cplusplus
}
#endif

#endif /* SCALE_MAP_H */
