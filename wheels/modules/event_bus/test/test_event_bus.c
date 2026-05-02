#include "test_harness.h"
#include "event_bus.h"

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
TEST_MAIN_END()
