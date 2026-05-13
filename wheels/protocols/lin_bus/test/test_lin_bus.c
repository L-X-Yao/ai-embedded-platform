// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "lin_bus.h"
#include <string.h>

/* ---------- PID calculation tests ---------- */

TEST(test_pid_id_0x00)
{
    /* ID=0x00: all bits 0
     * P0 = 0^0^0^0 = 0, P1 = ~(0^0^0^0) = 1
     * PID = 0x00 | (0 << 6) | (1 << 7) = 0x80 */
    ASSERT_EQ_HEX(lin_bus_calc_pid(0x00), 0x80);
}

TEST(test_pid_id_0x3F)
{
    /* ID=0x3F = 0b111111
     * id0-id5 = all 1
     * P0 = 1^1^1^1 = 0, P1 = ~(1^1^1^1) = 1
     * PID = 0x3F | (0 << 6) | (1 << 7) = 0xBF */
    ASSERT_EQ_HEX(lin_bus_calc_pid(0x3F), 0xBF);
}

TEST(test_pid_id_out_of_range)
{
    ASSERT_EQ_HEX(lin_bus_calc_pid(64), 0x00);
    ASSERT_EQ_HEX(lin_bus_calc_pid(0xFF), 0x00);
}

TEST(test_pid_verify_valid)
{
    /* Verify all valid IDs produce valid PIDs */
    for (int id = 0; id <= 63; id++) {
        uint8_t pid = lin_bus_calc_pid((uint8_t)id);
        ASSERT_TRUE(lin_bus_verify_pid(pid));
    }
}

TEST(test_pid_verify_invalid)
{
    /* Flip a parity bit -- should fail */
    uint8_t pid = lin_bus_calc_pid(0x00);
    uint8_t bad_pid = (uint8_t)(pid ^ 0x40u); /* flip P0 */
    ASSERT_FALSE(lin_bus_verify_pid(bad_pid));

    bad_pid = (uint8_t)(pid ^ 0x80u); /* flip P1 */
    ASSERT_FALSE(lin_bus_verify_pid(bad_pid));
}

TEST(test_pid_specific_ids)
{
    /* ID=0x01: id0=1,id1=0,id2=0,id3=0,id4=0,id5=0
     * P0 = 1^0^0^0 = 1, P1 = ~(0^0^0^0) = 1
     * PID = 0x01 | 0x40 | 0x80 = 0xC1 */
    ASSERT_EQ_HEX(lin_bus_calc_pid(0x01), 0xC1);

    /* ID=0x30 = 0b110000: id0=0,id1=0,id2=0,id3=0,id4=1,id5=1
     * P0 = 0^0^0^1 = 1, P1 = ~(0^0^1^1) = 1
     * PID = 0x30 | 0x40 | 0x80 = 0xF0 */
    ASSERT_EQ_HEX(lin_bus_calc_pid(0x30), 0xF0);
}

/* ---------- checksum tests ---------- */

TEST(test_checksum_classic_known)
{
    /* Classic checksum: sum data bytes with carry wraparound, invert */
    lin_bus_frame_t frame = {
        .id = 0x00,
        .data = {0x4A, 0x55, 0x93, 0xE5},
        .data_len = 4,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t cs = lin_bus_calc_checksum(&frame);
    /* Manual: 0x4A + 0x55 = 0x9F, + 0x93 = 0x132 -> 0x33,
     *         0x33 + 0xE5 = 0x118 -> 0x19, ~0x19 = 0xE6 */
    ASSERT_EQ_HEX(cs, 0xE6);
}

TEST(test_checksum_enhanced_known)
{
    /* Enhanced checksum: PID + data bytes */
    lin_bus_frame_t frame = {
        .id = 0x00,
        .data = {0x4A, 0x55, 0x93, 0xE5},
        .data_len = 4,
        .checksum_type = LIN_BUS_CHECKSUM_ENHANCED,
    };
    uint8_t pid = lin_bus_calc_pid(0x00); /* 0x80 */
    uint8_t cs = lin_bus_calc_checksum(&frame);
    /* Manual: 0x80 + 0x4A = 0xCA, + 0x55 = 0x11F -> 0x20,
     *         0x20 + 0x93 = 0xB3, + 0xE5 = 0x198 -> 0x99, ~0x99 = 0x66 */
    (void)pid;
    ASSERT_EQ_HEX(cs, 0x66);
}

TEST(test_checksum_null)
{
    ASSERT_EQ(lin_bus_calc_checksum(NULL), 0);
}

TEST(test_checksum_invalid_len)
{
    lin_bus_frame_t frame = {
        .id = 0x00,
        .data = {0x01},
        .data_len = 0,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    ASSERT_EQ(lin_bus_calc_checksum(&frame), 0);

    frame.data_len = 9;
    ASSERT_EQ(lin_bus_calc_checksum(&frame), 0);
}

/* ---------- encode/decode NULL params ---------- */

TEST(test_encode_null_params)
{
    lin_bus_frame_t frame = {
        .id = 0x00,
        .data = {0x01},
        .data_len = 1,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len;

    ASSERT_EQ(lin_bus_encode(NULL, buf, sizeof(buf), &len), LIN_BUS_ERR_PARAM);
    ASSERT_EQ(lin_bus_encode(&frame, NULL, sizeof(buf), &len), LIN_BUS_ERR_PARAM);
    ASSERT_EQ(lin_bus_encode(&frame, buf, sizeof(buf), NULL), LIN_BUS_ERR_PARAM);
}

TEST(test_decode_null_params)
{
    uint8_t buf[] = {0x55, 0x80, 0x01, 0x00};
    lin_bus_frame_t frame;

    ASSERT_EQ(lin_bus_decode(NULL, sizeof(buf), &frame), LIN_BUS_ERR_PARAM);
    ASSERT_EQ(lin_bus_decode(buf, sizeof(buf), NULL), LIN_BUS_ERR_PARAM);
}

/* ---------- encode param validation ---------- */

TEST(test_encode_id_out_of_range)
{
    lin_bus_frame_t frame = {
        .id = 64,
        .data = {0x01},
        .data_len = 1,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len;
    ASSERT_EQ(lin_bus_encode(&frame, buf, sizeof(buf), &len), LIN_BUS_ERR_PARAM);
}

TEST(test_encode_data_len_zero)
{
    lin_bus_frame_t frame = {
        .id = 0x00,
        .data = {0x01},
        .data_len = 0,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len;
    ASSERT_EQ(lin_bus_encode(&frame, buf, sizeof(buf), &len), LIN_BUS_ERR_PARAM);
}

TEST(test_encode_data_len_over_8)
{
    lin_bus_frame_t frame = {
        .id = 0x00,
        .data = {0},
        .data_len = 9,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len;
    ASSERT_EQ(lin_bus_encode(&frame, buf, sizeof(buf), &len), LIN_BUS_ERR_PARAM);
}

TEST(test_encode_buffer_too_small)
{
    lin_bus_frame_t frame = {
        .id = 0x00,
        .data = {0x01, 0x02, 0x03, 0x04},
        .data_len = 4,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[3]; /* Need at least 7 bytes */
    size_t len;
    ASSERT_EQ(lin_bus_encode(&frame, buf, sizeof(buf), &len), LIN_BUS_ERR_PARAM);
}

/* ---------- roundtrip tests ---------- */

TEST(test_roundtrip_classic)
{
    lin_bus_frame_t tx = {
        .id = 0x10,
        .data = {0xAA, 0xBB, 0xCC},
        .data_len = 3,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(lin_bus_encode(&tx, buf, sizeof(buf), &len), LIN_BUS_OK);
    ASSERT_EQ(len, 6); /* sync + PID + 3 data + checksum */

    lin_bus_frame_t rx;
    ASSERT_EQ(lin_bus_decode(buf, len, &rx), LIN_BUS_OK);
    ASSERT_EQ(rx.id, 0x10);
    ASSERT_EQ(rx.data_len, 3);
    ASSERT_MEMEQ_N(rx.data, tx.data, 3);
    ASSERT_EQ((int)rx.checksum_type, (int)LIN_BUS_CHECKSUM_CLASSIC);
}

TEST(test_roundtrip_enhanced)
{
    lin_bus_frame_t tx = {
        .id = 0x20,
        .data = {0x11, 0x22, 0x33, 0x44, 0x55},
        .data_len = 5,
        .checksum_type = LIN_BUS_CHECKSUM_ENHANCED,
    };
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(lin_bus_encode(&tx, buf, sizeof(buf), &len), LIN_BUS_OK);
    ASSERT_EQ(len, 8); /* sync + PID + 5 data + checksum */

    lin_bus_frame_t rx;
    ASSERT_EQ(lin_bus_decode(buf, len, &rx), LIN_BUS_OK);
    ASSERT_EQ(rx.id, 0x20);
    ASSERT_EQ(rx.data_len, 5);
    ASSERT_MEMEQ_N(rx.data, tx.data, 5);
    ASSERT_EQ((int)rx.checksum_type, (int)LIN_BUS_CHECKSUM_ENHANCED);
}

/* ---------- decode error tests ---------- */

TEST(test_decode_wrong_sync)
{
    uint8_t buf[] = {0xAA, 0x80, 0x01, 0x00};
    lin_bus_frame_t frame;
    ASSERT_EQ(lin_bus_decode(buf, sizeof(buf), &frame), LIN_BUS_ERR_SYNC);
}

TEST(test_decode_bad_parity)
{
    lin_bus_frame_t tx = {
        .id = 0x05,
        .data = {0x01},
        .data_len = 1,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len = 0;
    lin_bus_encode(&tx, buf, sizeof(buf), &len);

    /* Corrupt PID parity bits */
    buf[1] ^= 0xC0u;

    lin_bus_frame_t rx;
    ASSERT_EQ(lin_bus_decode(buf, len, &rx), LIN_BUS_ERR_PARITY);
}

TEST(test_decode_bad_checksum)
{
    lin_bus_frame_t tx = {
        .id = 0x05,
        .data = {0x01, 0x02},
        .data_len = 2,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len = 0;
    lin_bus_encode(&tx, buf, sizeof(buf), &len);

    /* Corrupt checksum (last byte) */
    buf[len - 1] ^= 0xFF;

    lin_bus_frame_t rx;
    ASSERT_EQ(lin_bus_decode(buf, len, &rx), LIN_BUS_ERR_CHECKSUM);
}

TEST(test_decode_too_short)
{
    uint8_t buf[] = {0x55, 0x80, 0x01};
    lin_bus_frame_t frame;
    ASSERT_EQ(lin_bus_decode(buf, sizeof(buf), &frame), LIN_BUS_ERR_FORMAT);
}

TEST(test_decode_too_long)
{
    uint8_t buf[12];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x55;
    lin_bus_frame_t frame;
    ASSERT_EQ(lin_bus_decode(buf, sizeof(buf), &frame), LIN_BUS_ERR_FORMAT);
}

/* ---------- all data lengths 1-8 ---------- */

TEST(test_roundtrip_all_lengths)
{
    for (uint8_t dlen = 1; dlen <= 8; dlen++) {
        lin_bus_frame_t tx;
        memset(&tx, 0, sizeof(tx));
        tx.id = 0x0A;
        tx.data_len = dlen;
        tx.checksum_type = LIN_BUS_CHECKSUM_CLASSIC;
        for (uint8_t i = 0; i < dlen; i++) {
            tx.data[i] = (uint8_t)(i + 0x10);
        }

        uint8_t buf[16];
        size_t len = 0;
        ASSERT_EQ(lin_bus_encode(&tx, buf, sizeof(buf), &len), LIN_BUS_OK);
        ASSERT_EQ(len, (size_t)dlen + 3);

        lin_bus_frame_t rx;
        ASSERT_EQ(lin_bus_decode(buf, len, &rx), LIN_BUS_OK);
        ASSERT_EQ(rx.id, 0x0A);
        ASSERT_EQ(rx.data_len, dlen);
        ASSERT_MEMEQ_N(rx.data, tx.data, dlen);
    }
}

/* ---------- diagnostic frame IDs ---------- */

TEST(test_roundtrip_diagnostic_ids)
{
    uint8_t diag_ids[] = {60, 61};
    for (int idx = 0; idx < 2; idx++) {
        lin_bus_frame_t tx = {
            .id = diag_ids[idx],
            .data = {0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
            .data_len = 8,
            .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
        };

        uint8_t buf[16];
        size_t len = 0;
        ASSERT_EQ(lin_bus_encode(&tx, buf, sizeof(buf), &len), LIN_BUS_OK);

        lin_bus_frame_t rx;
        ASSERT_EQ(lin_bus_decode(buf, len, &rx), LIN_BUS_OK);
        ASSERT_EQ(rx.id, diag_ids[idx]);
        ASSERT_EQ(rx.data_len, 8);
    }
}

/* ---------- edge case data values ---------- */

TEST(test_roundtrip_all_zeros)
{
    lin_bus_frame_t tx = {
        .id = 0x00,
        .data = {0x00, 0x00, 0x00, 0x00},
        .data_len = 4,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(lin_bus_encode(&tx, buf, sizeof(buf), &len), LIN_BUS_OK);

    lin_bus_frame_t rx;
    ASSERT_EQ(lin_bus_decode(buf, len, &rx), LIN_BUS_OK);
    ASSERT_EQ(rx.id, 0x00);
    uint8_t zeros[4] = {0};
    ASSERT_MEMEQ_N(rx.data, zeros, 4);
}

TEST(test_roundtrip_all_0xff)
{
    lin_bus_frame_t tx = {
        .id = 0x3F,
        .data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        .data_len = 8,
        .checksum_type = LIN_BUS_CHECKSUM_ENHANCED,
    };
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(lin_bus_encode(&tx, buf, sizeof(buf), &len), LIN_BUS_OK);

    lin_bus_frame_t rx;
    ASSERT_EQ(lin_bus_decode(buf, len, &rx), LIN_BUS_OK);
    ASSERT_EQ(rx.id, 0x3F);
    ASSERT_EQ(rx.data_len, 8);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ_HEX(rx.data[i], 0xFF);
    }
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)LIN_BUS_OK);
    ASSERT_EQ(-1, (int)LIN_BUS_ERR_PARAM);
    ASSERT_EQ(-2, (int)LIN_BUS_ERR_SYNC);
    ASSERT_EQ(-3, (int)LIN_BUS_ERR_PARITY);
    ASSERT_EQ(-4, (int)LIN_BUS_ERR_CHECKSUM);
    ASSERT_EQ(-5, (int)LIN_BUS_ERR_FORMAT);
}

TEST(test_encode_verify_sync_and_pid)
{
    lin_bus_frame_t tx = {
        .id = 0x15,
        .data = {0xAA},
        .data_len = 1,
        .checksum_type = LIN_BUS_CHECKSUM_CLASSIC,
    };
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(lin_bus_encode(&tx, buf, sizeof(buf), &len), LIN_BUS_OK);

    /* Verify sync byte */
    ASSERT_EQ_HEX(buf[0], LIN_BUS_SYNC_BYTE);

    /* Verify PID matches calc_pid */
    ASSERT_EQ_HEX(buf[1], lin_bus_calc_pid(0x15));
}

TEST(test_diag_frame_forces_classic_checksum)
{
    /* Per LIN 2.0+ spec: diagnostic frames (ID 60, 61) MUST use classic
     * checksum even if the user specifies enhanced. */
    uint8_t diag_ids[] = {60, 61};
    for (int idx = 0; idx < 2; idx++) {
        /* Encode with enhanced -- should silently use classic */
        lin_bus_frame_t tx = {
            .id = diag_ids[idx],
            .data = {0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07},
            .data_len = 8,
            .checksum_type = LIN_BUS_CHECKSUM_ENHANCED,
        };
        uint8_t buf_enh[16];
        size_t len_enh = 0;
        ASSERT_EQ(lin_bus_encode(&tx, buf_enh, sizeof(buf_enh), &len_enh), LIN_BUS_OK);

        /* Encode with classic */
        tx.checksum_type = LIN_BUS_CHECKSUM_CLASSIC;
        uint8_t buf_cls[16];
        size_t len_cls = 0;
        ASSERT_EQ(lin_bus_encode(&tx, buf_cls, sizeof(buf_cls), &len_cls), LIN_BUS_OK);

        /* Checksum bytes must be identical (both forced to classic) */
        ASSERT_EQ(len_enh, len_cls);
        ASSERT_EQ_HEX(buf_enh[len_enh - 1], buf_cls[len_cls - 1]);

        /* Decode the enhanced-encoded frame -- must report classic */
        lin_bus_frame_t rx;
        ASSERT_EQ(lin_bus_decode(buf_enh, len_enh, &rx), LIN_BUS_OK);
        ASSERT_EQ((int)rx.checksum_type, (int)LIN_BUS_CHECKSUM_CLASSIC);
    }
}

TEST(test_diag_frame_decode_rejects_enhanced)
{
    /* Construct a diagnostic frame with a manually calculated enhanced
     * checksum. Decoder should reject it because diagnostics require classic. */
    uint8_t pid60 = lin_bus_calc_pid(60);

    /* Build a frame manually with enhanced checksum */
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};

    /* Compute classic checksum */
    uint16_t sum_cls = 0;
    for (int i = 0; i < 4; i++) {
        sum_cls += data[i];
        if (sum_cls > 0xFF) sum_cls -= 0xFF;
    }
    uint8_t cs_classic = (uint8_t)(~sum_cls & 0xFF);

    /* Compute enhanced checksum (includes PID) */
    uint16_t sum_enh = pid60;
    for (int i = 0; i < 4; i++) {
        sum_enh += data[i];
        if (sum_enh > 0xFF) sum_enh -= 0xFF;
    }
    uint8_t cs_enhanced = (uint8_t)(~sum_enh & 0xFF);

    /* If they happen to be equal, this test is not useful -- skip */
    if (cs_classic == cs_enhanced) return;

    /* Build raw buffer with enhanced checksum */
    uint8_t buf[] = {0x55, pid60, 0xAA, 0xBB, 0xCC, 0xDD, cs_enhanced};
    lin_bus_frame_t rx;
    lin_bus_err_t err = lin_bus_decode(buf, sizeof(buf), &rx);
    /* Should fail because decoder only accepts classic for diagnostic frames */
    ASSERT_EQ(err, LIN_BUS_ERR_CHECKSUM);
}

TEST_MAIN("lin_bus")
    RUN(test_pid_id_0x00);
    RUN(test_pid_id_0x3F);
    RUN(test_pid_id_out_of_range);
    RUN(test_pid_verify_valid);
    RUN(test_pid_verify_invalid);
    RUN(test_pid_specific_ids);
    RUN(test_checksum_classic_known);
    RUN(test_checksum_enhanced_known);
    RUN(test_checksum_null);
    RUN(test_checksum_invalid_len);
    RUN(test_encode_null_params);
    RUN(test_decode_null_params);
    RUN(test_encode_id_out_of_range);
    RUN(test_encode_data_len_zero);
    RUN(test_encode_data_len_over_8);
    RUN(test_encode_buffer_too_small);
    RUN(test_roundtrip_classic);
    RUN(test_roundtrip_enhanced);
    RUN(test_decode_wrong_sync);
    RUN(test_decode_bad_parity);
    RUN(test_decode_bad_checksum);
    RUN(test_decode_too_short);
    RUN(test_decode_too_long);
    RUN(test_roundtrip_all_lengths);
    RUN(test_roundtrip_diagnostic_ids);
    RUN(test_roundtrip_all_zeros);
    RUN(test_roundtrip_all_0xff);
    RUN(test_error_codes);
    RUN(test_encode_verify_sync_and_pid);
    RUN(test_diag_frame_forces_classic_checksum);
    RUN(test_diag_frame_decode_rejects_enhanced);
TEST_MAIN_END()
