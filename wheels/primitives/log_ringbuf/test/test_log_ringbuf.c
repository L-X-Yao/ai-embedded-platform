#include "test_harness.h"
#include "log_ringbuf.h"

#include <string.h>

static uint8_t g_buf[128];
static log_ringbuf_t g_rb;

static void reset(void) {
    memset(g_buf, 0, sizeof(g_buf));
    log_ringbuf_init(&g_rb, g_buf, sizeof(g_buf));
}

TEST(test_init) {
    reset();
    ASSERT_EQ(log_ringbuf_used(&g_rb), 0);
    ASSERT_EQ(log_ringbuf_total(&g_rb), 0);
    ASSERT_EQ(log_ringbuf_dropped(&g_rb), 0);
}

TEST(test_write_and_read) {
    reset();
    const char *msg = "hello\n";
    size_t len = strlen(msg);
    log_ringbuf_write(&g_rb, msg, len);

    ASSERT_EQ(log_ringbuf_used(&g_rb), len);
    ASSERT_EQ(log_ringbuf_total(&g_rb), 1);
    ASSERT_EQ(log_ringbuf_dropped(&g_rb), 0);

    char out[64];
    size_t n = log_ringbuf_read(&g_rb, out, sizeof(out));
    ASSERT_EQ(n, len);
    ASSERT_MEMEQ(out, "hello\n");
}

TEST(test_multiple_writes) {
    reset();
    log_ringbuf_write(&g_rb, "AAA\n", 4);
    log_ringbuf_write(&g_rb, "BBB\n", 4);
    log_ringbuf_write(&g_rb, "CCC\n", 4);

    ASSERT_EQ(log_ringbuf_used(&g_rb), 12);
    ASSERT_EQ(log_ringbuf_total(&g_rb), 3);

    char out[64];
    size_t n = log_ringbuf_read(&g_rb, out, sizeof(out));
    ASSERT_EQ(n, 12);
    ASSERT_MEMEQ(out, "AAA\nBBB\nCCC\n");
}

TEST(test_overflow_evicts_oldest) {
    uint8_t small_buf[10];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, small_buf, sizeof(small_buf));

    log_ringbuf_write(&rb, "AAAA\n", 5);
    log_ringbuf_write(&rb, "BBBB\n", 5);
    ASSERT_EQ(log_ringbuf_used(&rb), 10);
    ASSERT_EQ(log_ringbuf_dropped(&rb), 0);

    log_ringbuf_write(&rb, "CCCC\n", 5);
    ASSERT_EQ(log_ringbuf_used(&rb), 10);
    ASSERT_EQ(log_ringbuf_dropped(&rb), 1);

    char out[32];
    size_t n = log_ringbuf_read(&rb, out, sizeof(out));
    ASSERT_EQ(n, 10);
    ASSERT_MEMEQ(out, "BBBB\nCCCC\n");
}

TEST(test_eviction_preserves_line_boundary) {
    uint8_t small_buf[16];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, small_buf, sizeof(small_buf));

    log_ringbuf_write(&rb, "AA\n", 3);
    log_ringbuf_write(&rb, "BBBB\n", 5);
    log_ringbuf_write(&rb, "CC\n", 3);
    ASSERT_EQ(log_ringbuf_used(&rb), 11);

    log_ringbuf_write(&rb, "DDDDDD\n", 7);
    ASSERT_EQ(log_ringbuf_dropped(&rb), 1);

    char out[32];
    size_t n = log_ringbuf_read(&rb, out, sizeof(out));
    out[n] = '\0';
    ASSERT_MEMEQ(out, "BBBB\nCC\nDDDDDD\n");
}

TEST(test_line_too_large_dropped) {
    uint8_t small_buf[4];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, small_buf, sizeof(small_buf));

    log_ringbuf_write(&rb, "toolong\n", 8);
    ASSERT_EQ(log_ringbuf_used(&rb), 0);
    ASSERT_EQ(log_ringbuf_total(&rb), 1);
    ASSERT_EQ(log_ringbuf_dropped(&rb), 1);
}

TEST(test_clear) {
    reset();
    log_ringbuf_write(&g_rb, "data\n", 5);
    ASSERT_EQ(log_ringbuf_used(&g_rb), 5);

    log_ringbuf_clear(&g_rb);
    ASSERT_EQ(log_ringbuf_used(&g_rb), 0);
    ASSERT_EQ(log_ringbuf_total(&g_rb), 1);
}

TEST(test_sink_adapter) {
    reset();
    log_ringbuf_sink(&g_rb, "[I] [app] boot\n", 15);
    ASSERT_EQ(log_ringbuf_used(&g_rb), 15);
    ASSERT_EQ(log_ringbuf_total(&g_rb), 1);

    char out[32];
    size_t n = log_ringbuf_read(&g_rb, out, sizeof(out));
    ASSERT_EQ(n, 15);
    ASSERT_MEMEQ(out, "[I] [app] boot\n");
}

TEST(test_read_partial) {
    reset();
    log_ringbuf_write(&g_rb, "long line here\n", 15);

    char out[5];
    size_t n = log_ringbuf_read(&g_rb, out, sizeof(out));
    ASSERT_EQ(n, 5);
    ASSERT_MEMEQ(out, "long ");
}

TEST(test_wraparound_read) {
    uint8_t small_buf[16];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, small_buf, sizeof(small_buf));

    log_ringbuf_write(&rb, "AAAAAAAAAAAA", 12);
    log_ringbuf_write(&rb, "BBBB", 4);
    ASSERT_EQ(log_ringbuf_used(&rb), 16);

    log_ringbuf_write(&rb, "CC", 2);

    char out[32];
    size_t n = log_ringbuf_read(&rb, out, sizeof(out));
    out[n] = '\0';
    ASSERT_TRUE(strstr(out, "CC") != NULL);
}

TEST(test_null_safety) {
    log_ringbuf_init(NULL, NULL, 0);
    log_ringbuf_write(NULL, "x", 1);
    log_ringbuf_sink(NULL, "x", 1);
    ASSERT_EQ(log_ringbuf_used(NULL), 0);
    ASSERT_EQ(log_ringbuf_total(NULL), 0);
    ASSERT_EQ(log_ringbuf_dropped(NULL), 0);
    log_ringbuf_clear(NULL);

    log_ringbuf_t rb;
    log_ringbuf_init(&rb, NULL, 0);
    log_ringbuf_write(&rb, "x", 1);
    ASSERT_EQ(log_ringbuf_used(&rb), 0);
}

TEST(test_fill_exact_capacity) {
    uint8_t small_buf[8];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, small_buf, sizeof(small_buf));

    log_ringbuf_write(&rb, "12345678", 8);
    ASSERT_EQ(log_ringbuf_used(&rb), 8);
    ASSERT_EQ(log_ringbuf_dropped(&rb), 0);

    char out[16];
    size_t n = log_ringbuf_read(&rb, out, sizeof(out));
    ASSERT_EQ(n, 8);
    ASSERT_MEMEQ(out, "12345678");
}

TEST_MAIN("log_ringbuf")
    RUN(test_init);
    RUN(test_write_and_read);
    RUN(test_multiple_writes);
    RUN(test_overflow_evicts_oldest);
    RUN(test_eviction_preserves_line_boundary);
    RUN(test_line_too_large_dropped);
    RUN(test_clear);
    RUN(test_sink_adapter);
    RUN(test_read_partial);
    RUN(test_wraparound_read);
    RUN(test_null_safety);
    RUN(test_fill_exact_capacity);
TEST_MAIN_END()
