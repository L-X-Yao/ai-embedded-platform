#include <stdio.h>
#include "../event_queue.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((int)(a) == (int)(b), msg)

static event_queue_t q;
static event_t buf[8];

static void setup(void)
{
    event_queue_init(&q, buf, 8);
}

static void test_init(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT(event_queue_is_empty(&q), "empty after init");
    ASSERT(!event_queue_is_full(&q), "not full after init");
    ASSERT_EQ(event_queue_count(&q), 0, "count=0");
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    ASSERT_EQ(event_queue_init(NULL, buf, 8), EVENT_QUEUE_ERR_PARAM, "init null q");
    ASSERT_EQ(event_queue_init(&q, NULL, 8), EVENT_QUEUE_ERR_PARAM, "init null buf");
    ASSERT_EQ(event_queue_init(&q, buf, 0), EVENT_QUEUE_ERR_PARAM, "init zero cap");
    ASSERT_EQ(event_queue_push(NULL, 0, 0), EVENT_QUEUE_ERR_PARAM, "push null");
    event_t e;
    ASSERT_EQ(event_queue_pop(NULL, &e), EVENT_QUEUE_ERR_PARAM, "pop null q");
    ASSERT_EQ(event_queue_pop(&q, NULL), EVENT_QUEUE_ERR_PARAM, "pop null out");
    ASSERT_EQ(event_queue_peek(NULL, &e), EVENT_QUEUE_ERR_PARAM, "peek null");
    ASSERT_EQ(event_queue_count(NULL), 0, "count null");
    ASSERT(event_queue_is_empty(NULL), "empty null");
    ASSERT(!event_queue_is_full(NULL), "full null");
    ASSERT_EQ(event_queue_clear(NULL), EVENT_QUEUE_ERR_PARAM, "clear null");
}

static void test_push_pop(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(event_queue_push(&q, 1, 0x1234), EVENT_QUEUE_OK, "push ok");
    ASSERT_EQ(event_queue_count(&q), 1, "count=1");

    event_t e;
    ASSERT_EQ(event_queue_pop(&q, &e), EVENT_QUEUE_OK, "pop ok");
    ASSERT_EQ(e.type, 1, "type=1");
    ASSERT(e.data == 0x1234, "data=0x1234");
    ASSERT(event_queue_is_empty(&q), "empty after pop");
}

static void test_fifo_order(void)
{
    printf("%s\n", __func__);
    setup();
    event_queue_push(&q, 10, 0);
    event_queue_push(&q, 20, 0);
    event_queue_push(&q, 30, 0);

    event_t e;
    event_queue_pop(&q, &e); ASSERT_EQ(e.type, 10, "first=10");
    event_queue_pop(&q, &e); ASSERT_EQ(e.type, 20, "second=20");
    event_queue_pop(&q, &e); ASSERT_EQ(e.type, 30, "third=30");
}

static void test_full(void)
{
    printf("%s\n", __func__);
    setup();
    for (int i = 0; i < 7; i++)
    {
        ASSERT_EQ(event_queue_push(&q, (uint16_t)i, 0), EVENT_QUEUE_OK, "push");
    }
    ASSERT(event_queue_is_full(&q), "full at capacity-1");
    ASSERT_EQ(event_queue_push(&q, 99, 0), EVENT_QUEUE_ERR_FULL, "push when full");
    ASSERT_EQ(event_queue_count(&q), 7, "count=7");
}

static void test_empty_pop(void)
{
    printf("%s\n", __func__);
    setup();
    event_t e;
    ASSERT_EQ(event_queue_pop(&q, &e), EVENT_QUEUE_ERR_EMPTY, "pop from empty");
}

static void test_peek(void)
{
    printf("%s\n", __func__);
    setup();
    event_queue_push(&q, 42, 0xBEEF);

    event_t e;
    ASSERT_EQ(event_queue_peek(&q, &e), EVENT_QUEUE_OK, "peek ok");
    ASSERT_EQ(e.type, 42, "peek type");
    ASSERT_EQ(event_queue_count(&q), 1, "peek doesn't consume");
}

static void test_peek_empty(void)
{
    printf("%s\n", __func__);
    setup();
    event_t e;
    ASSERT_EQ(event_queue_peek(&q, &e), EVENT_QUEUE_ERR_EMPTY, "peek empty");
}

static void test_clear(void)
{
    printf("%s\n", __func__);
    setup();
    event_queue_push(&q, 1, 0);
    event_queue_push(&q, 2, 0);
    event_queue_clear(&q);
    ASSERT(event_queue_is_empty(&q), "empty after clear");
    ASSERT_EQ(event_queue_count(&q), 0, "count=0 after clear");
}

static void test_wraparound(void)
{
    printf("%s\n", __func__);
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
        ASSERT_EQ(event_queue_push(&q, (uint16_t)(100 + i), 0), EVENT_QUEUE_OK, "push after wrap");
    }
    ASSERT_EQ(event_queue_count(&q), 7, "count=7 after wrap");

    event_queue_pop(&q, &e);
    ASSERT_EQ(e.type, 100, "first after wrap = 100");
}

static void test_data_payload(void)
{
    printf("%s\n", __func__);
    setup();
    event_queue_push(&q, 1, 0xDEADBEEF);

    event_t e;
    event_queue_pop(&q, &e);
    ASSERT(e.data == 0xDEADBEEF, "data preserved");
}

int main(void)
{
    printf("test_event_queue\n");
    test_init();
    test_null_safety();
    test_push_pop();
    test_fifo_order();
    test_full();
    test_empty_pop();
    test_peek();
    test_peek_empty();
    test_clear();
    test_wraparound();
    test_data_payload();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
