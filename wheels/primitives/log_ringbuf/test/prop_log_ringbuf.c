/*
 * prop_log_ringbuf.c — property-based tests for log_ringbuf
 *
 * Requires: property_test.h from tooling/test/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/test prop_log_ringbuf.c ../log_ringbuf.c -o prop_log_ringbuf
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_ringbuf.h"
#include <string.h>

#define BUF_CAP 256

/* Property: sink never crashes on arbitrary input */
PROPERTY(no_crash, 10000) {
    uint8_t storage[BUF_CAP];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, storage, BUF_CAP);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 30);
    for (unsigned i = 0; i < ops; i++) {
        char line[64];
        unsigned len = (unsigned)ARBI_RANGE(seed, 0, 63);
        ARBI_FILL(seed, (uint8_t *)line, len);
        for (unsigned j = 0; j < len; j++) {
            if (line[j] == '\0') line[j] = 'A';
        }
        log_ringbuf_sink(&rb, line, len);
    }
    PROP_ASSERT(1);
}

/* Property: used never exceeds capacity */
PROPERTY(used_le_capacity, 10000) {
    uint8_t storage[BUF_CAP];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, storage, BUF_CAP);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 50);
    for (unsigned i = 0; i < ops; i++) {
        char line[32];
        unsigned len = (unsigned)ARBI_RANGE(seed, 1, 31);
        ARBI_FILL(seed, (uint8_t *)line, len);
        for (unsigned j = 0; j < len; j++) {
            if (line[j] == '\0') line[j] = 'A';
        }
        log_ringbuf_write(&rb, line, len);
        PROP_ASSERT(log_ringbuf_used(&rb) <= BUF_CAP);
    }
}

/* Property: total lines always increments by 1 per write */
PROPERTY(total_increments, 5000) {
    uint8_t storage[BUF_CAP];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, storage, BUF_CAP);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 40);
    for (unsigned i = 0; i < ops; i++) {
        uint32_t before = log_ringbuf_total(&rb);
        log_ringbuf_write(&rb, "x", 1);
        PROP_ASSERT_EQ(log_ringbuf_total(&rb), before + 1);
    }
}

/* Property: clear resets used to 0 but preserves counters */
PROPERTY(clear_preserves_counters, 5000) {
    uint8_t storage[BUF_CAP];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, storage, BUF_CAP);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 20);
    for (unsigned i = 0; i < ops; i++) {
        log_ringbuf_write(&rb, "data", 4);
    }

    uint32_t total_before = log_ringbuf_total(&rb);
    uint32_t dropped_before = log_ringbuf_dropped(&rb);

    log_ringbuf_clear(&rb);

    PROP_ASSERT_EQ(log_ringbuf_used(&rb), 0);
    PROP_ASSERT_EQ(log_ringbuf_total(&rb), total_before);
    PROP_ASSERT_EQ(log_ringbuf_dropped(&rb), dropped_before);
}

/* Property: read returns data that was written */
PROPERTY(write_read_roundtrip, 5000) {
    uint8_t storage[BUF_CAP];
    log_ringbuf_t rb;
    log_ringbuf_init(&rb, storage, BUF_CAP);

    /* Write a short known line */
    const char *line = "hello world";
    size_t line_len = strlen(line);
    log_ringbuf_write(&rb, line, line_len);

    char dst[BUF_CAP];
    size_t read = log_ringbuf_read(&rb, dst, sizeof(dst));
    PROP_ASSERT(read > 0);
    /* The written content should be present in the read output */
    PROP_ASSERT(strstr(dst, "hello world") != NULL);
}

TEST_MAIN("log_ringbuf properties") {
    RUN(no_crash);
    RUN(used_le_capacity);
    RUN(total_increments);
    RUN(clear_preserves_counters);
    RUN(write_read_roundtrip);
}
TEST_MAIN_END()
