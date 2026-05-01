#include "test_harness.h"
#include "button_handler.h"

/* ---------- mock infrastructure ---------- */

static bool mock_states[BTN_HANDLER_MAX_BUTTONS];

static bool mock_read_0(void *ctx) { (void)ctx; return mock_states[0]; }
static bool mock_read_1(void *ctx) { (void)ctx; return mock_states[1]; }

/* event log for callback verification */
#define MAX_EVENTS 64

typedef struct {
    uint8_t     id;
    btn_event_t event;
} event_record_t;

static event_record_t event_log[MAX_EVENTS];
static int event_count;

static void reset_events(void)
{
    event_count = 0;
}

static void test_callback(void *ctx, uint8_t button_id, btn_event_t event)
{
    (void)ctx;
    if (event_count < MAX_EVENTS) {
        event_log[event_count].id    = button_id;
        event_log[event_count].event = event;
        event_count++;
    }
}

/* callback that stores ctx value to verify it is passed through */
static void *received_ctx;

static void ctx_callback(void *ctx, uint8_t button_id, btn_event_t event)
{
    (void)button_id;
    (void)event;
    received_ctx = ctx;
}

/* ---------- helpers ---------- */

/* Timing constants (in ticks):
 *   debounce = 3, long_press = 100, double_click = 30, repeat = 20
 */
#define T_DEBOUNCE     3
#define T_LONG_PRESS 100
#define T_DBLCLICK    30
#define T_REPEAT      20

static void tick_n(btn_handler_t *bh, int n)
{
    for (int i = 0; i < n; i++) {
        btn_handler_tick(bh);
    }
}

static void init_default(btn_handler_t *bh)
{
    reset_events();
    for (int i = 0; i < BTN_HANDLER_MAX_BUTTONS; i++) {
        mock_states[i] = false;
    }
    btn_handler_init(bh, test_callback, NULL,
                     T_DEBOUNCE, T_LONG_PRESS, T_DBLCLICK, T_REPEAT);
}

/* find last event of a given type for a given id */
static bool has_event(uint8_t id, btn_event_t evt)
{
    for (int i = 0; i < event_count; i++) {
        if (event_log[i].id == id && event_log[i].event == evt) {
            return true;
        }
    }
    return false;
}

/* count occurrences of a specific event for a given id */
static int count_events(uint8_t id, btn_event_t evt)
{
    int c = 0;
    for (int i = 0; i < event_count; i++) {
        if (event_log[i].id == id && event_log[i].event == evt) {
            c++;
        }
    }
    return c;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    btn_handler_t bh;
    btn_handler_err_t err = btn_handler_init(&bh, test_callback, NULL,
                                             3, 100, 30, 20);
    ASSERT_EQ(err, BTN_HANDLER_OK);
    ASSERT_EQ(btn_handler_count(&bh), 0);
}

TEST(test_init_null_handler)
{
    btn_handler_err_t err = btn_handler_init(NULL, test_callback, NULL,
                                             3, 100, 30, 20);
    ASSERT_EQ(err, BTN_HANDLER_ERR_PARAMS);
}

TEST(test_init_null_callback)
{
    btn_handler_t bh;
    btn_handler_err_t err = btn_handler_init(&bh, NULL, NULL, 3, 100, 30, 20);
    ASSERT_EQ(err, BTN_HANDLER_ERR_PARAMS);
}

TEST(test_register_button)
{
    btn_handler_t bh;
    init_default(&bh);

    btn_handler_err_t err = btn_handler_register(&bh, 1, mock_read_0, NULL);
    ASSERT_EQ(err, BTN_HANDLER_OK);
    ASSERT_EQ(btn_handler_count(&bh), 1);
}

TEST(test_register_null_handler)
{
    btn_handler_err_t err = btn_handler_register(NULL, 1, mock_read_0, NULL);
    ASSERT_EQ(err, BTN_HANDLER_ERR_PARAMS);
}

TEST(test_register_null_read)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_err_t err = btn_handler_register(&bh, 1, NULL, NULL);
    ASSERT_EQ(err, BTN_HANDLER_ERR_PARAMS);
}

TEST(test_register_max_buttons)
{
    btn_handler_t bh;
    init_default(&bh);

    for (uint8_t i = 0; i < BTN_HANDLER_MAX_BUTTONS; i++) {
        btn_handler_err_t err = btn_handler_register(&bh, i, mock_read_0, NULL);
        ASSERT_EQ(err, BTN_HANDLER_OK);
    }
    ASSERT_EQ(btn_handler_count(&bh), BTN_HANDLER_MAX_BUTTONS);

    /* one more should fail */
    btn_handler_err_t err = btn_handler_register(&bh, 99, mock_read_0, NULL);
    ASSERT_EQ(err, BTN_HANDLER_ERR_FULL);
    ASSERT_EQ(btn_handler_count(&bh), BTN_HANDLER_MAX_BUTTONS);
}

TEST(test_simple_press_release)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* press */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1);
    ASSERT_TRUE(has_event(1, BTN_EVENT_PRESS));

    /* release */
    reset_events();
    mock_states[0] = false;
    tick_n(&bh, 1);
    ASSERT_TRUE(has_event(1, BTN_EVENT_RELEASE));
}

TEST(test_click_event)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* press past debounce */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1);

    /* release */
    mock_states[0] = false;
    tick_n(&bh, 1);

    /* wait for double-click window to expire */
    reset_events();
    tick_n(&bh, T_DBLCLICK + 1);

    ASSERT_TRUE(has_event(1, BTN_EVENT_CLICK));
    ASSERT_EQ(count_events(1, BTN_EVENT_CLICK), 1);
    ASSERT_FALSE(has_event(1, BTN_EVENT_DOUBLE_CLICK));
}

TEST(test_double_click_event)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* first press + release */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1);
    mock_states[0] = false;
    tick_n(&bh, 1);

    /* second press + release within double-click window */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1);
    mock_states[0] = false;
    tick_n(&bh, 1);

    /* wait for timeout */
    reset_events();
    tick_n(&bh, T_DBLCLICK + 1);

    ASSERT_TRUE(has_event(1, BTN_EVENT_DOUBLE_CLICK));
    ASSERT_EQ(count_events(1, BTN_EVENT_DOUBLE_CLICK), 1);
    /* should not also fire a single click */
    ASSERT_FALSE(has_event(1, BTN_EVENT_CLICK));
}

TEST(test_long_press)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* press past debounce */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1);

    /* hold until long-press */
    reset_events();
    tick_n(&bh, T_LONG_PRESS);

    ASSERT_TRUE(has_event(1, BTN_EVENT_LONG_PRESS));
    ASSERT_EQ(count_events(1, BTN_EVENT_LONG_PRESS), 1);
}

TEST(test_repeat_during_hold)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* press past debounce */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1);

    /* hold past long-press */
    tick_n(&bh, T_LONG_PRESS);

    /* hold for 3 repeat intervals */
    reset_events();
    tick_n(&bh, T_REPEAT * 3 + 1);

    int repeats = count_events(1, BTN_EVENT_REPEAT);
    ASSERT_TRUE(repeats >= 3);
}

TEST(test_debounce_rejects_short_pulse)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* pulse shorter than debounce */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE - 1);
    mock_states[0] = false;
    tick_n(&bh, T_DBLCLICK + 10);

    ASSERT_FALSE(has_event(1, BTN_EVENT_PRESS));
    ASSERT_FALSE(has_event(1, BTN_EVENT_CLICK));
}

TEST(test_release_after_long_press_no_click)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* press + hold until long-press */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1 + T_LONG_PRESS);

    /* release */
    reset_events();
    mock_states[0] = false;
    tick_n(&bh, 1);

    ASSERT_TRUE(has_event(1, BTN_EVENT_RELEASE));

    /* wait -- no click should come */
    reset_events();
    tick_n(&bh, T_DBLCLICK + 10);

    ASSERT_FALSE(has_event(1, BTN_EVENT_CLICK));
    ASSERT_FALSE(has_event(1, BTN_EVENT_DOUBLE_CLICK));
}

TEST(test_multiple_buttons)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 10, mock_read_0, NULL);
    btn_handler_register(&bh, 20, mock_read_1, NULL);

    /* press button 10 only */
    mock_states[0] = true;
    mock_states[1] = false;
    tick_n(&bh, T_DEBOUNCE + 1);

    ASSERT_TRUE(has_event(10, BTN_EVENT_PRESS));
    ASSERT_FALSE(has_event(20, BTN_EVENT_PRESS));

    /* now press button 20 too */
    reset_events();
    mock_states[1] = true;
    tick_n(&bh, T_DEBOUNCE + 1);

    ASSERT_TRUE(has_event(20, BTN_EVENT_PRESS));
}

TEST(test_callback_receives_correct_id)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 42, mock_read_0, NULL);

    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1);

    ASSERT_TRUE(event_count > 0);
    ASSERT_EQ(event_log[0].id, 42);
    ASSERT_EQ(event_log[0].event, BTN_EVENT_PRESS);
}

TEST(test_no_event_when_idle)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* run many ticks with button released */
    tick_n(&bh, 200);

    ASSERT_EQ(event_count, 0);
}

TEST(test_tick_without_buttons)
{
    btn_handler_t bh;
    init_default(&bh);

    /* should not crash with zero registered buttons */
    tick_n(&bh, 100);
    ASSERT_EQ(event_count, 0);
}

TEST(test_tick_null_handler)
{
    /* should not crash */
    btn_handler_tick(NULL);
}

TEST(test_count_null_handler)
{
    ASSERT_EQ(btn_handler_count(NULL), 0);
}

TEST(test_callback_ctx_passthrough)
{
    btn_handler_t bh;
    int sentinel = 0xBEEF;
    received_ctx = NULL;

    for (int i = 0; i < BTN_HANDLER_MAX_BUTTONS; i++) {
        mock_states[i] = false;
    }

    btn_handler_init(&bh, ctx_callback, &sentinel,
                     T_DEBOUNCE, T_LONG_PRESS, T_DBLCLICK, T_REPEAT);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1);

    ASSERT_TRUE(received_ctx == &sentinel);
}

TEST(test_triple_click)
{
    btn_handler_t bh;
    init_default(&bh);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* three rapid presses within double-click window */
    for (int i = 0; i < 3; i++) {
        mock_states[0] = true;
        tick_n(&bh, T_DEBOUNCE + 1);
        mock_states[0] = false;
        tick_n(&bh, 1);
    }

    /* wait for timeout */
    reset_events();
    tick_n(&bh, T_DBLCLICK + 1);

    /* triple = double_click + click */
    ASSERT_TRUE(has_event(1, BTN_EVENT_DOUBLE_CLICK));
    ASSERT_TRUE(has_event(1, BTN_EVENT_CLICK));
}

TEST(test_repeat_zero_no_repeat)
{
    btn_handler_t bh;
    reset_events();
    for (int i = 0; i < BTN_HANDLER_MAX_BUTTONS; i++) {
        mock_states[i] = false;
    }

    /* repeat_ticks = 0 means no repeat events */
    btn_handler_init(&bh, test_callback, NULL,
                     T_DEBOUNCE, T_LONG_PRESS, T_DBLCLICK, 0);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* press + hold past long-press + extra */
    mock_states[0] = true;
    tick_n(&bh, T_DEBOUNCE + 1 + T_LONG_PRESS + 100);

    ASSERT_TRUE(has_event(1, BTN_EVENT_LONG_PRESS));
    ASSERT_FALSE(has_event(1, BTN_EVENT_REPEAT));
}

TEST(test_timing_config)
{
    btn_handler_t bh;
    reset_events();
    for (int i = 0; i < BTN_HANDLER_MAX_BUTTONS; i++) {
        mock_states[i] = false;
    }

    /* custom: debounce=5, long=50, dblclick=20, repeat=10 */
    btn_handler_init(&bh, test_callback, NULL, 5, 50, 20, 10);
    btn_handler_register(&bh, 1, mock_read_0, NULL);

    /* press -- should debounce in 5 ticks, not 3 */
    mock_states[0] = true;
    tick_n(&bh, 4);
    ASSERT_FALSE(has_event(1, BTN_EVENT_PRESS));

    tick_n(&bh, 2);
    ASSERT_TRUE(has_event(1, BTN_EVENT_PRESS));

    /* long-press at 50, not 100 */
    reset_events();
    tick_n(&bh, 50);
    ASSERT_TRUE(has_event(1, BTN_EVENT_LONG_PRESS));
}

TEST_MAIN("button_handler")
    RUN(test_init_ok);
    RUN(test_init_null_handler);
    RUN(test_init_null_callback);
    RUN(test_register_button);
    RUN(test_register_null_handler);
    RUN(test_register_null_read);
    RUN(test_register_max_buttons);
    RUN(test_simple_press_release);
    RUN(test_click_event);
    RUN(test_double_click_event);
    RUN(test_long_press);
    RUN(test_repeat_during_hold);
    RUN(test_debounce_rejects_short_pulse);
    RUN(test_release_after_long_press_no_click);
    RUN(test_multiple_buttons);
    RUN(test_callback_receives_correct_id);
    RUN(test_no_event_when_idle);
    RUN(test_tick_without_buttons);
    RUN(test_tick_null_handler);
    RUN(test_count_null_handler);
    RUN(test_callback_ctx_passthrough);
    RUN(test_triple_click);
    RUN(test_repeat_zero_no_repeat);
    RUN(test_timing_config);
TEST_MAIN_END()
