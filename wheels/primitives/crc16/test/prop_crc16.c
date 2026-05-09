/*
 * prop_crc16.c — property-based tests for crc16
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_crc16.c ../crc16.c -o prop_crc16
 */

#include "test_harness.h"
#include "property_test.h"
#include "../crc16.h"
#include <string.h>

/* Property: same input always produces same CRC (deterministic) */
PROPERTY(deterministic, 10000) {
    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, data, len);

    uint16_t crc1 = crc16_modbus(data, len);
    uint16_t crc2 = crc16_modbus(data, len);
    PROP_ASSERT_EQ(crc1, crc2);
}

/* Property: step-by-step streaming matches one-shot */
PROPERTY(streaming_matches_oneshot, 10000) {
    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, data, len);

    uint16_t oneshot = crc16_modbus(data, len);

    uint16_t streaming = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < len; i++) {
        streaming = crc16_modbus_step(streaming, data[i]);
    }

    PROP_ASSERT_EQ(streaming, oneshot);
}

/* Property: flipping a bit changes the CRC */
PROPERTY(bit_flip_detected, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 63) + 1;
    ARBI_FILL(seed, data, len);

    uint16_t crc_orig = crc16_modbus(data, len);

    size_t byte_idx = ARBI_SIZE(seed, len - 1);
    uint8_t bit_mask = (uint8_t)(1u << (ARBI_UINT8(seed) % 8));
    data[byte_idx] ^= bit_mask;

    uint16_t crc_flipped = crc16_modbus(data, len);
    PROP_ASSERT(crc_orig != crc_flipped);
}

/* Property: empty data returns init value */
PROPERTY(empty_returns_init, 1000) {
    uint16_t crc = crc16_modbus(NULL, 0);
    PROP_ASSERT_EQ(crc, CRC16_MODBUS_INIT);
}

/* Property: known test vector - "123456789" CRC16-MODBUS = 0x4B37 */
PROPERTY(known_vector, 1) {
    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_modbus(data, 9);
    PROP_ASSERT_EQ(crc, 0x4B37);
}

TEST_MAIN("crc16 properties") {
    RUN(deterministic);
    RUN(streaming_matches_oneshot);
    RUN(bit_flip_detected);
    RUN(empty_returns_init);
    RUN(known_vector);
}
TEST_MAIN_END()
