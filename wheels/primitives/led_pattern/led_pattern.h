/*
 * led_pattern — LED blink pattern engine
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Drives LED on/off patterns from a simple descriptor array.
 * Caller ticks at fixed interval, engine returns on/off state.
 * Supports one-shot and looping patterns. Pure C99, no heap.
 * Thread safety: caller owns the led_pattern_t struct.
 */

#ifndef LED_PATTERN_H
#define LED_PATTERN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t on_ticks;
    uint16_t off_ticks;
} led_step_t;

typedef struct {
    const led_step_t *steps;
    uint8_t           step_count;
    bool              loop;
} led_pattern_cfg_t;

typedef struct {
    led_pattern_cfg_t cfg;
    uint8_t           current_step;
    uint16_t          tick_in_step;
    bool              is_on;
    bool              finished;
} led_pattern_t;

/** @thread_safe: no */
void led_pattern_init(led_pattern_t *p, const led_pattern_cfg_t *cfg);

/** Advance pattern by one tick and return current on/off state. @thread_safe: no */
bool led_pattern_tick(led_pattern_t *p);

/** Return true if the LED is currently on. @thread_safe: no */
bool led_pattern_is_on(const led_pattern_t *p);

/** Return true if a one-shot pattern has completed. @thread_safe: no */
bool led_pattern_finished(const led_pattern_t *p);

/** Reset pattern to the beginning. @thread_safe: no */
void led_pattern_restart(led_pattern_t *p);

#ifdef __cplusplus
}
#endif

#endif /* LED_PATTERN_H */
