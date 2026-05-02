#include "test_harness.h"
#include <string.h>
#include "fifo_queue.h"

/* ---- helpers ---------------------------------------------------------- */

typedef struct {
    uint16_t id;
    int32_t  value;
    uint8_t  flags;
} sample_t;   /* 7 bytes, odd size on purpose */

/* ---- tests ------------------------------------------------------------ */

TEST(test_init_ok)
{
    fifo_queue_t q;
    uint32_t storage[4];
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_init(&q, storage, 4, sizeof(uint32_t)));
    ASSERT_EQ(0, fifo_queue_count(&q));
    ASSERT_EQ(4, fifo_queue_free(&q));
    ASSERT_TRUE(fifo_queue_is_empty(&q));
    ASSERT_FALSE(fifo_queue_is_full(&q));
}

TEST(test_init_null_params)
{
    fifo_queue_t q;
    uint32_t storage[4];
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_init(NULL, storage, 4, sizeof(uint32_t)));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_init(&q, NULL, 4, sizeof(uint32_t)));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_init(&q, storage, 0, sizeof(uint32_t)));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_init(&q, storage, 4, 0));
}

TEST(test_push_pop_int)
{
    fifo_queue_t q;
    int storage[4];
    fifo_queue_init(&q, storage, 4, sizeof(int));

    int v1 = 42, v2 = -7, out = 0;
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v1));
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v2));
    ASSERT_EQ(2, fifo_queue_count(&q));

    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
    ASSERT_EQ(42, out);
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
    ASSERT_EQ(-7, out);
    ASSERT_TRUE(fifo_queue_is_empty(&q));
}

TEST(test_push_pop_struct)
{
    fifo_queue_t q;
    sample_t storage[3];
    fifo_queue_init(&q, storage, 3, sizeof(sample_t));

    sample_t s1 = { .id = 1, .value = -100, .flags = 0xAB };
    sample_t s2 = { .id = 2, .value = 9999, .flags = 0x00 };
    sample_t out;
    memset(&out, 0, sizeof(out));

    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &s1));
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &s2));

    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
    ASSERT_EQ(1, out.id);
    ASSERT_EQ(-100, out.value);
    ASSERT_EQ(0xAB, out.flags);

    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
    ASSERT_EQ(2, out.id);
    ASSERT_EQ(9999, out.value);
}

TEST(test_peek)
{
    fifo_queue_t q;
    int storage[4];
    fifo_queue_init(&q, storage, 4, sizeof(int));

    int v = 77, out = 0;
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v));
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_peek(&q, &out));
    ASSERT_EQ(77, out);
    /* peek must not remove the item */
    ASSERT_EQ(1, fifo_queue_count(&q));
}

TEST(test_full_error)
{
    fifo_queue_t q;
    uint8_t storage[3];
    fifo_queue_init(&q, storage, 3, sizeof(uint8_t));

    uint8_t v = 0;
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v)); v++;
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v)); v++;
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v));
    ASSERT_TRUE(fifo_queue_is_full(&q));
    ASSERT_EQ(FIFO_QUEUE_ERR_FULL, fifo_queue_push(&q, &v));
}

TEST(test_empty_error)
{
    fifo_queue_t q;
    int storage[2];
    fifo_queue_init(&q, storage, 2, sizeof(int));

    int out;
    ASSERT_EQ(FIFO_QUEUE_ERR_EMPTY, fifo_queue_pop(&q, &out));
    ASSERT_EQ(FIFO_QUEUE_ERR_EMPTY, fifo_queue_peek(&q, &out));
}

TEST(test_wraparound)
{
    fifo_queue_t q;
    int storage[3];
    fifo_queue_init(&q, storage, 3, sizeof(int));

    /* Fill, then drain 2, then push 2 more — forces tail wraparound */
    int v, out;
    v = 10; fifo_queue_push(&q, &v);
    v = 20; fifo_queue_push(&q, &v);
    v = 30; fifo_queue_push(&q, &v);

    fifo_queue_pop(&q, &out); ASSERT_EQ(10, out);
    fifo_queue_pop(&q, &out); ASSERT_EQ(20, out);

    v = 40; ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v));
    v = 50; ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v));

    fifo_queue_pop(&q, &out); ASSERT_EQ(30, out);
    fifo_queue_pop(&q, &out); ASSERT_EQ(40, out);
    fifo_queue_pop(&q, &out); ASSERT_EQ(50, out);
    ASSERT_TRUE(fifo_queue_is_empty(&q));
}

TEST(test_count_free)
{
    fifo_queue_t q;
    uint16_t storage[5];
    fifo_queue_init(&q, storage, 5, sizeof(uint16_t));

    uint16_t v = 1;
    fifo_queue_push(&q, &v);
    fifo_queue_push(&q, &v);
    ASSERT_EQ(2, fifo_queue_count(&q));
    ASSERT_EQ(3, fifo_queue_free(&q));
}

TEST(test_clear)
{
    fifo_queue_t q;
    int storage[4];
    fifo_queue_init(&q, storage, 4, sizeof(int));

    int v = 1;
    fifo_queue_push(&q, &v);
    fifo_queue_push(&q, &v);
    fifo_queue_clear(&q);

    ASSERT_TRUE(fifo_queue_is_empty(&q));
    ASSERT_EQ(0, fifo_queue_count(&q));
    ASSERT_EQ(4, fifo_queue_free(&q));
}

TEST(test_push_pop_null_params)
{
    fifo_queue_t q;
    int storage[2], v = 0;
    fifo_queue_init(&q, storage, 2, sizeof(int));

    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_push(NULL, &v));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_push(&q, NULL));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_pop(NULL, &v));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_pop(&q, NULL));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_peek(NULL, &v));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_peek(&q, NULL));
}

TEST(test_capacity_one)
{
    fifo_queue_t q;
    int storage[1];
    fifo_queue_init(&q, storage, 1, sizeof(int));

    int v = 99, out = 0;
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v));
    ASSERT_TRUE(fifo_queue_is_full(&q));
    ASSERT_EQ(FIFO_QUEUE_ERR_FULL, fifo_queue_push(&q, &v));

    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
    ASSERT_EQ(99, out);
    ASSERT_TRUE(fifo_queue_is_empty(&q));

    /* re-push after drain — verifies head/tail reset via modulus */
    v = 55;
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v));
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
    ASSERT_EQ(55, out);
}

TEST(test_large_elements)
{
    typedef struct { uint8_t data[64]; } big_t;
    fifo_queue_t q;
    big_t storage[2];
    fifo_queue_init(&q, storage, 2, sizeof(big_t));

    big_t a, b, out;
    memset(&a, 0xAA, sizeof(a));
    memset(&b, 0xBB, sizeof(b));

    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &a));
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &b));
    ASSERT_TRUE(fifo_queue_is_full(&q));

    memset(&out, 0, sizeof(out));
    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
    ASSERT_EQ(0, memcmp(&out, &a, sizeof(big_t)));

    ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
    ASSERT_EQ(0, memcmp(&out, &b, sizeof(big_t)));
}

TEST(test_interleaved_push_pop)
{
    fifo_queue_t q;
    int storage[3];
    fifo_queue_init(&q, storage, 3, sizeof(int));

    int v, out;
    /* push 1, pop 1, repeat — stresses head/tail tracking */
    for (int i = 0; i < 10; i++)
    {
        v = i * 3;
        ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_push(&q, &v));
        ASSERT_EQ(FIFO_QUEUE_OK, fifo_queue_pop(&q, &out));
        ASSERT_EQ(v, out);
    }
    ASSERT_TRUE(fifo_queue_is_empty(&q));
}

TEST(test_zero_init_safety)
{
    fifo_queue_t q;
    memset(&q, 0, sizeof(q));

    /* Operations on zero-init struct must not crash */
    ASSERT_EQ(0, fifo_queue_count(&q));
    ASSERT_TRUE(fifo_queue_is_empty(&q));
    ASSERT_FALSE(fifo_queue_is_full(&q));

    int out;
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_pop(&q, &out));
    ASSERT_EQ(FIFO_QUEUE_ERR_PARAMS, fifo_queue_peek(&q, &out));
}

TEST_MAIN("fifo_queue")
    RUN(test_init_ok);
    RUN(test_init_null_params);
    RUN(test_push_pop_int);
    RUN(test_push_pop_struct);
    RUN(test_peek);
    RUN(test_full_error);
    RUN(test_empty_error);
    RUN(test_wraparound);
    RUN(test_count_free);
    RUN(test_clear);
    RUN(test_push_pop_null_params);
    RUN(test_capacity_one);
    RUN(test_large_elements);
    RUN(test_interleaved_push_pop);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
