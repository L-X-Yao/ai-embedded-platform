#include "test_harness.h"
#include "event_queue.h"
static event_queue_t q;
static event_t buf[8];

static void setup(void)
{
    event_queue_init(&q, buf, 8);
}

TEST(test_init)
{
    setup();
    ASSERT_TRUE(event_queue_is_empty(&q));
    ASSERT_TRUE(!event_queue_is_full(&q));
    ASSERT_EQ(event_queue_count(&q), 0);
}

TEST(test_null_safety)
{
    ASSERT_EQ(event_queue_init(NULL, buf, 8), EVENT_QUEUE_ERR_PARAM);
    ASSERT_EQ(event_queue_init(&q, NULL, 8), EVENT_QUEUE_ERR_PARAM);
    ASSERT_EQ(event_queue_init(&q, buf, 0), EVENT_QUEUE_ERR_PARAM);
    ASSERT_EQ(event_queue_push(NULL, 0, 0), EVENT_QUEUE_ERR_PARAM);
    event_t e;
    ASSERT_EQ(event_queue_pop(NULL, &e), EVENT_QUEUE_ERR_PARAM);
    ASSERT_EQ(event_queue_pop(&q, NULL), EVENT_QUEUE_ERR_PARAM);
    ASSERT_EQ(event_queue_peek(NULL, &e), EVENT_QUEUE_ERR_PARAM);
    ASSERT_EQ(event_queue_count(NULL), 0);
    ASSERT_TRUE(event_queue_is_empty(NULL));
    ASSERT_TRUE(!event_queue_is_full(NULL));
    ASSERT_EQ(event_queue_clear(NULL), EVENT_QUEUE_ERR_PARAM);
}

TEST(test_push_pop)
{
    setup();
    ASSERT_EQ(event_queue_push(&q, 1, 0x1234), EVENT_QUEUE_OK);
    ASSERT_EQ(event_queue_count(&q), 1);

    event_t e;
    ASSERT_EQ(event_queue_pop(&q, &e), EVENT_QUEUE_OK);
    ASSERT_EQ(e.type, 1);
    ASSERT_TRUE(e.data == 0x1234);
    ASSERT_TRUE(event_queue_is_empty(&q));
}

TEST(test_fifo_order)
{
    setup();
    event_queue_push(&q, 10, 0);
    event_queue_push(&q, 20, 0);
    event_queue_push(&q, 30, 0);

    event_t e;
    event_queue_pop(&q, &e); ASSERT_EQ(e.type, 10);
    event_queue_pop(&q, &e); ASSERT_EQ(e.type, 20);
    event_queue_pop(&q, &e); ASSERT_EQ(e.type, 30);
}

TEST(test_full)
{
    setup();
    for (int i = 0; i < 7; i++)
    {
        ASSERT_EQ(event_queue_push(&q, (uint16_t)i, 0), EVENT_QUEUE_OK);
    }
    ASSERT_TRUE(event_queue_is_full(&q));
    ASSERT_EQ(event_queue_push(&q, 99, 0), EVENT_QUEUE_ERR_FULL);
    ASSERT_EQ(event_queue_count(&q), 7);
}

TEST(test_empty_pop)
{
    setup();
    event_t e;
    ASSERT_EQ(event_queue_pop(&q, &e), EVENT_QUEUE_ERR_EMPTY);
}

TEST(test_peek)
{
    setup();
    event_queue_push(&q, 42, 0xBEEF);

    event_t e;
    ASSERT_EQ(event_queue_peek(&q, &e), EVENT_QUEUE_OK);
    ASSERT_EQ(e.type, 42);
    ASSERT_EQ(event_queue_count(&q), 1);
}

TEST(test_peek_empty)
{
    setup();
    event_t e;
    ASSERT_EQ(event_queue_peek(&q, &e), EVENT_QUEUE_ERR_EMPTY);
}

TEST(test_clear)
{
    setup();
    event_queue_push(&q, 1, 0);
    event_queue_push(&q, 2, 0);
    event_queue_clear(&q);
    ASSERT_TRUE(event_queue_is_empty(&q));
    ASSERT_EQ(event_queue_count(&q), 0);
}

TEST(test_wraparound)
{
    setup();
    for (int i = 0; i < 5; i++)
    {
        event_queue_push(&q, (uint16_t)i, 0);
    }
    event_t e;
    for (int i = 0; i < 5; i++)
    {
        event_queue_pop(&q, &e);
    }
    for (int i = 0; i < 7; i++)
    {
        ASSERT_EQ(event_queue_push(&q, (uint16_t)(100 + i), 0), EVENT_QUEUE_OK);
    }
    ASSERT_EQ(event_queue_count(&q), 7);

    event_queue_pop(&q, &e);
    ASSERT_EQ(e.type, 100);
}

TEST(test_data_payload)
{
    setup();
    event_queue_push(&q, 1, 0xDEADBEEF);

    event_t e;
    event_queue_pop(&q, &e);
    ASSERT_TRUE(e.data == 0xDEADBEEF);
}

TEST_MAIN("event_queue")
    RUN(test_init);
    RUN(test_null_safety);
    RUN(test_push_pop);
    RUN(test_fifo_order);
    RUN(test_full);
    RUN(test_empty_pop);
    RUN(test_peek);
    RUN(test_peek_empty);
    RUN(test_clear);
    RUN(test_wraparound);
    RUN(test_data_payload);
TEST_MAIN_END()

