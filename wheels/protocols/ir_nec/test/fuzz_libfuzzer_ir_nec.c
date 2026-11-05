// SPDX-License-Identifier: MIT
// LibFuzzer harness for ir_nec -- coverage-guided fuzzing
//
// Decodes pulse arrays read from the input, then encoded frames with timing
// jitter inside the 20% tolerance, which have to decode to the same message.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_ir_nec.c ../ir_nec.c -o fuzz_ir_nec
// Run:     ./fuzz_ir_nec -max_total_time=60 corpus_ir_nec/

#include "../ir_nec.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void decode_raw(const uint8_t *data, size_t size) {
    // Exact-size array, so ASan sees any read past count.
    uint16_t count = (uint16_t)(size / sizeof(ir_nec_pulse_t));
    ir_nec_pulse_t *pulses = malloc((count ? count : 1) * sizeof(*pulses));
    if (!pulses) return;
    if (count > 0) memcpy(pulses, data, (size_t)count * sizeof(*pulses));

    ir_nec_msg_t msg;
    uint16_t address = 0;
    uint8_t command = 0;
    ir_nec_decode(pulses, count, &msg);
    ir_nec_decode_extended(pulses, count, &address, &command);
    (void)ir_nec_is_repeat(pulses, count);
    free(pulses);
}

// Spread each timing up to 15% either way, inside the decoder's tolerance.
static void jitter(ir_nec_pulse_t *pulses, uint16_t count, fuzz_input_t *in) {
    for (uint16_t i = 0; i < count; i++) {
        uint32_t mark_pct = 85u + fuzz_u8(in) % 31u;
        uint32_t space_pct = 85u + fuzz_u8(in) % 31u;
        pulses[i].mark_us = pulses[i].mark_us * mark_pct / 100u;
        pulses[i].space_us = pulses[i].space_us * space_pct / 100u;
    }
}

static void round_trip(fuzz_input_t *in) {
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ir_nec_msg_t msg = { fuzz_u8(in), fuzz_u8(in) };
    if (ir_nec_encode(&msg, pulses, &count) != IR_NEC_OK || count != IR_NEC_PULSE_COUNT) {
        __builtin_trap();
    }
    jitter(pulses, count, in);

    ir_nec_msg_t back;
    if (ir_nec_decode(pulses, count, &back) != IR_NEC_OK || back.address != msg.address ||
        back.command != msg.command) {
        __builtin_trap();
    }
    if (ir_nec_decode(pulses, (uint16_t)(count - 1), &back) != IR_NEC_ERR_SHORT) __builtin_trap();

    uint16_t address = fuzz_u16(in);
    uint8_t command = fuzz_u8(in);
    if (ir_nec_encode_extended(address, command, pulses, &count) != IR_NEC_OK) __builtin_trap();
    jitter(pulses, count, in);

    uint16_t got_address = 0;
    uint8_t got_command = 0;
    if (ir_nec_decode_extended(pulses, count, &got_address, &got_command) != IR_NEC_OK ||
        got_address != address || got_command != command) {
        __builtin_trap();
    }

    if (ir_nec_encode_repeat(pulses, &count) != IR_NEC_OK) __builtin_trap();
    jitter(pulses, count, in);
    if (!ir_nec_is_repeat(pulses, count)) __builtin_trap();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    decode_raw(data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    round_trip(&in);

    return 0;
}
