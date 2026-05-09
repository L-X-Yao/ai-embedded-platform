// SPDX-License-Identifier: MIT
// LibFuzzer harness for canbus -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_canbus.c ../canbus.c -o fuzz_canbus
// Run:     ./fuzz_canbus -max_total_time=60 corpus_canbus/

#include "../canbus.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Exercise frame unpack with fuzz data
    canbus_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    canbus_frame_unpack(data, (uint16_t)(size > 0xFFFF ? 0xFFFF : size), &frame);

    // If unpack succeeded, validate and re-pack
    if (canbus_frame_validate(&frame) == CANBUS_OK) {
        uint8_t packed[CANBUS_PACK_MAX_SIZE];
        canbus_frame_pack(&frame, packed, sizeof(packed));
    }

    // Exercise filter matching with data-derived filter params
    if (size >= 9) {
        canbus_filter_t filter;
        memset(&filter, 0, sizeof(filter));
        filter.id = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                    ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
        filter.mask = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                      ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
        filter.extended = data[8] & 1;

        canbus_frame_t test_frame;
        memset(&test_frame, 0, sizeof(test_frame));
        canbus_frame_unpack(data, (uint16_t)(size > 0xFFFF ? 0xFFFF : size),
                            &test_frame);
        canbus_filter_match(&filter, &test_frame);
    }

    // Exercise signal extract/pack with data-derived bit params
    if (size >= 11) {
        uint8_t can_data[8];
        memset(can_data, 0, sizeof(can_data));
        size_t copy_len = size < 8 ? size : 8;
        memcpy(can_data, data, copy_len);

        uint8_t start_bit = data[size >= 9 ? 8 : 0] & 0x3F;  // 0-63
        uint8_t length = (data[size >= 10 ? 9 : 0] & 0x3F) + 1; // 1-64
        bool big_endian = data[size >= 11 ? 10 : 0] & 1;

        canbus_signal_extract(can_data, start_bit, length, big_endian);
        canbus_signal_pack(can_data, start_bit, length, big_endian, 0x12345678);
    }

    // Exercise ID classification
    if (size >= 4) {
        uint32_t id = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                      ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
        canbus_is_standard_id(id);
        canbus_is_extended_id(id);
    }

    return 0;
}
