// SPDX-License-Identifier: MIT
/*
 * prop_crc8.c — property-based tests for crc8
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_crc8.c ../crc8.c -o prop_crc8
 */

#include "test_harness.h"
#include "property_test.h"
#include "../crc8.h"
#include <string.h>

/* Property: same input always produces same CRC (deterministic) */
PROPERTY(deterministic, 10000) {
    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, data, len);

    uint8_t crc1 = crc8_compute(&CRC8_DEFAULT, data, len);
    uint8_t crc2 = crc8_compute(&CRC8_DEFAULT, data, len);
    PROP_ASSERT_EQ(crc1, crc2);
}

/* Property: deterministic across all predefined configs */
PROPERTY(deterministic_all_configs, 5000) {
    const crc8_config_t *configs[] = {
        &CRC8_DEFAULT, &CRC8_MAXIM, &CRC8_SENSIRION, &CRC8_SMBUS, &CRC8_ROHC
    };

    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 64);
    ARBI_FILL(seed, data, len);

    for (int c = 0; c < 5; c++) {
        uint8_t crc1 = crc8_compute(configs[c], data, len);
        uint8_t crc2 = crc8_compute(configs[c], data, len);
        PROP_ASSERT_EQ(crc1, crc2);
    }
}

/* Property: verify returns true for matching CRC */
PROPERTY(verify_matches_compute, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 64);
    ARBI_FILL(seed, data, len);

    uint8_t crc = crc8_compute(&CRC8_DEFAULT, data, len);
    PROP_ASSERT(crc8_verify(&CRC8_DEFAULT, data, len, crc));
}

/* Property: flipping a bit changes the CRC (error detection) */
PROPERTY(bit_flip_detected, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 63) + 1; /* at least 1 byte */
    ARBI_FILL(seed, data, len);

    uint8_t crc_orig = crc8_compute(&CRC8_DEFAULT, data, len);

    /* flip one random bit */
    size_t byte_idx = ARBI_SIZE(seed, len - 1);
    uint8_t bit_mask = (uint8_t)(1u << (ARBI_UINT8(seed) % 8));
    data[byte_idx] ^= bit_mask;

    uint8_t crc_flipped = crc8_compute(&CRC8_DEFAULT, data, len);
    PROP_ASSERT(crc_orig != crc_flipped);
}

/* Property: known test vectors - "123456789" */
PROPERTY(known_vector_default, 1) {
    const uint8_t data[] = "123456789";
    /* CRC-8 of "123456789" with poly=0x07, init=0x00 is 0xF4 */
    uint8_t crc = crc8_compute(&CRC8_DEFAULT, data, 9);
    PROP_ASSERT_EQ(crc, 0xF4);
}

TEST_MAIN("crc8 properties") {
    RUN(deterministic);
    RUN(deterministic_all_configs);
    RUN(verify_matches_compute);
    RUN(bit_flip_detected);
    RUN(known_vector_default);
}
TEST_MAIN_END()
