#include "test_harness.h"
#include <string.h>
#include "command_queue.h"

#define Q_CAP 8
static command_queue_item_t g_buf[Q_CAP];
static command_queue_t      g_q;

/* ── helpers ────────────────────────────────────────────────── */

static void setup(void)
{
    memset(&g_q, 0, sizeof(g_q));
    memset(g_buf, 0, sizeof(g_buf));
    command_queue_init(&g_q, g_buf, Q_CAP);
}

/* ── tests ──────────────────────────────────────────────────── */

TEST(test_init_and_empty)
{
    setup();
    ASSERT_TRUE(command_queue_is_empty(&g_q));
    ASSERT_FALSE(command_queue_is_full(&g_q));
    ASSERT_EQ(command_queue_count(&g_q), 0);
}

TEST(test_push_pop_single)
{
    setup();
    uint8_t payload[] = {0xAA, 0xBB};
    ASSERT_EQ(command_queue_push(&g_q, 42, 5, payload, 2), COMMAND_QUEUE_OK);
    ASSERT_EQ(command_queue_count(&g_q), 1);

    command_queue_item_t out;
    ASSERT_EQ(command_queue_pop(&g_q, &out), COMMAND_QUEUE_OK);
    ASSERT_EQ(out.cmd_id, 42);
    ASSERT_EQ(out.priority, 5);
    ASSERT_EQ(out.len, 2);
    ASSERT_EQ(out.data[0], 0xAA);
    ASSERT_EQ(out.data[1], 0xBB);
    ASSERT_TRUE(command_queue_is_empty(&g_q));
}

TEST(test_priority_ordering)
{
    setup();
    /* Push low-priority first, then high-priority */
    command_queue_push(&g_q, 1, 10, NULL, 0);  /* low prio */
    command_queue_push(&g_q, 2,  1, NULL, 0);  /* high prio */
    command_queue_push(&g_q, 3,  5, NULL, 0);  /* mid prio */

    command_queue_item_t out;
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 2);   /* prio 1 first */
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 3);   /* prio 5 next */
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 1);   /* prio 10 last */
}

TEST(test_fifo_within_same_priority)
{
    setup();
    command_queue_push(&g_q, 10, 3, NULL, 0);
    command_queue_push(&g_q, 20, 3, NULL, 0);
    command_queue_push(&g_q, 30, 3, NULL, 0);

    command_queue_item_t out;
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 10);
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 20);
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 30);
}

TEST(test_full_queue)
{
    setup();
    for (int i = 0; i < Q_CAP; i++) {
        ASSERT_EQ(command_queue_push(&g_q, (uint16_t)i, 5, NULL, 0),
                  COMMAND_QUEUE_OK);
    }
    ASSERT_TRUE(command_queue_is_full(&g_q));
    ASSERT_EQ(command_queue_push(&g_q, 99, 0, NULL, 0),
              COMMAND_QUEUE_ERR_FULL);
}

TEST(test_pop_empty)
{
    setup();
    command_queue_item_t out;
    ASSERT_EQ(command_queue_pop(&g_q, &out), COMMAND_QUEUE_ERR_EMPTY);
}

TEST(test_peek)
{
    setup();
    command_queue_push(&g_q, 7, 2, NULL, 0);
    command_queue_push(&g_q, 8, 1, NULL, 0);

    command_queue_item_t out;
    ASSERT_EQ(command_queue_peek(&g_q, &out), COMMAND_QUEUE_OK);
    ASSERT_EQ(out.cmd_id, 8);  /* highest priority */
    /* peek must not remove */
    ASSERT_EQ(command_queue_count(&g_q), 2);
}

TEST(test_peek_empty)
{
    setup();
    command_queue_item_t out;
    ASSERT_EQ(command_queue_peek(&g_q, &out), COMMAND_QUEUE_ERR_EMPTY);
}

TEST(test_clear)
{
    setup();
    command_queue_push(&g_q, 1, 0, NULL, 0);
    command_queue_push(&g_q, 2, 0, NULL, 0);
    ASSERT_EQ(command_queue_clear(&g_q), COMMAND_QUEUE_OK);
    ASSERT_TRUE(command_queue_is_empty(&g_q));
    ASSERT_EQ(command_queue_count(&g_q), 0);
}

TEST(test_null_params)
{
    /* init */
    command_queue_item_t buf[4];
    ASSERT_EQ(command_queue_init(NULL, buf, 4), COMMAND_QUEUE_ERR_PARAM);
    command_queue_t q;
    ASSERT_EQ(command_queue_init(&q, NULL, 4), COMMAND_QUEUE_ERR_PARAM);
    ASSERT_EQ(command_queue_init(&q, buf, 0), COMMAND_QUEUE_ERR_PARAM);

    /* push */
    ASSERT_EQ(command_queue_push(NULL, 1, 0, NULL, 0), COMMAND_QUEUE_ERR_PARAM);

    /* pop */
    command_queue_item_t out;
    ASSERT_EQ(command_queue_pop(NULL, &out), COMMAND_QUEUE_ERR_PARAM);
    setup();
    ASSERT_EQ(command_queue_pop(&g_q, NULL), COMMAND_QUEUE_ERR_PARAM);

    /* peek */
    ASSERT_EQ(command_queue_peek(NULL, &out), COMMAND_QUEUE_ERR_PARAM);

    /* clear */
    ASSERT_EQ(command_queue_clear(NULL), COMMAND_QUEUE_ERR_PARAM);
}

TEST(test_push_null_data_with_nonzero_len)
{
    setup();
    ASSERT_EQ(command_queue_push(&g_q, 1, 0, NULL, 5),
              COMMAND_QUEUE_ERR_PARAM);
}

TEST(test_push_data_too_long)
{
    setup();
    uint8_t big[COMMAND_QUEUE_DATA_MAX + 1];
    memset(big, 0, sizeof(big));
    ASSERT_EQ(command_queue_push(&g_q, 1, 0, big,
              (uint8_t)(COMMAND_QUEUE_DATA_MAX + 1)),
              COMMAND_QUEUE_ERR_PARAM);
}

TEST(test_push_zero_len_null_data_ok)
{
    setup();
    ASSERT_EQ(command_queue_push(&g_q, 1, 0, NULL, 0), COMMAND_QUEUE_OK);
    command_queue_item_t out;
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.len, 0);
}

TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
    command_queue_t zq;
    memset(&zq, 0, sizeof(zq));
    ASSERT_TRUE(command_queue_is_empty(&zq));
    ASSERT_FALSE(command_queue_is_full(&zq));
    ASSERT_EQ(command_queue_count(&zq), 0);
}

TEST(test_mixed_priority_interleave)
{
    /* Stress: interleaved pushes at different priorities */
    setup();
    command_queue_push(&g_q, 1, 5, NULL, 0);
    command_queue_push(&g_q, 2, 1, NULL, 0);
    command_queue_push(&g_q, 3, 3, NULL, 0);
    command_queue_push(&g_q, 4, 1, NULL, 0);  /* same prio as #2 */
    command_queue_push(&g_q, 5, 3, NULL, 0);  /* same prio as #3 */

    command_queue_item_t out;
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 2);  /* prio 1, first pushed */
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 4);  /* prio 1, second pushed (FIFO) */
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 3);  /* prio 3, first pushed */
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 5);  /* prio 3, second pushed (FIFO) */
    command_queue_pop(&g_q, &out);
    ASSERT_EQ(out.cmd_id, 1);  /* prio 5 */
}

TEST_MAIN("command_queue")
    RUN(test_init_and_empty);
    RUN(test_push_pop_single);
    RUN(test_priority_ordering);
    RUN(test_fifo_within_same_priority);
    RUN(test_full_queue);
    RUN(test_pop_empty);
    RUN(test_peek);
    RUN(test_peek_empty);
    RUN(test_clear);
    RUN(test_null_params);
    RUN(test_push_null_data_with_nonzero_len);
    RUN(test_push_data_too_long);
    RUN(test_push_zero_len_null_data_ok);
    RUN(test_zero_init_safety);
    RUN(test_mixed_priority_interleave);
TEST_MAIN_END()
