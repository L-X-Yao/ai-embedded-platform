// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "keypad_matrix.h"

/* ---------- mock GPIO state ---------- */

/* Simulated column state per row: bit N set = column N pressed (active low). */
static uint16_t mock_col_state[KEYPAD_MAX_ROWS];

/* Track which row is currently driven low (-1 = none). */
static int8_t mock_active_row;

static void mock_set_row_low(void *ctx, uint8_t row)
{
    (void)ctx;
    mock_active_row = (int8_t)row;
}

static void mock_set_row_high(void *ctx, uint8_t row)
{
    (void)ctx;
    if (mock_active_row == (int8_t)row)
        mock_active_row = -1;
}

static bool mock_read_col(void *ctx, uint8_t col)
{
    (void)ctx;
    if (mock_active_row < 0)
        return true;  /* no row driven → high (not pressed) */
    /* Return false (low) if key pressed, true (high) if not. */
    return !(mock_col_state[mock_active_row] & (1U << col));
}

static const keypad_hal_t mock_hal = {
    .set_row_low  = mock_set_row_low,
    .set_row_high = mock_set_row_high,
    .read_col     = mock_read_col,
    .ctx          = NULL,
};

static const char keymap_4x4[] = "123A456B789C*0#D";
static const char keymap_3x3[] = "123456789";

static void mock_reset(void)
{
    memset(mock_col_state, 0, sizeof(mock_col_state));
    mock_active_row = -1;
}

/* Helper: press key at (row, col) in mock. */
static void mock_press(uint8_t row, uint8_t col)
{
    mock_col_state[row] |= (uint16_t)(1U << col);
}

/* Helper: release key at (row, col) in mock. */
static void mock_release(uint8_t row, uint8_t col)
{
    mock_col_state[row] &= (uint16_t)~(1U << col);
}

/* ---------- tests ---------- */

TEST(test_init_valid_4x4)
{
    keypad_matrix_t kp;
    mock_reset();
    ASSERT_EQ(keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4), KEYPAD_OK);
    ASSERT_EQ(kp.rows, 4);
    ASSERT_EQ(kp.cols, 4);
}

TEST(test_init_null_params)
{
    keypad_matrix_t kp;
    ASSERT_EQ(keypad_matrix_init(NULL, &mock_hal, 4, 4, keymap_4x4), KEYPAD_ERR_PARAM);
    ASSERT_EQ(keypad_matrix_init(&kp, NULL, 4, 4, keymap_4x4), KEYPAD_ERR_PARAM);
    ASSERT_EQ(keypad_matrix_init(&kp, &mock_hal, 4, 4, NULL), KEYPAD_ERR_PARAM);
}

TEST(test_init_rows_cols_exceed_max)
{
    keypad_matrix_t kp;
    ASSERT_EQ(keypad_matrix_init(&kp, &mock_hal, KEYPAD_MAX_ROWS + 1, 4, keymap_4x4),
              KEYPAD_ERR_PARAM);
    ASSERT_EQ(keypad_matrix_init(&kp, &mock_hal, 4, KEYPAD_MAX_COLS + 1, keymap_4x4),
              KEYPAD_ERR_PARAM);
    ASSERT_EQ(keypad_matrix_init(&kp, &mock_hal, 0, 4, keymap_4x4), KEYPAD_ERR_PARAM);
    ASSERT_EQ(keypad_matrix_init(&kp, &mock_hal, 4, 0, keymap_4x4), KEYPAD_ERR_PARAM);
}

TEST(test_no_key_pressed)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);
    /* Two scans for debounce. */
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    char key = '?';
    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_NONE);
    ASSERT_EQ(keypad_matrix_get_pressed(&kp), 0);
}

TEST(test_single_key_press)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);

    /* Press key at row=0, col=0 → '1'. */
    mock_press(0, 0);

    /* First scan: debounce pending, not yet confirmed. */
    keypad_matrix_scan(&kp);
    /* Second scan: debounce confirms. */
    keypad_matrix_scan(&kp);

    char key = 0;
    keypad_event_t evt = keypad_matrix_get_event(&kp, &key);
    ASSERT_EQ(evt, KEYPAD_EVENT_PRESS);
    ASSERT_EQ(key, '1');
}

TEST(test_key_release)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);

    /* Press and confirm. */
    mock_press(0, 2);  /* row=0, col=2 → '3' */
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    /* Consume press event. */
    char key = 0;
    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_PRESS);
    ASSERT_EQ(key, '3');

    /* Release and confirm. */
    mock_release(0, 2);
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    key = 0;
    keypad_event_t evt = keypad_matrix_get_event(&kp, &key);
    ASSERT_EQ(evt, KEYPAD_EVENT_RELEASE);
    ASSERT_EQ(key, '3');
}

TEST(test_debounce_single_scan_no_register)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);

    /* Press key. */
    mock_press(1, 1);

    /* Only one scan → should NOT confirm (debounce requires 2). */
    keypad_matrix_scan(&kp);

    char key = 0;
    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_NONE);
    ASSERT_EQ(keypad_matrix_get_pressed(&kp), 0);

    /* Second scan → confirms. */
    keypad_matrix_scan(&kp);
    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_PRESS);
    ASSERT_EQ(key, '5');
}

TEST(test_multiple_keys_returns_first)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);

    /* Press two keys simultaneously. */
    mock_press(0, 0);  /* '1' */
    mock_press(2, 2);  /* '9' */

    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    /* get_event returns the first detected (row 0 scanned first). */
    char key = 0;
    keypad_event_t evt = keypad_matrix_get_event(&kp, &key);
    ASSERT_EQ(evt, KEYPAD_EVENT_PRESS);
    ASSERT_EQ(key, '1');

    /* get_pressed also returns first. */
    ASSERT_EQ(keypad_matrix_get_pressed(&kp), '1');
}

TEST(test_keymap_mapping_all_positions)
{
    keypad_matrix_t kp;

    /* Verify each position in the 4x4 keymap. */
    for (uint8_t r = 0; r < 4; r++) {
        for (uint8_t c = 0; c < 4; c++) {
            mock_reset();
            keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);

            mock_press(r, c);
            keypad_matrix_scan(&kp);
            keypad_matrix_scan(&kp);

            char expected = keymap_4x4[r * 4 + c];
            char key = 0;
            keypad_event_t evt = keypad_matrix_get_event(&kp, &key);
            ASSERT_EQ(evt, KEYPAD_EVENT_PRESS);
            ASSERT_EQ(key, expected);
        }
    }
}

TEST(test_is_pressed_query)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);

    ASSERT_FALSE(keypad_matrix_is_pressed(&kp, '5'));

    /* Press row=1, col=1 → '5'. */
    mock_press(1, 1);
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    ASSERT_TRUE(keypad_matrix_is_pressed(&kp, '5'));
    ASSERT_FALSE(keypad_matrix_is_pressed(&kp, '1'));
    ASSERT_FALSE(keypad_matrix_is_pressed(&kp, 'A'));
}

TEST(test_get_pressed_returns_current)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);

    ASSERT_EQ(keypad_matrix_get_pressed(&kp), 0);

    mock_press(3, 3);  /* 'D' */
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    ASSERT_EQ(keypad_matrix_get_pressed(&kp), 'D');
}

TEST(test_3x3_non_square)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 3, 3, keymap_3x3);

    /* Press row=2, col=1 → keymap_3x3[2*3+1] = '8'. */
    mock_press(2, 1);
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    char key = 0;
    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_PRESS);
    ASSERT_EQ(key, '8');
    ASSERT_TRUE(keypad_matrix_is_pressed(&kp, '8'));
    ASSERT_EQ(keypad_matrix_get_pressed(&kp), '8');
}

TEST(test_rapid_press_release_cycle)
{
    keypad_matrix_t kp;
    mock_reset();
    keypad_matrix_init(&kp, &mock_hal, 4, 4, keymap_4x4);

    /* Press and confirm '7' (row=2, col=0). */
    mock_press(2, 0);
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    char key = 0;
    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_PRESS);
    ASSERT_EQ(key, '7');

    /* Release and confirm. */
    mock_release(2, 0);
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_RELEASE);
    ASSERT_EQ(key, '7');

    /* Press again and confirm. */
    mock_press(2, 0);
    keypad_matrix_scan(&kp);
    keypad_matrix_scan(&kp);

    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_PRESS);
    ASSERT_EQ(key, '7');

    /* No event after idle scan. */
    keypad_matrix_scan(&kp);
    ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_NONE);
}

TEST(test_init_missing_hal_functions)
{
    keypad_matrix_t kp;
    keypad_hal_t bad_hal = { .set_row_low = NULL, .set_row_high = NULL,
                             .read_col = NULL, .ctx = NULL };
    ASSERT_EQ(keypad_matrix_init(&kp, &bad_hal, 4, 4, keymap_4x4), KEYPAD_ERR_PARAM);

    /* Partially filled HAL. */
    bad_hal.set_row_low = mock_set_row_low;
    ASSERT_EQ(keypad_matrix_init(&kp, &bad_hal, 4, 4, keymap_4x4), KEYPAD_ERR_PARAM);

    bad_hal.set_row_high = mock_set_row_high;
    ASSERT_EQ(keypad_matrix_init(&kp, &bad_hal, 4, 4, keymap_4x4), KEYPAD_ERR_PARAM);
}

#if KEYPAD_MAX_COLS > 8
/* Only in test_wide_keypad_matrix, built with KEYPAD_MAX_COLS=10 */
TEST(test_every_column_reports_its_key)
{
    static const char keymap[] = "0123456789";
    keypad_matrix_t kp;

    for (uint8_t c = 0; c < 10; c++) {
        char key = 0;
        mock_reset();
        ASSERT_EQ(keypad_matrix_init(&kp, &mock_hal, 1, 10, keymap), KEYPAD_OK);

        mock_press(0, c);
        keypad_matrix_scan(&kp);
        keypad_matrix_scan(&kp);
        ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_PRESS);
        ASSERT_EQ(key, keymap[c]);
        ASSERT_EQ(keypad_matrix_get_pressed(&kp), keymap[c]);
        ASSERT_TRUE(keypad_matrix_is_pressed(&kp, keymap[c]));

        mock_release(0, c);
        keypad_matrix_scan(&kp);
        keypad_matrix_scan(&kp);
        ASSERT_EQ(keypad_matrix_get_event(&kp, &key), KEYPAD_EVENT_RELEASE);
        ASSERT_EQ(key, keymap[c]);
    }
}
#endif

TEST_MAIN("keypad_matrix")
    RUN(test_init_valid_4x4);
    RUN(test_init_null_params);
    RUN(test_init_rows_cols_exceed_max);
    RUN(test_no_key_pressed);
    RUN(test_single_key_press);
    RUN(test_key_release);
    RUN(test_debounce_single_scan_no_register);
    RUN(test_multiple_keys_returns_first);
    RUN(test_keymap_mapping_all_positions);
    RUN(test_is_pressed_query);
    RUN(test_get_pressed_returns_current);
    RUN(test_3x3_non_square);
    RUN(test_rapid_press_release_cycle);
    RUN(test_init_missing_hal_functions);
#if KEYPAD_MAX_COLS > 8
    RUN(test_every_column_reports_its_key);
#endif
TEST_MAIN_END()
