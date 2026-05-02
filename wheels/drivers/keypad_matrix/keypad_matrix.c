/*
 * keypad_matrix — GPIO matrix keypad scanner
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    gpio_hal (injected)
 *
 * Scan algorithm:
 *   For each row: drive row low, read all columns. A low column means
 *   the key at (row, col) is pressed. After scanning, drive row high again.
 *
 * Debounce:
 *   A raw scan result must match the pending state for two consecutive
 *   scans before it becomes the confirmed state. This filters glitches.
 */

#include "keypad_matrix.h"
#include <string.h>

keypad_err_t keypad_matrix_init(keypad_matrix_t *kp, const keypad_hal_t *hal,
                                uint8_t rows, uint8_t cols, const char *keymap)
{
    if (!kp || !hal || !keymap)
        return KEYPAD_ERR_PARAMS;
    if (rows == 0 || cols == 0)
        return KEYPAD_ERR_PARAMS;
    if (rows > KEYPAD_MAX_ROWS || cols > KEYPAD_MAX_COLS)
        return KEYPAD_ERR_PARAMS;
    if (!hal->set_row_low || !hal->set_row_high || !hal->read_col)
        return KEYPAD_ERR_PARAMS;

    kp->hal    = hal;
    kp->keymap = keymap;
    kp->rows   = rows;
    kp->cols   = cols;
    memset(kp->state, 0, sizeof(kp->state));
    memset(kp->prev_state, 0, sizeof(kp->prev_state));
    memset(kp->debounce, 0, sizeof(kp->debounce));
    return KEYPAD_OK;
}

void keypad_matrix_scan(keypad_matrix_t *kp)
{
    if (!kp || !kp->hal)
        return;

    /* Save current confirmed state as previous (for edge detection). */
    memcpy(kp->prev_state, kp->state, sizeof(kp->prev_state));

    for (uint8_t r = 0; r < kp->rows; r++) {
        /* Drive this row low. */
        kp->hal->set_row_low(kp->hal->ctx, r);

        /* Read each column. */
        uint8_t raw = 0;
        for (uint8_t c = 0; c < kp->cols; c++) {
            if (!kp->hal->read_col(kp->hal->ctx, c)) {
                /* Column reads low → key pressed (active low). */
                raw |= (uint8_t)(1U << c);
            }
        }

        /* Release row. */
        kp->hal->set_row_high(kp->hal->ctx, r);

        /* Debounce: raw must match pending for two consecutive scans. */
        if (raw == kp->debounce[r]) {
            /* Second consecutive identical scan → confirm. */
            kp->state[r] = raw;
        } else {
            /* Mismatch → update pending, keep previous confirmed state. */
            kp->debounce[r] = raw;
        }
    }
}

keypad_event_t keypad_matrix_get_event(keypad_matrix_t *kp, char *key)
{
    if (!kp)
        return KEYPAD_EVENT_NONE;

    for (uint8_t r = 0; r < kp->rows; r++) {
        uint8_t changed = kp->state[r] ^ kp->prev_state[r];
        if (changed == 0)
            continue;

        for (uint8_t c = 0; c < kp->cols; c++) {
            if (changed & (1U << c)) {
                if (key) {
                    *key = kp->keymap[(uint8_t)(r * kp->cols + c)];
                }
                /* Mark this key as consumed so the same event is not
                 * returned on the next call to get_event. */
                kp->prev_state[r] ^= (uint8_t)(1U << c);
                if (kp->state[r] & (1U << c)) {
                    return KEYPAD_EVENT_PRESS;
                } else {
                    return KEYPAD_EVENT_RELEASE;
                }
            }
        }
    }
    return KEYPAD_EVENT_NONE;
}

char keypad_matrix_get_pressed(const keypad_matrix_t *kp)
{
    if (!kp)
        return 0;

    for (uint8_t r = 0; r < kp->rows; r++) {
        if (kp->state[r] == 0)
            continue;
        for (uint8_t c = 0; c < kp->cols; c++) {
            if (kp->state[r] & (1U << c)) {
                return kp->keymap[(uint8_t)(r * kp->cols + c)];
            }
        }
    }
    return 0;
}

bool keypad_matrix_is_pressed(const keypad_matrix_t *kp, char key)
{
    if (!kp)
        return false;

    for (uint8_t r = 0; r < kp->rows; r++) {
        for (uint8_t c = 0; c < kp->cols; c++) {
            if (kp->keymap[(uint8_t)(r * kp->cols + c)] == key) {
                return (kp->state[r] & (1U << c)) != 0;
            }
        }
    }
    return false;
}
