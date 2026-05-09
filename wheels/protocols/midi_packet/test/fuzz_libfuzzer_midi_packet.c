// SPDX-License-Identifier: MIT
// LibFuzzer harness for midi_packet -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_midi_packet.c ../midi_packet.c -o fuzz_midi_packet
// Run:     ./fuzz_midi_packet -max_total_time=60 corpus_midi_packet/

#include "../midi_packet.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Exercise streaming parser byte-at-a-time
    midi_parser_t parser;
    midi_parser_init(&parser);

    for (size_t i = 0; i < size; i++) {
        midi_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        midi_parse(&parser, data[i], &msg);
    }

    // Exercise builder functions with data-derived params
    uint8_t buf[8];

    if (size >= 3) {
        midi_build_note_on(buf, sizeof(buf),
                           data[0] & 0x0F, data[1] & 0x7F, data[2] & 0x7F);
        midi_build_note_off(buf, sizeof(buf),
                            data[0] & 0x0F, data[1] & 0x7F, data[2] & 0x7F);
        midi_build_cc(buf, sizeof(buf),
                      data[0] & 0x0F, data[1] & 0x7F, data[2] & 0x7F);
    }

    if (size >= 2) {
        midi_build_program(buf, sizeof(buf),
                           data[0] & 0x0F, data[1] & 0x7F);
    }

    if (size >= 3) {
        int16_t bend_val = (int16_t)(((uint16_t)data[1] | ((uint16_t)data[2] << 8))
                                     - 8192);
        midi_build_pitch_bend(buf, sizeof(buf), data[0] & 0x0F, bend_val);
    }

    // Exercise builder with out-of-range values (should return errors)
    if (size >= 3) {
        midi_build_note_on(buf, sizeof(buf), data[0], data[1], data[2]);
        midi_build_cc(buf, sizeof(buf), data[0], data[1], data[2]);
        midi_build_pitch_bend(buf, sizeof(buf), data[0],
                              (int16_t)((uint16_t)data[1] | ((uint16_t)data[2] << 8)));
    }

    // Exercise builder with zero-size buffer
    midi_build_note_on(buf, 0, 0, 60, 100);
    midi_build_note_off(buf, 0, 0, 60, 100);
    midi_build_cc(buf, 0, 0, 64, 127);
    midi_build_program(buf, 0, 0, 0);
    midi_build_pitch_bend(buf, 0, 0, 0);

    return 0;
}
