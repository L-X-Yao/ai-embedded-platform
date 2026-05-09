/*
 * prop_canbus.c — property-based tests for canbus
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_canbus.c ../canbus.c -o prop_canbus
 */

#include "test_harness.h"
#include "property_test.h"
#include "../canbus.h"
#include <string.h>

/* Property: pack then unpack roundtrip preserves frame fields */
PROPERTY(pack_unpack_roundtrip, 10000) {
    canbus_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.extended = (bool)(ARBI_UINT8(seed) & 1);
    frame.rtr = false; /* RTR frames have no data, test data frames */
    frame.id = frame.extended
        ? (ARBI_UINT32(seed) & CANBUS_EXT_ID_MAX)
        : (ARBI_UINT32(seed) & CANBUS_STD_ID_MAX);
    frame.dlc = (uint8_t)ARBI_RANGE(seed, 0, CANBUS_DLC_MAX);
    ARBI_FILL(seed, frame.data, frame.dlc);

    uint8_t buf[CANBUS_PACK_MAX_SIZE];
    int packed = canbus_frame_pack(&frame, buf, sizeof(buf));
    PROP_ASSERT(packed > 0);

    canbus_frame_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    canbus_err_t err = canbus_frame_unpack(buf, (uint16_t)packed, &parsed);
    PROP_ASSERT_EQ(err, CANBUS_OK);
    PROP_ASSERT_EQ(parsed.id, frame.id);
    PROP_ASSERT_EQ(parsed.dlc, frame.dlc);
    PROP_ASSERT_EQ(parsed.extended, frame.extended);
    if (frame.dlc > 0) {
        PROP_ASSERT_MEM_EQ(parsed.data, frame.data, frame.dlc);
    }
}

/* Property: signal pack then extract roundtrip (Intel byte order) */
PROPERTY(signal_roundtrip_intel, 10000) {
    uint8_t data[8];
    memset(data, 0, sizeof(data));
    uint8_t length = (uint8_t)ARBI_RANGE(seed, 1, 32);
    uint8_t start_bit = (uint8_t)ARBI_RANGE(seed, 0, 63 - length + 1);
    uint64_t mask = (length >= 64) ? ~0ULL : ((1ULL << length) - 1);
    uint64_t value = ARBI_UINT32(seed) & mask;

    canbus_signal_pack(data, start_bit, length, false, value);
    uint64_t extracted = canbus_signal_extract(data, start_bit, length, false);
    PROP_ASSERT_EQ(extracted, value);
}

/* Property: pack is deterministic */
PROPERTY(pack_determinism, 5000) {
    canbus_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.extended = false;
    frame.rtr = false;
    frame.id = ARBI_UINT32(seed) & CANBUS_STD_ID_MAX;
    frame.dlc = (uint8_t)ARBI_RANGE(seed, 0, CANBUS_DLC_MAX);
    ARBI_FILL(seed, frame.data, frame.dlc);

    uint8_t buf1[CANBUS_PACK_MAX_SIZE], buf2[CANBUS_PACK_MAX_SIZE];
    int len1 = canbus_frame_pack(&frame, buf1, sizeof(buf1));
    int len2 = canbus_frame_pack(&frame, buf2, sizeof(buf2));
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: unpack of arbitrary bytes never crashes */
PROPERTY(unpack_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[CANBUS_PACK_MAX_SIZE];
    size_t len = ARBI_SIZE(seed, CANBUS_PACK_MAX_SIZE);
    ARBI_FILL(seed, garbage, len);

    canbus_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    canbus_frame_unpack(garbage, (uint16_t)len, &frame);
    canbus_frame_validate(&frame);

    canbus_filter_t filter = { .id = 0, .mask = 0, .extended = false };
    canbus_filter_match(&filter, &frame);
}

TEST_MAIN("canbus properties") {
    RUN(pack_unpack_roundtrip);
    RUN(signal_roundtrip_intel);
    RUN(pack_determinism);
    RUN(unpack_no_crash);
}
TEST_MAIN_END()
