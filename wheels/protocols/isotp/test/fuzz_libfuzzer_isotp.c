// SPDX-License-Identifier: MIT
// LibFuzzer harness for isotp -- coverage-guided fuzzing
//
// Feeds CAN frames read from the input into an exact-size reassembly buffer, then
// segments a payload into First and Consecutive Frames and checks it reassembles.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_isotp.c ../isotp.c -o fuzz_isotp
// Run:     ./fuzz_isotp -max_total_time=60 corpus_isotp/

#include "../isotp.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void read_message(const isotp_rx_t *rx, size_t cap) {
    size_t len = isotp_rx_length(rx);
    if (len > cap) __builtin_trap();

    // Touch every byte, so ASan sees a length past the buffer.
    const uint8_t *msg = isotp_rx_data(rx);
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum = (uint8_t)(sum + msg[i]);
    (void)sum;
}

// Each record is [dlc][frame bytes]. DLC 0 and 9 are refused.
static void feed_frames(fuzz_input_t *in) {
    size_t cap = fuzz_u16(in) % (ISOTP_MAX_PAYLOAD + 2);
    uint8_t *buf = malloc(cap ? cap : 1);
    if (!buf) return;

    isotp_rx_t rx;
    if (isotp_rx_init(&rx, buf, cap) != ISOTP_OK) __builtin_trap();

    while (in->left > 0) {
        uint8_t dlc = fuzz_u8(in) % (ISOTP_CAN_DLC + 2);

        // Exact-size frame, so ASan sees any read past the DLC.
        uint8_t *can = calloc(1, dlc ? dlc : 1);
        if (!can) break;
        fuzz_take(in, can, dlc);

        if (isotp_rx_feed(&rx, can, dlc) == ISOTP_OK) {
            // Flow control carries no data, so it cannot complete a message.
            if (isotp_frame_type(can[0]) == ISOTP_FC) __builtin_trap();
            read_message(&rx, cap);
        }
        free(can);

        if (in->left > 0 && (in->data[0] & 0xF0) == 0xF0) isotp_rx_reset(&rx);
    }
    free(buf);
}

static void reassemble(const uint8_t *payload, size_t len) {
    uint8_t *buf = malloc(len);
    if (!buf) return;

    isotp_rx_t rx;
    if (isotp_rx_init(&rx, buf, len) != ISOTP_OK) __builtin_trap();

    uint8_t can[ISOTP_CAN_DLC];
    can[0] = (uint8_t)(0x10 | (len >> 8));
    can[1] = (uint8_t)len;
    memcpy(&can[2], payload, 6);
    if (isotp_rx_feed(&rx, can, ISOTP_CAN_DLC) != ISOTP_INCOMPLETE) __builtin_trap();

    size_t off = 6;
    uint8_t sn = 1;
    while (off < len) {
        size_t chunk = len - off < 7 ? len - off : 7;
        memset(can, 0xCC, sizeof(can));
        can[0] = (uint8_t)(0x20 | sn);
        memcpy(&can[1], &payload[off], chunk);
        off += chunk;
        sn = (uint8_t)((sn + 1) & 0x0F);

        isotp_err_t want = off < len ? ISOTP_INCOMPLETE : ISOTP_OK;
        if (isotp_rx_feed(&rx, can, (uint8_t)(chunk + 1)) != want) __builtin_trap();
    }

    if (isotp_rx_length(&rx) != len || memcmp(isotp_rx_data(&rx), payload, len) != 0) {
        __builtin_trap();
    }
    free(buf);
}

static void round_trip(const uint8_t *data, size_t size) {
    static uint8_t payload[ISOTP_MAX_PAYLOAD];

    if (size >= 8) {
        size_t len = size > ISOTP_MAX_PAYLOAD ? ISOTP_MAX_PAYLOAD : size;
        memcpy(payload, data, len);
        reassemble(payload, len);
    }

    if (size >= 1 && size <= 7) {
        uint8_t can[ISOTP_CAN_DLC];
        uint8_t buf[7];
        isotp_rx_t rx;
        if (isotp_encode_sf(data, size, can) != ISOTP_OK) __builtin_trap();
        if (isotp_rx_init(&rx, buf, sizeof(buf)) != ISOTP_OK) __builtin_trap();
        if (isotp_rx_feed(&rx, can, ISOTP_CAN_DLC) != ISOTP_OK) __builtin_trap();
        if (isotp_rx_length(&rx) != size || memcmp(buf, data, size) != 0) __builtin_trap();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    feed_frames(&in);

    round_trip(data, size);

    if (size >= 3) {
        uint8_t can[ISOTP_CAN_DLC];
        isotp_encode_fc((isotp_fc_flag_t)(data[0] % 4), data[1], data[2], can);
    }

    return 0;
}
