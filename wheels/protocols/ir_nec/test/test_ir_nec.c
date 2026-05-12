// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ir_nec.h"
#include <string.h>

/* ========== Tests ========== */

/* ---------- encode/decode round-trip ---------- */

TEST(test_encode_decode_roundtrip)
{
    ir_nec_msg_t msg = { .address = 0x00, .command = 0x0C };
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode(&msg, pulses, &count), IR_NEC_OK);
    ASSERT_EQ(count, IR_NEC_PULSE_COUNT);

    ir_nec_msg_t decoded;
    ASSERT_EQ(ir_nec_decode(pulses, count, &decoded), IR_NEC_OK);
    ASSERT_EQ(decoded.address, 0x00);
    ASSERT_EQ(decoded.command, 0x0C);
}

/* ---------- known NEC frame: addr=0x00, cmd=0x0C ---------- */

TEST(test_known_frame)
{
    /*
     * Standard NEC: addr=0x00, cmd=0x0C
     * 32-bit data LSB first: 0x00, 0xFF, 0x0C, 0xF3
     * Raw 32-bit word (MSB first): 0xF30CFF00
     * We verify by encoding and checking the data bytes.
     */
    ir_nec_msg_t msg = { .address = 0x00, .command = 0x0C };
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode(&msg, pulses, &count), IR_NEC_OK);

    /* Leader pulse */
    ASSERT_EQ(pulses[0].mark_us, IR_NEC_LEADER_MARK_US);
    ASSERT_EQ(pulses[0].space_us, IR_NEC_LEADER_SPACE_US);

    /* Verify address byte = 0x00: all 8 bits should be zero-space */
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(pulses[1 + i].mark_us, IR_NEC_BIT_MARK_US);
        ASSERT_EQ(pulses[1 + i].space_us, IR_NEC_BIT_ZERO_SPACE_US);
    }

    /* Address complement = 0xFF: all 8 bits should be one-space */
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(pulses[9 + i].mark_us, IR_NEC_BIT_MARK_US);
        ASSERT_EQ(pulses[9 + i].space_us, IR_NEC_BIT_ONE_SPACE_US);
    }

    /* Command = 0x0C = 0b00001100 (LSB first: 0,0,1,1,0,0,0,0) */
    uint8_t expected_cmd_bits[] = {0, 0, 1, 1, 0, 0, 0, 0};
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(pulses[17 + i].mark_us, IR_NEC_BIT_MARK_US);
        if (expected_cmd_bits[i]) {
            ASSERT_EQ(pulses[17 + i].space_us, IR_NEC_BIT_ONE_SPACE_US);
        } else {
            ASSERT_EQ(pulses[17 + i].space_us, IR_NEC_BIT_ZERO_SPACE_US);
        }
    }

    /* Stop bit */
    ASSERT_EQ(pulses[33].mark_us, IR_NEC_BIT_MARK_US);
    ASSERT_EQ(pulses[33].space_us, 0u);
}

/* ---------- repeat frame ---------- */

TEST(test_repeat_frame)
{
    ir_nec_pulse_t pulses[IR_NEC_REPEAT_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode_repeat(pulses, &count), IR_NEC_OK);
    ASSERT_EQ(count, 2u);

    ASSERT_EQ(pulses[0].mark_us, IR_NEC_LEADER_MARK_US);
    ASSERT_EQ(pulses[0].space_us, IR_NEC_REPEAT_SPACE_US);
    ASSERT_EQ(pulses[1].mark_us, IR_NEC_BIT_MARK_US);
    ASSERT_EQ(pulses[1].space_us, 0u);
}

/* ---------- is_repeat ---------- */

TEST(test_is_repeat)
{
    ir_nec_pulse_t repeat[3];
    uint16_t count = 0;
    ASSERT_EQ(ir_nec_encode_repeat(repeat, &count), IR_NEC_OK);
    ASSERT_TRUE(ir_nec_is_repeat(repeat, count));

    /* Normal frame should NOT be detected as repeat */
    ir_nec_msg_t msg = { .address = 0x10, .command = 0x20 };
    ir_nec_pulse_t normal[IR_NEC_PULSE_COUNT];
    uint16_t ncount = 0;
    ASSERT_EQ(ir_nec_encode(&msg, normal, &ncount), IR_NEC_OK);
    ASSERT_FALSE(ir_nec_is_repeat(normal, ncount));
}

/* ---------- extended NEC ---------- */

TEST(test_extended_roundtrip)
{
    uint16_t addr = 0x1234;
    uint8_t cmd = 0x56;

    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode_extended(addr, cmd, pulses, &count), IR_NEC_OK);
    ASSERT_EQ(count, IR_NEC_PULSE_COUNT);

    uint16_t dec_addr = 0;
    uint8_t dec_cmd = 0;
    ASSERT_EQ(ir_nec_decode_extended(pulses, count, &dec_addr, &dec_cmd), IR_NEC_OK);
    ASSERT_EQ(dec_addr, 0x1234);
    ASSERT_EQ(dec_cmd, 0x56);
}

/* ---------- complement mismatch rejection ---------- */

TEST(test_complement_mismatch)
{
    ir_nec_msg_t msg = { .address = 0xAA, .command = 0x55 };
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode(&msg, pulses, &count), IR_NEC_OK);

    /* Corrupt the address complement: flip bit 0 of byte 2 (pulse index 9) */
    /* Change a zero-space to one-space or vice versa */
    if (pulses[9].space_us == IR_NEC_BIT_ZERO_SPACE_US) {
        pulses[9].space_us = IR_NEC_BIT_ONE_SPACE_US;
    } else {
        pulses[9].space_us = IR_NEC_BIT_ZERO_SPACE_US;
    }

    ir_nec_msg_t decoded;
    ASSERT_EQ(ir_nec_decode(pulses, count, &decoded), IR_NEC_ERR_FORMAT);
}

/* ---------- timing tolerance boundaries ---------- */

TEST(test_timing_tolerance)
{
    ir_nec_msg_t msg = { .address = 0x42, .command = 0x69 };
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode(&msg, pulses, &count), IR_NEC_OK);

    /* Apply +19% to all marks and spaces -- should still decode */
    ir_nec_pulse_t stretched[IR_NEC_PULSE_COUNT];
    memcpy(stretched, pulses, sizeof(pulses));
    for (uint16_t i = 0; i < count; i++) {
        stretched[i].mark_us  = stretched[i].mark_us * 119u / 100u;
        stretched[i].space_us = stretched[i].space_us * 119u / 100u;
    }

    ir_nec_msg_t decoded;
    ASSERT_EQ(ir_nec_decode(stretched, count, &decoded), IR_NEC_OK);
    ASSERT_EQ(decoded.address, 0x42);
    ASSERT_EQ(decoded.command, 0x69);

    /* Apply -19% -- should still decode */
    ir_nec_pulse_t shrunk[IR_NEC_PULSE_COUNT];
    memcpy(shrunk, pulses, sizeof(pulses));
    for (uint16_t i = 0; i < count; i++) {
        shrunk[i].mark_us  = shrunk[i].mark_us * 81u / 100u;
        shrunk[i].space_us = shrunk[i].space_us * 81u / 100u;
    }
    ASSERT_EQ(ir_nec_decode(shrunk, count, &decoded), IR_NEC_OK);
    ASSERT_EQ(decoded.address, 0x42);
    ASSERT_EQ(decoded.command, 0x69);
}

/* ---------- timing out of tolerance ---------- */

TEST(test_timing_out_of_range)
{
    ir_nec_msg_t msg = { .address = 0x10, .command = 0x20 };
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode(&msg, pulses, &count), IR_NEC_OK);

    /* Corrupt leader mark to be way too short */
    ir_nec_pulse_t bad[IR_NEC_PULSE_COUNT];
    memcpy(bad, pulses, sizeof(pulses));
    bad[0].mark_us = 1000;  /* 9000 * 0.8 = 7200, 1000 is way below */

    ir_nec_msg_t decoded;
    ASSERT_EQ(ir_nec_decode(bad, count, &decoded), IR_NEC_ERR_FORMAT);
}

/* ---------- LSB bit order verification ---------- */

TEST(test_lsb_order)
{
    /* Address = 0x01 = binary 00000001. LSB first: bit0=1, rest=0.
     * So pulse[1] (first data bit) should have ONE space. */
    ir_nec_msg_t msg = { .address = 0x01, .command = 0x80 };
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode(&msg, pulses, &count), IR_NEC_OK);

    /* First address bit (LSB) = 1 */
    ASSERT_EQ(pulses[1].space_us, IR_NEC_BIT_ONE_SPACE_US);
    /* Second address bit = 0 */
    ASSERT_EQ(pulses[2].space_us, IR_NEC_BIT_ZERO_SPACE_US);

    /* Command = 0x80 = binary 10000000. LSB first: bit0=0 ... bit7=1. */
    /* First command bit (pulse[17]) = 0 */
    ASSERT_EQ(pulses[17].space_us, IR_NEC_BIT_ZERO_SPACE_US);
    /* Last command bit (pulse[24]) = 1 */
    ASSERT_EQ(pulses[24].space_us, IR_NEC_BIT_ONE_SPACE_US);
}

/* ---------- NULL rejection ---------- */

TEST(test_null_rejection)
{
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;
    ir_nec_msg_t msg = { .address = 0, .command = 0 };

    /* encode NULL */
    ASSERT_EQ(ir_nec_encode(NULL, pulses, &count), IR_NEC_ERR_PARAM);
    ASSERT_EQ(ir_nec_encode(&msg, NULL, &count), IR_NEC_ERR_PARAM);
    ASSERT_EQ(ir_nec_encode(&msg, pulses, NULL), IR_NEC_ERR_PARAM);

    /* decode NULL */
    ASSERT_EQ(ir_nec_decode(NULL, 34, &msg), IR_NEC_ERR_PARAM);
    ASSERT_EQ(ir_nec_decode(pulses, 34, NULL), IR_NEC_ERR_PARAM);

    /* repeat NULL */
    ASSERT_EQ(ir_nec_encode_repeat(NULL, &count), IR_NEC_ERR_PARAM);
    ASSERT_EQ(ir_nec_encode_repeat(pulses, NULL), IR_NEC_ERR_PARAM);

    /* is_repeat NULL */
    ASSERT_FALSE(ir_nec_is_repeat(NULL, 3));

    /* extended NULL */
    uint16_t addr = 0;
    uint8_t cmd = 0;
    ASSERT_EQ(ir_nec_encode_extended(0, 0, NULL, &count), IR_NEC_ERR_PARAM);
    ASSERT_EQ(ir_nec_encode_extended(0, 0, pulses, NULL), IR_NEC_ERR_PARAM);
    ASSERT_EQ(ir_nec_decode_extended(NULL, 34, &addr, &cmd), IR_NEC_ERR_PARAM);
    ASSERT_EQ(ir_nec_decode_extended(pulses, 34, NULL, &cmd), IR_NEC_ERR_PARAM);
    ASSERT_EQ(ir_nec_decode_extended(pulses, 34, &addr, NULL), IR_NEC_ERR_PARAM);
}

/* ---------- too few pulses ---------- */

TEST(test_too_few_pulses)
{
    ir_nec_pulse_t pulses[10];
    memset(pulses, 0, sizeof(pulses));

    ir_nec_msg_t msg;
    ASSERT_EQ(ir_nec_decode(pulses, 10, &msg), IR_NEC_ERR_SHORT);
    ASSERT_EQ(ir_nec_decode(pulses, 0, &msg), IR_NEC_ERR_SHORT);
    ASSERT_EQ(ir_nec_decode(pulses, 33, &msg), IR_NEC_ERR_SHORT);

    uint16_t addr;
    uint8_t cmd;
    ASSERT_EQ(ir_nec_decode_extended(pulses, 10, &addr, &cmd), IR_NEC_ERR_SHORT);
}

/* ---------- all addresses and commands round-trip ---------- */

TEST(test_all_values_roundtrip)
{
    /* Test a sampling of address/command combinations */
    uint8_t test_addrs[] = {0x00, 0x01, 0x55, 0xAA, 0xFE, 0xFF};
    uint8_t test_cmds[]  = {0x00, 0x01, 0x55, 0xAA, 0xFE, 0xFF};

    for (int a = 0; a < (int)(sizeof(test_addrs)); a++) {
        for (int c = 0; c < (int)(sizeof(test_cmds)); c++) {
            ir_nec_msg_t msg = { .address = test_addrs[a], .command = test_cmds[c] };
            ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
            uint16_t count = 0;

            ASSERT_EQ(ir_nec_encode(&msg, pulses, &count), IR_NEC_OK);

            ir_nec_msg_t decoded;
            ASSERT_EQ(ir_nec_decode(pulses, count, &decoded), IR_NEC_OK);
            ASSERT_EQ(decoded.address, test_addrs[a]);
            ASSERT_EQ(decoded.command, test_cmds[c]);
        }
    }
}

/* ---------- extended NEC various addresses ---------- */

TEST(test_extended_various)
{
    uint16_t addrs[] = {0x0000, 0x00FF, 0xFF00, 0xFFFF, 0x1234, 0xABCD};
    uint8_t cmds[]   = {0x00, 0x0C, 0x55, 0xFF};

    for (int a = 0; a < (int)(sizeof(addrs) / sizeof(addrs[0])); a++) {
        for (int c = 0; c < (int)(sizeof(cmds) / sizeof(cmds[0])); c++) {
            ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
            uint16_t count = 0;

            ASSERT_EQ(ir_nec_encode_extended(addrs[a], cmds[c], pulses, &count), IR_NEC_OK);

            uint16_t dec_addr = 0;
            uint8_t dec_cmd = 0;
            ASSERT_EQ(ir_nec_decode_extended(pulses, count, &dec_addr, &dec_cmd), IR_NEC_OK);
            ASSERT_EQ(dec_addr, addrs[a]);
            ASSERT_EQ(dec_cmd, cmds[c]);
        }
    }
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)IR_NEC_OK, 0);
    ASSERT_EQ((int)IR_NEC_ERR_PARAM, -1);
    ASSERT_EQ((int)IR_NEC_ERR_FORMAT, -2);
    ASSERT_EQ((int)IR_NEC_ERR_SHORT, -3);
}

/* ---------- command complement corruption ---------- */

TEST(test_command_complement_corruption)
{
    ir_nec_msg_t msg = { .address = 0x20, .command = 0x40 };
    ir_nec_pulse_t pulses[IR_NEC_PULSE_COUNT];
    uint16_t count = 0;

    ASSERT_EQ(ir_nec_encode(&msg, pulses, &count), IR_NEC_OK);

    /* Corrupt command complement (pulse index 25, first bit of cmd_inv) */
    if (pulses[25].space_us == IR_NEC_BIT_ZERO_SPACE_US) {
        pulses[25].space_us = IR_NEC_BIT_ONE_SPACE_US;
    } else {
        pulses[25].space_us = IR_NEC_BIT_ZERO_SPACE_US;
    }

    ir_nec_msg_t decoded;
    ASSERT_EQ(ir_nec_decode(pulses, count, &decoded), IR_NEC_ERR_FORMAT);
}

/* ---------- is_repeat edge cases ---------- */

TEST(test_is_repeat_edge_cases)
{
    /* Too few pulses */
    ir_nec_pulse_t single[1] = {{ .mark_us = 9000, .space_us = 2250 }};
    ASSERT_FALSE(ir_nec_is_repeat(single, 1));

    /* Count = 0 */
    ASSERT_FALSE(ir_nec_is_repeat(single, 0));

    /* Wrong leader timing */
    ir_nec_pulse_t bad[2] = {
        { .mark_us = 1000, .space_us = 2250 },
        { .mark_us = 562, .space_us = 0 }
    };
    ASSERT_FALSE(ir_nec_is_repeat(bad, 2));
}

/* ---------- main ---------- */

TEST_MAIN("ir_nec")
    RUN(test_encode_decode_roundtrip);
    RUN(test_known_frame);
    RUN(test_repeat_frame);
    RUN(test_is_repeat);
    RUN(test_extended_roundtrip);
    RUN(test_complement_mismatch);
    RUN(test_timing_tolerance);
    RUN(test_timing_out_of_range);
    RUN(test_lsb_order);
    RUN(test_null_rejection);
    RUN(test_too_few_pulses);
    RUN(test_all_values_roundtrip);
    RUN(test_extended_various);
    RUN(test_error_codes);
    RUN(test_command_complement_corruption);
    RUN(test_is_repeat_edge_cases);
TEST_MAIN_END()
