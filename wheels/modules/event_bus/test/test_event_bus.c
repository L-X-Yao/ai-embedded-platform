// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "event_bus.h"
#include <stdlib.h>

/* ---- test helpers ---- */

static uint16_t last_event_id;
static const void *last_data;
static size_t last_data_len;
static void *last_ctx;
static int call_count;

static void reset_tracker(void)
{
    last_event_id = 0;
    last_data = NULL;
    last_data_len = 0;
    last_ctx = NULL;
    call_count = 0;
}

static void tracker_cb(uint16_t event_id, const void *data,
                       size_t data_len, void *ctx)
{
    last_event_id = event_id;
    last_data = data;
    last_data_len = data_len;
    last_ctx = ctx;
    call_count++;
}

static int second_call_count;

static void second_cb(uint16_t event_id, const void *data,
                      size_t data_len, void *ctx)
{
    (void)event_id;
    (void)data;
    (void)data_len;
    (void)ctx;
    second_call_count++;
}

/* ---- tests ---- */

TEST(test_init)
{
    event_bus_t bus;
    ASSERT_EQ(event_bus_init(&bus), EVENT_BUS_OK);
}

TEST(test_init_null)
{
    ASSERT_EQ(event_bus_init(NULL), EVENT_BUS_ERR_PARAM);
}

TEST(test_subscribe_and_publish)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();

    int ctx_val = 42;
    ASSERT_EQ(event_bus_subscribe(&bus, 1, tracker_cb, &ctx_val), EVENT_BUS_OK);

    uint8_t payload = 0xAB;
    ASSERT_EQ(event_bus_publish(&bus, 1, &payload, sizeof(payload)), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 1);
    ASSERT_EQ(last_event_id, 1);
    ASSERT_TRUE(last_ctx == &ctx_val);
}

TEST(test_multiple_subscribers)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();
    second_call_count = 0;

    ASSERT_EQ(event_bus_subscribe(&bus, 5, tracker_cb, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_subscribe(&bus, 5, second_cb, NULL), EVENT_BUS_OK);

    ASSERT_EQ(event_bus_publish(&bus, 5, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 1);
    ASSERT_EQ(second_call_count, 1);
}

TEST(test_unsubscribe)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();

    ASSERT_EQ(event_bus_subscribe(&bus, 1, tracker_cb, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_unsubscribe(&bus, 1, tracker_cb, NULL), EVENT_BUS_OK);

    ASSERT_EQ(event_bus_publish(&bus, 1, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 0);
}

TEST(test_publish_no_subscribers)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();

    /* Publish to an event with no subscribers — should succeed silently. */
    ASSERT_EQ(event_bus_publish(&bus, 99, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 0);
}

TEST(test_max_event_types)
{
    event_bus_t bus;
    event_bus_init(&bus);

    for (uint16_t i = 0; i < EVENT_BUS_MAX_EVENTS; i++) {
        ASSERT_EQ(event_bus_subscribe(&bus, i, tracker_cb, NULL), EVENT_BUS_OK);
    }
    /* Next distinct event should fail. */
    ASSERT_EQ(event_bus_subscribe(&bus, EVENT_BUS_MAX_EVENTS, tracker_cb, NULL),
              EVENT_BUS_ERR_FULL);
}

TEST(test_max_subscribers_per_event)
{
    event_bus_t bus;
    event_bus_init(&bus);

    /* Use distinct ctx pointers to create unique subscriptions. */
    int ctx_arr[EVENT_BUS_MAX_SUBS_PER_EVENT + 1];
    for (int i = 0; i < EVENT_BUS_MAX_SUBS_PER_EVENT; i++) {
        ASSERT_EQ(event_bus_subscribe(&bus, 1, tracker_cb, &ctx_arr[i]),
                  EVENT_BUS_OK);
    }
    ASSERT_EQ(event_bus_subscribe(&bus, 1, tracker_cb,
                                  &ctx_arr[EVENT_BUS_MAX_SUBS_PER_EVENT]),
              EVENT_BUS_ERR_FULL);
}

TEST(test_data_passthrough)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();

    ASSERT_EQ(event_bus_subscribe(&bus, 10, tracker_cb, NULL), EVENT_BUS_OK);

    uint32_t payload = 0xDEADBEEF;
    ASSERT_EQ(event_bus_publish(&bus, 10, &payload, sizeof(payload)), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 1);
    ASSERT_TRUE(last_data == &payload);
    ASSERT_EQ((int)last_data_len, (int)sizeof(payload));
}

TEST(test_null_params)
{
    event_bus_t bus;
    event_bus_init(&bus);

    ASSERT_EQ(event_bus_subscribe(NULL, 1, tracker_cb, NULL), EVENT_BUS_ERR_PARAM);
    ASSERT_EQ(event_bus_subscribe(&bus, 1, NULL, NULL), EVENT_BUS_ERR_PARAM);
    ASSERT_EQ(event_bus_unsubscribe(NULL, 1, tracker_cb, NULL), EVENT_BUS_ERR_PARAM);
    ASSERT_EQ(event_bus_unsubscribe(&bus, 1, NULL, NULL), EVENT_BUS_ERR_PARAM);
    ASSERT_EQ(event_bus_publish(NULL, 1, NULL, 0), EVENT_BUS_ERR_PARAM);
}

TEST(test_publish_unregistered_event)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();

    /* Subscribe to event 1, publish to event 2. */
    ASSERT_EQ(event_bus_subscribe(&bus, 1, tracker_cb, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_publish(&bus, 2, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 0);
}

TEST(test_subscriber_context)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();

    int ctx_a = 100;
    int ctx_b = 200;
    ASSERT_EQ(event_bus_subscribe(&bus, 1, tracker_cb, &ctx_a), EVENT_BUS_OK);

    event_bus_publish(&bus, 1, NULL, 0);
    ASSERT_TRUE(last_ctx == &ctx_a);

    /* Unsubscribe ctx_a, subscribe ctx_b. */
    event_bus_unsubscribe(&bus, 1, tracker_cb, &ctx_a);
    event_bus_subscribe(&bus, 1, tracker_cb, &ctx_b);
    reset_tracker();

    event_bus_publish(&bus, 1, NULL, 0);
    ASSERT_TRUE(last_ctx == &ctx_b);
    ASSERT_EQ(call_count, 1);
}

TEST(test_unsubscribe_preserves_others)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();
    second_call_count = 0;

    ASSERT_EQ(event_bus_subscribe(&bus, 3, tracker_cb, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_subscribe(&bus, 3, second_cb, NULL), EVENT_BUS_OK);

    /* Remove first subscriber; second should still fire. */
    event_bus_unsubscribe(&bus, 3, tracker_cb, NULL);

    event_bus_publish(&bus, 3, NULL, 0);
    ASSERT_EQ(call_count, 0);
    ASSERT_EQ(second_call_count, 1);
}

TEST(test_multiple_events_isolation)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();
    second_call_count = 0;

    ASSERT_EQ(event_bus_subscribe(&bus, 1, tracker_cb, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_subscribe(&bus, 2, second_cb, NULL), EVENT_BUS_OK);

    event_bus_publish(&bus, 1, NULL, 0);
    ASSERT_EQ(call_count, 1);
    ASSERT_EQ(second_call_count, 0);

    event_bus_publish(&bus, 2, NULL, 0);
    ASSERT_EQ(call_count, 1);
    ASSERT_EQ(second_call_count, 1);
}

TEST(test_publish_null_data_with_zero_len)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();

    ASSERT_EQ(event_bus_subscribe(&bus, 7, tracker_cb, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_publish(&bus, 7, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 1);
    ASSERT_TRUE(last_data == NULL);
    ASSERT_EQ((int)last_data_len, 0);
}

/* Cover unsubscribe when event_id is not registered at all (line 99). */
TEST(test_unsubscribe_event_not_found)
{
    event_bus_t bus;
    event_bus_init(&bus);

    /* Never subscribed to event 55 — unsubscribe should return OK silently. */
    ASSERT_EQ(event_bus_unsubscribe(&bus, 55, tracker_cb, NULL), EVENT_BUS_OK);
}

/* Cover unsubscribe when (cb, ctx) is not in the slot (line 113). */
TEST(test_unsubscribe_no_cb_ctx_match)
{
    event_bus_t bus;
    event_bus_init(&bus);
    reset_tracker();

    int ctx_a = 1;
    int ctx_b = 2;
    /* Subscribe with ctx_a, then try to unsubscribe with ctx_b — no match. */
    ASSERT_EQ(event_bus_subscribe(&bus, 10, tracker_cb, &ctx_a), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_unsubscribe(&bus, 10, tracker_cb, &ctx_b), EVENT_BUS_OK);

    /* tracker_cb with ctx_a must still fire. */
    ASSERT_EQ(event_bus_publish(&bus, 10, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 1);
}

/* Shared state for the tests that unsubscribe during a publish. */
static event_bus_t *g_bus;
static int self_unsub_cb_count;
static int unsub_failures;

static void self_unsubscribing_cb(uint16_t event_id, const void *data,
                                  size_t data_len, void *ctx)
{
    (void)data;
    (void)data_len;
    self_unsub_cb_count++;
    event_bus_unsubscribe(g_bus, event_id, self_unsubscribing_cb, ctx);
}

/* It fires once, and the second publish must not call it again. */
TEST(test_unsubscribe_self_during_publish)
{
    event_bus_t bus;
    event_bus_init(&bus);
    g_bus = &bus;
    self_unsub_cb_count = 0;

    ASSERT_EQ(event_bus_subscribe(&bus, 20, self_unsubscribing_cb, NULL),
              EVENT_BUS_OK);

    ASSERT_EQ(event_bus_publish(&bus, 20, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(self_unsub_cb_count, 1);

    ASSERT_EQ(event_bus_publish(&bus, 20, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(self_unsub_cb_count, 1);
}

/* ctx counts the calls of one subscriber */
static void one_shot_cb(uint16_t event_id, const void *data, size_t data_len,
                        void *ctx)
{
    (void)data;
    (void)data_len;
    (*(int *)ctx)++;
    if (event_bus_unsubscribe(g_bus, event_id, one_shot_cb, ctx)
        != EVENT_BUS_OK) {
        unsub_failures++;
    }
}

/* A full table of subscribers can all leave in one publish. */
TEST(test_every_unsubscribe_during_publish_takes_effect)
{
    event_bus_t bus;
    int calls[EVENT_BUS_MAX_SUBS_PER_EVENT] = { 0 };

    event_bus_init(&bus);
    g_bus = &bus;
    unsub_failures = 0;
    for (int i = 0; i < EVENT_BUS_MAX_SUBS_PER_EVENT; i++) {
        ASSERT_EQ(event_bus_subscribe(&bus, 30, one_shot_cb, &calls[i]),
                  EVENT_BUS_OK);
    }

    ASSERT_EQ(event_bus_publish(&bus, 30, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_publish(&bus, 30, NULL, 0), EVENT_BUS_OK);

    ASSERT_EQ(unsub_failures, 0);
    for (int i = 0; i < EVENT_BUS_MAX_SUBS_PER_EVENT; i++) {
        ASSERT_EQ(calls[i], 1);
    }
}

/* Their places are free again once the publish has ended. */
TEST(test_unsubscribe_during_publish_frees_the_table)
{
    event_bus_t bus;
    int calls[EVENT_BUS_MAX_SUBS_PER_EVENT] = { 0 };

    event_bus_init(&bus);
    g_bus = &bus;
    for (int i = 0; i < EVENT_BUS_MAX_SUBS_PER_EVENT; i++) {
        ASSERT_EQ(event_bus_subscribe(&bus, 31, one_shot_cb, &calls[i]),
                  EVENT_BUS_OK);
    }
    ASSERT_EQ(event_bus_publish(&bus, 31, NULL, 0), EVENT_BUS_OK);

    for (int i = 0; i < EVENT_BUS_MAX_SUBS_PER_EVENT; i++) {
        ASSERT_EQ(event_bus_subscribe(&bus, 31, tracker_cb, &calls[i]),
                  EVENT_BUS_OK);
    }
    ASSERT_EQ(event_bus_subscribe(&bus, 31, tracker_cb, NULL),
              EVENT_BUS_ERR_FULL);
}

/* ctx is released on OK: ASan reports any later call */
static void releasing_cb(uint16_t event_id, const void *data, size_t data_len,
                         void *ctx)
{
    (void)data;
    (void)data_len;
    (*(int *)ctx)++;
    if (event_bus_unsubscribe(g_bus, event_id, releasing_cb, ctx)
        == EVENT_BUS_OK) {
        free(ctx);
    }
}

TEST(test_ctx_can_be_released_after_unsubscribe_returns_ok)
{
    event_bus_t bus;

    event_bus_init(&bus);
    g_bus = &bus;
    for (int i = 0; i < EVENT_BUS_MAX_SUBS_PER_EVENT; i++) {
        int *ctx = calloc(1, sizeof(*ctx));
        ASSERT_NOT_NULL(ctx);
        ASSERT_EQ(event_bus_subscribe(&bus, 32, releasing_cb, ctx),
                  EVENT_BUS_OK);
    }

    ASSERT_EQ(event_bus_publish(&bus, 32, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_publish(&bus, 32, NULL, 0), EVENT_BUS_OK);
}

typedef struct {
    uint16_t       event_id;
    event_bus_cb_t cb;
    void          *ctx;
} unsub_request_t;

/* ctx says which other subscriber to remove */
static void unsubscribing_other_cb(uint16_t event_id, const void *data,
                                   size_t data_len, void *ctx)
{
    const unsub_request_t *req = ctx;

    (void)event_id;
    (void)data;
    (void)data_len;
    event_bus_unsubscribe(g_bus, req->event_id, req->cb, req->ctx);
}

/* After OK its ctx may be gone, so even this publish must not call it. */
TEST(test_later_subscriber_removed_is_not_called_by_this_publish)
{
    event_bus_t bus;
    unsub_request_t req = { 33, tracker_cb, NULL };

    event_bus_init(&bus);
    g_bus = &bus;
    reset_tracker();
    second_call_count = 0;
    event_bus_subscribe(&bus, 33, unsubscribing_other_cb, &req);
    event_bus_subscribe(&bus, 33, tracker_cb, NULL);
    event_bus_subscribe(&bus, 33, second_cb, NULL);

    ASSERT_EQ(event_bus_publish(&bus, 33, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 0);
    ASSERT_EQ(second_call_count, 1);
}

/* One of two equal subscriptions goes, also from another event's publish. */
TEST(test_unsubscribe_from_another_events_publish_removes_one)
{
    event_bus_t bus;
    unsub_request_t req = { 35, tracker_cb, NULL };

    event_bus_init(&bus);
    g_bus = &bus;
    reset_tracker();
    event_bus_subscribe(&bus, 34, unsubscribing_other_cb, &req);
    event_bus_subscribe(&bus, 35, tracker_cb, NULL);
    event_bus_subscribe(&bus, 35, tracker_cb, NULL);

    ASSERT_EQ(event_bus_publish(&bus, 34, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 0);

    ASSERT_EQ(event_bus_publish(&bus, 35, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 1);
}

static char order_log[16];
static size_t order_len;

static void order_cb(uint16_t event_id, const void *data, size_t data_len,
                     void *ctx)
{
    (void)event_id;
    (void)data;
    (void)data_len;
    order_log[order_len++] = *(const char *)ctx;
    order_log[order_len] = '\0';
}

static void order_once_cb(uint16_t event_id, const void *data,
                          size_t data_len, void *ctx)
{
    order_cb(event_id, data, data_len, ctx);
    event_bus_unsubscribe(g_bus, event_id, order_once_cb, ctx);
}

TEST(test_order_survives_unsubscribe_during_publish)
{
    event_bus_t bus;
    char a = 'a', b = 'b', c = 'c';

    event_bus_init(&bus);
    g_bus = &bus;
    order_len = 0;
    event_bus_subscribe(&bus, 36, order_cb, &a);
    event_bus_subscribe(&bus, 36, order_once_cb, &b);
    event_bus_subscribe(&bus, 36, order_cb, &c);

    ASSERT_EQ(event_bus_publish(&bus, 36, NULL, 0), EVENT_BUS_OK);
    ASSERT_STR_EQ(order_log, "abc");
    ASSERT_EQ(event_bus_publish(&bus, 36, NULL, 0), EVENT_BUS_OK);
    ASSERT_STR_EQ(order_log, "abcac");
}

static int nesting_depth;

/* removes the subscriber before it, then publishes from inside the publish */
static void nesting_cb(uint16_t event_id, const void *data, size_t data_len,
                       void *ctx)
{
    (void)ctx;
    if (nesting_depth > 0) {
        return;
    }
    nesting_depth++;
    event_bus_unsubscribe(g_bus, event_id, tracker_cb, NULL);
    event_bus_publish(g_bus, event_id, data, data_len);
    nesting_depth--;
}

/* The outer loop is still walking the table when the inner publish ends. */
TEST(test_nested_publish_keeps_the_outer_loop_in_place)
{
    event_bus_t bus;

    event_bus_init(&bus);
    g_bus = &bus;
    reset_tracker();
    second_call_count = 0;
    nesting_depth = 0;
    event_bus_subscribe(&bus, 37, tracker_cb, NULL);
    event_bus_subscribe(&bus, 37, nesting_cb, NULL);
    event_bus_subscribe(&bus, 37, second_cb, NULL);

    ASSERT_EQ(event_bus_publish(&bus, 37, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 1);          /* the outer publish, before removal */
    ASSERT_EQ(second_call_count, 2);   /* inner and outer */

    ASSERT_EQ(event_bus_publish(&bus, 37, NULL, 0), EVENT_BUS_OK);
    ASSERT_EQ(call_count, 1);
    ASSERT_EQ(second_call_count, 4);
}

TEST_MAIN("event_bus")
    RUN(test_init);
    RUN(test_init_null);
    RUN(test_subscribe_and_publish);
    RUN(test_multiple_subscribers);
    RUN(test_unsubscribe);
    RUN(test_publish_no_subscribers);
    RUN(test_max_event_types);
    RUN(test_max_subscribers_per_event);
    RUN(test_data_passthrough);
    RUN(test_null_params);
    RUN(test_publish_unregistered_event);
    RUN(test_subscriber_context);
    RUN(test_unsubscribe_preserves_others);
    RUN(test_multiple_events_isolation);
    RUN(test_publish_null_data_with_zero_len);
    RUN(test_unsubscribe_event_not_found);
    RUN(test_unsubscribe_no_cb_ctx_match);
    RUN(test_unsubscribe_self_during_publish);
    RUN(test_every_unsubscribe_during_publish_takes_effect);
    RUN(test_unsubscribe_during_publish_frees_the_table);
    RUN(test_later_subscriber_removed_is_not_called_by_this_publish);
    RUN(test_unsubscribe_from_another_events_publish_removes_one);
    RUN(test_order_survives_unsubscribe_during_publish);
    RUN(test_nested_publish_keeps_the_outer_loop_in_place);
    RUN(test_ctx_can_be_released_after_unsubscribe_returns_ok);
TEST_MAIN_END()
