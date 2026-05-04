/*
 * prop_command_queue.c — property-based tests for command_queue
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_command_queue.c ../command_queue.c -o prop_command_queue
 */

#include "test_harness.h"
#include "property_test.h"
#include "../command_queue.h"
#include <string.h>

#define Q_CAP 16
static command_queue_item_t q_buf[Q_CAP];

/* Property: random push/pop/peek sequences never crash, count stays valid */
PROPERTY(random_ops_no_crash, 10000) {
    command_queue_t q;
    command_queue_init(&q, q_buf, Q_CAP);

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 50);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 5;
        if (op == 0) {
            uint8_t data[COMMAND_QUEUE_DATA_MAX];
            uint8_t len = (uint8_t)ARBI_RANGE(seed, 0, COMMAND_QUEUE_DATA_MAX);
            ARBI_FILL(seed, data, len);
            command_queue_push(&q, ARBI_UINT16(seed),
                               ARBI_UINT8(seed), data, len);
        } else if (op == 1) {
            command_queue_item_t item;
            command_queue_pop(&q, &item);
        } else if (op == 2) {
            command_queue_item_t item;
            command_queue_peek(&q, &item);
        } else if (op == 3) {
            command_queue_clear(&q);
        } else {
            (void)command_queue_is_empty(&q);
            (void)command_queue_is_full(&q);
        }
    }

    size_t count = command_queue_count(&q);
    PROP_ASSERT(count <= Q_CAP);
}

/* Property: push then pop returns same item (FIFO within same priority) */
PROPERTY(push_pop_roundtrip, 10000) {
    command_queue_t q;
    command_queue_init(&q, q_buf, Q_CAP);

    uint16_t cmd_id = ARBI_UINT16(seed);
    uint8_t prio = ARBI_UINT8(seed);
    uint8_t data[8];
    uint8_t len = (uint8_t)ARBI_RANGE(seed, 0, 8);
    ARBI_FILL(seed, data, len);

    command_queue_err_t err = command_queue_push(&q, cmd_id, prio, data, len);
    PROP_ASSERT_EQ(err, COMMAND_QUEUE_OK);
    PROP_ASSERT_EQ(command_queue_count(&q), 1);

    command_queue_item_t item;
    err = command_queue_pop(&q, &item);
    PROP_ASSERT_EQ(err, COMMAND_QUEUE_OK);
    PROP_ASSERT_EQ(item.cmd_id, cmd_id);
    PROP_ASSERT_EQ(item.priority, prio);
    PROP_ASSERT_EQ(item.len, len);
    if (len > 0) {
        PROP_ASSERT_MEM_EQ(item.data, data, len);
    }
    PROP_ASSERT_EQ(command_queue_count(&q), 0);
}

/* Property: pop from higher priority first */
PROPERTY(priority_ordering, 10000) {
    command_queue_t q;
    command_queue_init(&q, q_buf, Q_CAP);

    uint8_t hi_prio = (uint8_t)ARBI_RANGE(seed, 0, 50);
    uint8_t lo_prio = (uint8_t)ARBI_RANGE(seed, 51, 255);

    command_queue_push(&q, 0xAAAA, lo_prio, NULL, 0);
    command_queue_push(&q, 0xBBBB, hi_prio, NULL, 0);

    command_queue_item_t item;
    command_queue_pop(&q, &item);
    PROP_ASSERT_EQ(item.cmd_id, 0xBBBB); /* higher priority (lower number) first */

    command_queue_pop(&q, &item);
    PROP_ASSERT_EQ(item.cmd_id, 0xAAAA);
}

/* Property: is_empty and is_full consistent with count */
PROPERTY(empty_full_consistent, 5000) {
    command_queue_t q;
    command_queue_init(&q, q_buf, Q_CAP);

    uint8_t pushes = (uint8_t)ARBI_RANGE(seed, 0, Q_CAP + 2);
    for (uint8_t i = 0; i < pushes; i++) {
        command_queue_push(&q, i, i, NULL, 0);
    }

    size_t count = command_queue_count(&q);
    PROP_ASSERT_EQ(command_queue_is_empty(&q), (count == 0));
    PROP_ASSERT_EQ(command_queue_is_full(&q), (count >= Q_CAP));
}

TEST_MAIN("command_queue properties") {
    RUN(random_ops_no_crash);
    RUN(push_pop_roundtrip);
    RUN(priority_ordering);
    RUN(empty_full_consistent);
}
TEST_MAIN_END()
