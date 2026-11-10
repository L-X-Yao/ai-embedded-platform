// SPDX-License-Identifier: MIT
// LibFuzzer harness for mavlink -- coverage-guided fuzzing
//
// Feeds the input to the parser as raw bytes, then serializes messages built
// from the input so frames get past the CRC and CRC_EXTRA check.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_mavlink.c ../mavlink.c -o fuzz_mavlink
// Run:     ./fuzz_mavlink -max_total_time=60 corpus_mavlink/

#include "../mavlink.h"
#include "fuzz_input.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t known_ids[] = {
    MAVLINK_MSG_ID_HEARTBEAT, MAVLINK_MSG_ID_SYS_STATUS,
    MAVLINK_MSG_ID_GPS_RAW_INT, MAVLINK_MSG_ID_ATTITUDE,
    MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
};
#define KNOWN_ID_COUNT (sizeof(known_ids) / sizeof(known_ids[0]))

static void feed_raw(const uint8_t *data, size_t size) {
    mavlink_parser_t parser;
    mavlink_parser_init(&parser);

    for (size_t i = 0; i < size; i++) {
        mavlink_parse_state_t state = mavlink_parse_byte(&parser, data[i]);
        const mavlink_msg_t *msg = mavlink_get_msg(&parser);
        if ((state == MAVLINK_PARSE_COMPLETE) != (msg != NULL)) __builtin_trap();
    }
}

static void read_message(fuzz_input_t *in, uint8_t op, mavlink_msg_t *msg) {
    memset(msg, 0, sizeof(*msg));
    msg->len = fuzz_u8(in);
    msg->incompat_flags = (op & 0x01) ? fuzz_u8(in) : 0;
    msg->compat_flags = fuzz_u8(in);
    msg->seq = fuzz_u8(in);
    msg->sysid = fuzz_u8(in);
    msg->compid = fuzz_u8(in);
    if (op & 0x02) {
        msg->msgid = ((uint32_t)fuzz_u8(in) << 16) | fuzz_u16(in);
    } else {
        msg->msgid = known_ids[fuzz_u8(in) % KNOWN_ID_COUNT];
    }
    fuzz_take(in, msg->payload, msg->len);
}

static void check_same(const mavlink_msg_t *a, const mavlink_msg_t *b) {
    if (a->len != b->len || a->compat_flags != b->compat_flags ||
        a->seq != b->seq || a->sysid != b->sysid || a->compid != b->compid ||
        a->msgid != b->msgid || memcmp(a->payload, b->payload, a->len) != 0) {
        __builtin_trap();
    }
}

// Each record is [op][message fields][payload]:
//   op bit 0: arbitrary incompat_flags, which the parser has to refuse
//   op bit 1: arbitrary message id, otherwise a known one
//   op bit 2: flip one payload bit, which the CRC has to catch
//   op bit 3: give the parser the message's CRC_EXTRA through a table
static void round_trip(fuzz_input_t *in) {
    mavlink_parser_t parser;
    mavlink_parser_init(&parser);

    while (in->left > 0) {
        uint8_t op = fuzz_u8(in);
        mavlink_msg_t msg;
        read_message(in, op, &msg);

        // Exact-size frame, so ASan sees any write past the end.
        uint16_t need = (uint16_t)(MAVLINK_HEADER_LEN + msg.len + MAVLINK_CHECKSUM_LEN);
        uint8_t *frame = malloc(need);
        if (!frame) return;

        uint16_t frame_len = 0;
        uint8_t extra = mavlink_msg_crc_extra(msg.msgid);
        bool known = extra != 0;
        mavlink_crc_extra_t entry = { msg.msgid, fuzz_u8(in) };
        if (op & 0x08) {
            extra = entry.crc_extra;
            known = true;
            mavlink_parser_set_crc_table(&parser, &entry, 1);
        } else {
            mavlink_parser_set_crc_table(&parser, NULL, 0);
        }
        if (mavlink_serialize(&msg, frame, (uint16_t)(need - 1), &frame_len, extra) !=
            MAVLINK_ERR_SIZE) {
            __builtin_trap();
        }
        if (mavlink_serialize(&msg, frame, need, &frame_len, extra) != MAVLINK_OK ||
            frame_len != need) {
            __builtin_trap();
        }

        bool flip = (op & 0x04) && msg.len > 0;
        if (flip) {
            frame[MAVLINK_HEADER_LEN + fuzz_u8(in) % msg.len] ^= (uint8_t)(1u << (op >> 5));
        }

        mavlink_parse_state_t state = MAVLINK_PARSE_IDLE;
        for (uint16_t i = 0; i < need; i++) {
            state = mavlink_parse_byte(&parser, frame[i]);
        }
        free(frame);

        if (msg.incompat_flags != 0) {
            // Refused mid frame, so the rest of the frame was scanned as noise.
            mavlink_parser_init(&parser);
        } else if (flip) {
            if (state == MAVLINK_PARSE_COMPLETE) __builtin_trap();
        } else if (!known) {
            // Nothing can check this frame, so nothing may call it valid.
            if (state != MAVLINK_PARSE_UNKNOWN_MSG || mavlink_get_msg(&parser)) {
                __builtin_trap();
            }
        } else {
            const mavlink_msg_t *got = mavlink_get_msg(&parser);
            if (state != MAVLINK_PARSE_COMPLETE || !got) __builtin_trap();
            check_same(&msg, got);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    feed_raw(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    round_trip(&in);

    return 0;
}
