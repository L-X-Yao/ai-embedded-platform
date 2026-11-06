// SPDX-License-Identifier: MIT
/*
 * button_handler -- high-level button event processor
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none -- takes a read callback, no direct wheel dependency)
 * Instances: multi
 *
 * Tick-driven pattern detector: click, double-click, long-press, repeat.
 * Register buttons with a read callback; call btn_handler_tick() periodically.
 * All timing in tick counts (caller decides tick rate, e.g. 10ms).
 * Pure C99, no heap, no OS, no timer HW.
 * Thread safety: caller owns the btn_handler_t struct.
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    BTN_HANDLER_OK         =  0,
    BTN_HANDLER_ERR_PARAM = -1,
    BTN_HANDLER_ERR_FULL   = -2,
} btn_handler_err_t;

/* ---------- events ---------- */

typedef enum {
    BTN_EVENT_NONE         = 0,
    BTN_EVENT_PRESS,
    BTN_EVENT_RELEASE,
    BTN_EVENT_CLICK,
    BTN_EVENT_DOUBLE_CLICK,
    BTN_EVENT_LONG_PRESS,
    BTN_EVENT_REPEAT,
} btn_event_t;

/* ---------- callbacks ---------- */

typedef bool (*btn_read_fn)(void *ctx);
typedef void (*btn_callback_fn)(void *ctx, uint8_t button_id, btn_event_t event);

/* ---------- compile-time config ---------- */

#ifndef BTN_HANDLER_MAX_BUTTONS
#define BTN_HANDLER_MAX_BUTTONS 8
#endif

/* ---------- per-button state ---------- */

typedef struct {
    btn_read_fn  read;
    void        *read_ctx;
    uint8_t      id;
    bool         last_state;
    bool         pressed;
    uint32_t     press_tick;
    uint32_t     low_tick;
    uint32_t     release_tick;
    uint8_t      click_count;
    bool         long_press_fired;
    uint32_t     repeat_tick;
} btn_entry_t;

/* ---------- handler instance ---------- */

typedef struct {
    btn_entry_t    buttons[BTN_HANDLER_MAX_BUTTONS];
    uint8_t        count;
    btn_callback_fn callback;
    void           *callback_ctx;
    uint32_t       tick;
    uint16_t       debounce_ticks;
    uint16_t       long_press_ticks;
    uint16_t       double_click_ticks;
    uint16_t       repeat_ticks;
} btn_handler_t;

/* ---------- API ---------- */

/** Initialize handler with timing thresholds. @thread_safe: no */
btn_handler_err_t btn_handler_init(btn_handler_t *bh, btn_callback_fn cb, void *ctx,
                                   uint16_t debounce_ticks, uint16_t long_press_ticks,
                                   uint16_t double_click_ticks, uint16_t repeat_ticks);

/** Register a button with read callback. @thread_safe: no */
btn_handler_err_t btn_handler_register(btn_handler_t *bh, uint8_t id,
                                       btn_read_fn read, void *ctx);

/** Advance state machine; call periodically (e.g. every 10ms). @thread_safe: no */
void btn_handler_tick(btn_handler_t *bh);

/** Return number of registered buttons. @thread_safe: yes (pure read) */
uint8_t btn_handler_count(const btn_handler_t *bh);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_HANDLER_H */
