// SPDX-License-Identifier: MIT
/*
 * keypad_matrix — GPIO matrix keypad scanner
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    gpio_hal (injected)
 * Thread safety: caller owns the keypad_matrix_t struct.
 */

#ifndef KEYPAD_MATRIX_H
#define KEYPAD_MATRIX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    KEYPAD_OK         =  0,
    KEYPAD_ERR_PARAM = -1,
} keypad_err_t;

/* ---------- event types ---------- */

typedef enum {
    KEYPAD_EVENT_NONE = 0,
    KEYPAD_EVENT_PRESS,
    KEYPAD_EVENT_RELEASE,
} keypad_event_t;

/* ---------- compile-time limits ---------- */

#ifndef KEYPAD_MAX_ROWS
#define KEYPAD_MAX_ROWS 4
#endif
#ifndef KEYPAD_MAX_COLS
#define KEYPAD_MAX_COLS 4
#endif

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void (*set_row_low)(void *ctx, uint8_t row);
    void (*set_row_high)(void *ctx, uint8_t row);
    bool (*read_col)(void *ctx, uint8_t col);
    void *ctx;
} keypad_hal_t;

/* ---------- instance state ---------- */

typedef struct {
    const keypad_hal_t *hal;
    const char *keymap;                  /* row*cols character map */
    uint8_t rows;
    uint8_t cols;
    uint8_t state[KEYPAD_MAX_ROWS];      /* current scan bitmask per row */
    uint8_t prev_state[KEYPAD_MAX_ROWS]; /* previous scan for edge detect */
    uint8_t debounce[KEYPAD_MAX_ROWS];   /* pending (unconfirmed) bitmask */
} keypad_matrix_t;

/* ---------- API ---------- */

/** Initialize keypad instance. @thread_safe: no */
keypad_err_t keypad_matrix_init(keypad_matrix_t *kp, const keypad_hal_t *hal,
                                uint8_t rows, uint8_t cols, const char *keymap);

/** Scan all keys. Call periodically (e.g. every 10-20 ms). @thread_safe: caller_must_lock */
void keypad_matrix_scan(keypad_matrix_t *kp);

/** Get event since last scan. Fills *key with keymap character. @thread_safe: caller_must_lock */
keypad_event_t keypad_matrix_get_event(keypad_matrix_t *kp, char *key);

/** Get currently pressed key character (0 if none). @thread_safe: caller_must_lock */
char keypad_matrix_get_pressed(const keypad_matrix_t *kp);

/** Check if a specific key is currently pressed. @thread_safe: caller_must_lock */
bool keypad_matrix_is_pressed(const keypad_matrix_t *kp, char key);

#ifdef __cplusplus
}
#endif

#endif /* KEYPAD_MATRIX_H */
