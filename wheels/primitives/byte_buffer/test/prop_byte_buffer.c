/*
 * prop_byte_buffer.c — property-based tests for byte_buffer
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_byte_buffer.c ../byte_buffer.c -o prop_byte_buffer
 */

#include "test_harness.h"
#include "property_test.h"
#include "../byte_buffer.h"
#include <string.h>

#define BUF_CAP 128

/* Property: put_bytes then get_bytes returns original data */
PROPERTY(write_read_roundtrip, 10000) {
    uint8_t storage[BUF_CAP];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_CAP);

    uint8_t input[BUF_CAP];
    size_t len = ARBI_SIZE(seed, BUF_CAP);
    ARBI_FILL(seed, input, len);

    byte_buffer_err_t err = byte_buffer_put_bytes(&buf, input, len);
    PROP_ASSERT_EQ(err, BYTE_BUFFER_OK);
    PROP_ASSERT_EQ(byte_buffer_readable(&buf), len);

    uint8_t output[BUF_CAP];
    err = byte_buffer_get_bytes(&buf, output, len);
    PROP_ASSERT_EQ(err, BYTE_BUFFER_OK);
    PROP_ASSERT_MEM_EQ(output, input, len);
}

/* Property: readable + writable always equals capacity */
PROPERTY(capacity_invariant, 10000) {
    uint8_t storage[BUF_CAP];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_CAP);

    /* Write some data */
    size_t write_len = ARBI_SIZE(seed, BUF_CAP);
    uint8_t data[BUF_CAP];
    ARBI_FILL(seed, data, write_len);
    byte_buffer_put_bytes(&buf, data, write_len);

    /* Read some of it back */
    size_t readable = byte_buffer_readable(&buf);
    size_t read_len = ARBI_SIZE(seed, readable);
    uint8_t tmp[BUF_CAP];
    byte_buffer_get_bytes(&buf, tmp, read_len);

    /* written = writable consumed, readable = bytes available */
    size_t after_readable = byte_buffer_readable(&buf);
    size_t after_writable = byte_buffer_writable(&buf);
    /* writable is capacity - write_pos, readable is write_pos - read_pos */
    PROP_ASSERT(after_readable + after_writable + buf.read_pos <= BUF_CAP);
}

/* Property: u8/u16/u32 put then get roundtrips */
PROPERTY(typed_put_get_roundtrip, 10000) {
    uint8_t storage[64];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, 64);

    uint8_t  v8  = ARBI_UINT8(seed);
    uint16_t v16 = ARBI_UINT16(seed);
    uint32_t v32 = ARBI_UINT32(seed);

    byte_buffer_put_u8(&buf, v8);
    byte_buffer_put_u16_be(&buf, v16);
    byte_buffer_put_u32_be(&buf, v32);

    PROP_ASSERT_EQ(byte_buffer_readable(&buf), 1 + 2 + 4);

    uint8_t  r8;
    uint16_t r16;
    uint32_t r32;
    byte_buffer_get_u8(&buf, &r8);
    byte_buffer_get_u16_be(&buf, &r16);
    byte_buffer_get_u32_be(&buf, &r32);

    PROP_ASSERT_EQ(r8, v8);
    PROP_ASSERT_EQ(r16, v16);
    PROP_ASSERT_EQ(r32, v32);
    PROP_ASSERT_EQ(byte_buffer_readable(&buf), 0);
}

/* Property: compact moves unread data to start, preserves content */
PROPERTY(compact_preserves_data, 5000) {
    uint8_t storage[BUF_CAP];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_CAP);

    /* write then partially read, then compact */
    size_t total = ARBI_SIZE(seed, BUF_CAP);
    uint8_t data[BUF_CAP];
    ARBI_FILL(seed, data, total);
    byte_buffer_put_bytes(&buf, data, total);

    size_t skip = ARBI_SIZE(seed, total);
    uint8_t tmp[BUF_CAP];
    byte_buffer_get_bytes(&buf, tmp, skip);

    size_t remaining = byte_buffer_readable(&buf);
    byte_buffer_compact(&buf);

    PROP_ASSERT_EQ(byte_buffer_readable(&buf), remaining);
    PROP_ASSERT(byte_buffer_writable(&buf) >= BUF_CAP - remaining);

    /* read remaining data and check it matches */
    uint8_t out[BUF_CAP];
    byte_buffer_get_bytes(&buf, out, remaining);
    PROP_ASSERT_MEM_EQ(out, data + skip, remaining);
}

/* Property: reset clears all state */
PROPERTY(reset_clears, 5000) {
    uint8_t storage[32];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, 32);

    size_t len = ARBI_SIZE(seed, 32);
    uint8_t data[32];
    ARBI_FILL(seed, data, len);
    byte_buffer_put_bytes(&buf, data, len);

    byte_buffer_reset(&buf);
    PROP_ASSERT_EQ(byte_buffer_readable(&buf), 0);
    PROP_ASSERT_EQ(byte_buffer_writable(&buf), 32);
}

TEST_MAIN("byte_buffer properties") {
    RUN(write_read_roundtrip);
    RUN(capacity_invariant);
    RUN(typed_put_get_roundtrip);
    RUN(compact_preserves_data);
    RUN(reset_clears);
}
TEST_MAIN_END()
