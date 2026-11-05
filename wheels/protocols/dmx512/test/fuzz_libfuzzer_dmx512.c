// SPDX-License-Identifier: MIT
// LibFuzzer harness for dmx512 -- coverage-guided fuzzing
//
// Parses the input as a raw packet into an exact-size universe, then runs a
// sequence of channel operations read from the input. Values have to read back
// the way they were written.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_dmx512.c ../dmx512.c -o fuzz_dmx512
// Run:     ./fuzz_dmx512 -max_total_time=60 corpus_dmx512/

#include "../dmx512.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Past the channel limit, so out-of-range channels are part of every run.
#define CHANNEL_SPAN (DMX512_MAX_CHANNELS + 64)

static void parse_with_start_code(dmx512_t *dmx, const uint8_t *data, size_t size) {
    if (size == 0) return;

    uint8_t *packet = malloc(size);
    if (!packet) return;
    memcpy(packet, data, size);
    packet[0] = DMX512_START_CODE;

    if (dmx512_parse_packet(dmx, packet, size) != DMX512_OK) __builtin_trap();

    for (uint16_t ch = 1; ch <= dmx->num_channels; ch++) {
        uint8_t value = 0;
        if (dmx512_get_channel(dmx, ch, &value) != DMX512_OK) __builtin_trap();
        if (value != (ch < size ? packet[ch] : 0)) __builtin_trap();
    }
    free(packet);
}

static void set_one(dmx512_t *dmx, uint16_t channel, uint8_t value) {
    if (dmx512_set_channel(dmx, channel, value) != DMX512_OK) return;

    uint8_t got = 0;
    if (dmx512_get_channel(dmx, channel, &got) != DMX512_OK || got != value) __builtin_trap();
}

static void set_many(dmx512_t *dmx, uint16_t start, const uint8_t *values, uint16_t count) {
    if (dmx512_set_range(dmx, start, values, count) != DMX512_OK) return;

    for (uint16_t i = 0; i < count; i++) {
        uint8_t got = 0;
        if (dmx512_get_channel(dmx, (uint16_t)(start + i), &got) != DMX512_OK ||
            got != values[i]) {
            __builtin_trap();
        }
    }
}

static void run_ops(dmx512_t *dmx, fuzz_input_t *in) {
    while (in->left > 0) {
        uint8_t op = fuzz_u8(in);
        uint16_t channel = (uint16_t)(fuzz_u16(in) % CHANNEL_SPAN);

        switch (op & 3) {
        case 0:
            set_one(dmx, channel, fuzz_u8(in));
            break;
        case 1: {
            uint8_t value = 0;
            dmx512_get_channel(dmx, channel, &value);
            break;
        }
        case 2: {
            uint8_t values[64];
            uint16_t count = (uint16_t)fuzz_take(in, values, op >> 2);
            set_many(dmx, channel, values, count);
            break;
        }
        default: {
            const uint8_t *packet = NULL;
            size_t len = 0;
            if (dmx512_get_packet(dmx, &packet, &len) != DMX512_OK ||
                len != (size_t)dmx->num_channels + 1) {
                __builtin_trap();
            }
            break;
        }
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_input_t in;
    fuzz_input_init(&in, data, size);

    // Zero and 513 are refused, the rest get an exact-size universe buffer.
    uint16_t channels = (uint16_t)(fuzz_u16(&in) % (DMX512_MAX_CHANNELS + 2));
    uint8_t *universe = malloc((size_t)channels + 1);
    if (!universe) return 0;

    dmx512_t dmx;
    if (dmx512_init(&dmx, universe, channels) == DMX512_OK) {
        dmx512_parse_packet(&dmx, data, size);
        parse_with_start_code(&dmx, data, size);
        run_ops(&dmx, &in);
    }
    free(universe);

    return 0;
}
