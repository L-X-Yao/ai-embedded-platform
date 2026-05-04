// SPDX-License-Identifier: MIT
/*
 * prop_keypad_matrix.c — property-based tests for matrix keypad scanner
 */

#include "test_harness.h"
#include "property_test.h"
#include "keypad_matrix.h"

/* --- GPIO mock --- */

static uint8_t mock_col_state[KEYPAD_MAX_COLS];
static uint8_t mock_active_row;

static void mock_set_row_low(void *ctx, uint8_t row)
{
    (void)ctx;
    mock_active_row = row;
}
static void mock_set_row_high(void *ctx, uint8_t row)
{
    (void)ctx; (void)row;
}
static bool mock_read_col(void *ctx, uint8_t col)
{
    (void)ctx;
    if (col >= KEYPAD_MAX_COLS) return true;
    /* When row driven low, active col reads low (pressed) */
    return mock_col_state[col] != 0;
}

static const keypad_hal_t prop_hal = {
    .set_row_low  = mock_set_row_low,
    .set_row_high = mock_set_row_high,
    .read_col     = mock_read_col,
    .ctx          = NULL,
};

static const char keymap_4x4[] = "123A456B789C*0#D";

PROPERTY(init_no_crash, 10000)
{
    memset(mock_col_state, 1, sizeof(mock_col_state));
    keypad_matrix_t kp;
    uint8_t rows = (uint8_t)(1 + ARBI_UINT8(seed) % KEYPAD_MAX_ROWS);
    uint8_t cols = (uint8_t)(1 + ARBI_UINT8(seed) % KEYPAD_MAX_COLS);
    keypad_matrix_init(&kp, &prop_hal, rows, cols, keymap_4x4);
}

PROPERTY(scan_no_crash, 5000)
{
    /* Set random column states */
    for (int i = 0; i < KEYPAD_MAX_COLS; i++)
        mock_col_state[i] = ARBI_UINT8(seed) & 1;

    keypad_matrix_t kp;
    keypad_matrix_init(&kp, &prop_hal, 4, 4, keymap_4x4);

    for (int i = 0; i < 5; i++) {
        /* Randomize col states between scans */
        for (int c = 0; c < KEYPAD_MAX_COLS; c++)
            mock_col_state[c] = ARBI_UINT8(seed) & 1;
        keypad_matrix_scan(&kp);

        char key;
        keypad_matrix_get_event(&kp, &key);
    }
}

TEST_MAIN("prop_keypad_matrix")
    RUN(init_no_crash);
    RUN(scan_no_crash);
TEST_MAIN_END()
