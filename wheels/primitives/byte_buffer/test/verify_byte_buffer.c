/*
 * verify_byte_buffer.c — CBMC bounded model checking for byte_buffer
 *
 * Verifies cursor invariants, endian roundtrips, and compact correctness.
 * Run with:
 *   cbmc verify_byte_buffer.c ../byte_buffer.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../byte_buffer.h"
#include <stdint.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

#define BUF_CAP 32

extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern uint32_t nondet_uint32(void);
extern unsigned  nondet_unsigned(void);

static void check_invariants(const byte_buffer_t *buf) {
    assert(buf->read_pos <= buf->write_pos);
    assert(buf->write_pos <= buf->capacity);
    assert(byte_buffer_readable(buf) == buf->write_pos - buf->read_pos);
    assert(byte_buffer_writable(buf) == buf->capacity - buf->write_pos);
    assert(byte_buffer_written(buf) == buf->write_pos);
}

/* Proof 1: invariants hold after any operation sequence */
void verify_invariants(void) {
    uint8_t storage[BUF_CAP];
    byte_buffer_t buf;

    byte_buffer_err_t err = byte_buffer_init(&buf, storage, BUF_CAP);
    assert(err == BYTE_BUFFER_OK);
    check_invariants(&buf);

    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= UNWIND_BOUND);

    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = nondet_uint8() % 5;
        switch (op) {
        case 0: {
            uint8_t val = nondet_uint8();
            byte_buffer_put_u8(&buf, val);
            break;
        }
        case 1: {
            uint8_t val;
            byte_buffer_get_u8(&buf, &val);
            break;
        }
        case 2: {
            uint16_t val = nondet_uint16();
            byte_buffer_put_u16_be(&buf, val);
            break;
        }
        case 3: {
            uint16_t val;
            byte_buffer_get_u16_be(&buf, &val);
            break;
        }
        case 4: {
            byte_buffer_compact(&buf);
            break;
        }
        }
        check_invariants(&buf);
    }
}

/* Proof 2: put/get roundtrip preserves values with big-endian encoding */
void verify_endian_roundtrip(void) {
    uint8_t storage[BUF_CAP];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_CAP);

    uint8_t  v8  = nondet_uint8();
    uint16_t v16 = nondet_uint16();
    uint32_t v32 = nondet_uint32();

    /* Write all three */
    byte_buffer_err_t err;
    err = byte_buffer_put_u8(&buf, v8);
    assert(err == BYTE_BUFFER_OK);
    err = byte_buffer_put_u16_be(&buf, v16);
    assert(err == BYTE_BUFFER_OK);
    err = byte_buffer_put_u32_be(&buf, v32);
    assert(err == BYTE_BUFFER_OK);

    assert(byte_buffer_readable(&buf) == 1 + 2 + 4);

    /* Read back in same order */
    uint8_t  out8;
    uint16_t out16;
    uint32_t out32;
    err = byte_buffer_get_u8(&buf, &out8);
    assert(err == BYTE_BUFFER_OK);
    err = byte_buffer_get_u16_be(&buf, &out16);
    assert(err == BYTE_BUFFER_OK);
    err = byte_buffer_get_u32_be(&buf, &out32);
    assert(err == BYTE_BUFFER_OK);

    assert(out8  == v8);
    assert(out16 == v16);
    assert(out32 == v32);

    assert(byte_buffer_readable(&buf) == 0);
}

/* Proof 3: compact preserves unread data */
void verify_compact(void) {
    uint8_t storage[BUF_CAP];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_CAP);

    /* Write 4 bytes, read 2 (advance read_pos) */
    uint8_t w[4];
    for (int i = 0; i < 4; i++) {
        w[i] = nondet_uint8();
    }
    byte_buffer_put_bytes(&buf, w, 4);

    uint8_t discard[2];
    byte_buffer_get_bytes(&buf, discard, 2);

    size_t readable_before = byte_buffer_readable(&buf);
    assert(readable_before == 2);

    /* Compact moves unread data to front */
    byte_buffer_compact(&buf);

    assert(byte_buffer_readable(&buf) == readable_before);
    assert(buf.read_pos == 0);
    assert(byte_buffer_writable(&buf) == BUF_CAP - readable_before);

    /* Remaining data is correct */
    uint8_t out[2];
    byte_buffer_get_bytes(&buf, out, 2);
    assert(out[0] == w[2]);
    assert(out[1] == w[3]);
}

/* Proof 4: NULL parameter handling */
void verify_null_safety(void) {
    byte_buffer_err_t err;

    err = byte_buffer_init(NULL, NULL, 0);
    assert(err == BYTE_BUFFER_ERR_PARAM);

    uint8_t storage[8];
    byte_buffer_t buf;
    err = byte_buffer_init(&buf, storage, 8);
    assert(err == BYTE_BUFFER_OK);

    /* put_bytes with NULL src */
    err = byte_buffer_put_bytes(&buf, NULL, 4);
    assert(err == BYTE_BUFFER_ERR_PARAM);

    /* get_bytes with NULL dst */
    err = byte_buffer_get_bytes(&buf, NULL, 4);
    assert(err != BYTE_BUFFER_OK);

    /* readable/writable on valid buffer still work */
    assert(byte_buffer_readable(&buf) == 0);
    assert(byte_buffer_writable(&buf) == 8);
}

int main(void) {
    verify_invariants();
    verify_endian_roundtrip();
    verify_compact();
    verify_null_safety();
    return 0;
}
