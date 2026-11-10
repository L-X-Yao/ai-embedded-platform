// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "xmodem.h"
#include <string.h>

/* ---------- mock infrastructure ---------- */

#define MAX_WRITTEN_BLOCKS  16
#define MAX_SENT_BYTES      64

static struct {
    uint32_t offsets[MAX_WRITTEN_BLOCKS];
    uint8_t  data[MAX_WRITTEN_BLOCKS][128];
    uint16_t lens[MAX_WRITTEN_BLOCKS];
    int      count;
    xmodem_err_t next_err;   /* inject write errors */
} g_written;

static struct {
    uint8_t  bytes[MAX_SENT_BYTES];
    int      count;
} g_sent;

static void mock_reset(void)
{
    memset(&g_written, 0, sizeof(g_written));
    memset(&g_sent, 0, sizeof(g_sent));
}

static xmodem_err_t mock_write_block(void *ctx, uint32_t offset,
                                      const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (g_written.next_err != XMODEM_OK) return g_written.next_err;
    if (g_written.count >= MAX_WRITTEN_BLOCKS) return XMODEM_ERR_WRITE;
    g_written.offsets[g_written.count] = offset;
    g_written.lens[g_written.count] = len;
    memcpy(g_written.data[g_written.count], data, len);
    g_written.count++;
    return XMODEM_OK;
}

static xmodem_err_t mock_send_byte(void *ctx, uint8_t byte)
{
    (void)ctx;
    if (g_sent.count < MAX_SENT_BYTES) {
        g_sent.bytes[g_sent.count] = byte;
    }
    g_sent.count++;
    return XMODEM_OK;
}

static const xmodem_io_t g_io = {
    .write_block = mock_write_block,
    .send_byte   = mock_send_byte,
    .ctx         = NULL,
};

/* ---------- helpers ---------- */

/* Build a valid checksum-mode frame (132 bytes): SOH + blk + ~blk + 128 data + cksum */
static void build_checksum_frame(uint8_t *frame, uint8_t blk, uint8_t fill)
{
    frame[0] = XMODEM_SOH;
    frame[1] = blk;
    frame[2] = (uint8_t)(~blk);
    memset(&frame[3], fill, 128);
    uint8_t sum = 0;
    for (int i = 0; i < 128; i++) {
        sum = (uint8_t)(sum + frame[3 + i]);
    }
    frame[131] = sum;
}

/* Build a valid CRC-mode frame (133 bytes): SOH + blk + ~blk + 128 data + crc_hi + crc_lo */
static void build_crc_frame_of(uint8_t *frame, uint8_t blk, const uint8_t *data)
{
    frame[0] = XMODEM_SOH;
    frame[1] = blk;
    frame[2] = (uint8_t)(~blk);
    memcpy(&frame[3], data, 128);
    /* compute CRC-16/XMODEM */
    uint16_t crc = 0;
    for (int i = 0; i < 128; i++) {
        crc = (uint16_t)(crc ^ ((uint16_t)frame[3 + i] << 8));
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    frame[131] = (uint8_t)(crc >> 8);
    frame[132] = (uint8_t)(crc & 0xFF);
}

static void build_crc_frame(uint8_t *frame, uint8_t blk, uint8_t fill)
{
    uint8_t data[128];
    memset(data, fill, sizeof(data));
    build_crc_frame_of(frame, blk, data);
}

static void feed_bytes(xmodem_t *xm, const uint8_t *data, int len)
{
    for (int i = 0; i < len; i++) {
        xmodem_feed(xm, data[i]);
    }
}

/* ---------- tests ---------- */

TEST(test_init_null_params)
{
    xmodem_t xm;
    ASSERT_EQ(xmodem_init(NULL, &g_io, false), XMODEM_ERR_PARAM);
    ASSERT_EQ(xmodem_init(&xm, NULL, false), XMODEM_ERR_PARAM);
}

TEST(test_init_ok)
{
    xmodem_t xm;
    mock_reset();
    ASSERT_EQ(xmodem_init(&xm, &g_io, false), XMODEM_OK);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_IDLE);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 0);
}

TEST(test_start_sends_nak_checksum_mode)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    ASSERT_EQ(xmodem_start(&xm), XMODEM_OK);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_WAIT_HEADER);
    ASSERT_EQ(g_sent.count, 1);
    ASSERT_EQ(g_sent.bytes[0], XMODEM_NAK);
}

TEST(test_start_sends_c_crc_mode)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, true);
    ASSERT_EQ(xmodem_start(&xm), XMODEM_OK);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_WAIT_HEADER);
    ASSERT_EQ(g_sent.count, 1);
    ASSERT_EQ(g_sent.bytes[0], XMODEM_C);
}

TEST(test_start_double_call_rejected)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    ASSERT_EQ(xmodem_start(&xm), XMODEM_OK);
    ASSERT_EQ(xmodem_start(&xm), XMODEM_ERR_PARAM);
}

TEST(test_single_block_checksum)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0xAA);
    feed_bytes(&xm, frame, 132);

    /* should have written one block */
    ASSERT_EQ(g_written.count, 1);
    ASSERT_EQ(g_written.offsets[0], 0);
    ASSERT_EQ(g_written.lens[0], 128);
    ASSERT_EQ(g_written.data[0][0], 0xAA);

    /* ACK should have been sent (NAK from start + ACK after block) */
    ASSERT_EQ(g_sent.count, 2);
    ASSERT_EQ(g_sent.bytes[1], XMODEM_ACK);

    ASSERT_EQ(xmodem_get_total_bytes(&xm), 128);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_RECEIVING);
}

TEST(test_single_block_crc)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, true);
    xmodem_start(&xm);

    uint8_t frame[133];
    build_crc_frame(frame, 1, 0x55);
    feed_bytes(&xm, frame, 133);

    ASSERT_EQ(g_written.count, 1);
    ASSERT_EQ(g_written.offsets[0], 0);
    ASSERT_EQ(g_written.data[0][0], 0x55);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 128);
}

/* XMODEM has no length field, so 0x1A padding cannot be told from data. */
TEST(test_total_bytes_counts_padding)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x1A);
    feed_bytes(&xm, frame, 132);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_EOT), XMODEM_DONE);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 128);
}

TEST(test_eot_completes_transfer)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0xBB);
    feed_bytes(&xm, frame, 132);

    /* feed EOT */
    xmodem_err_t err = xmodem_feed(&xm, XMODEM_EOT);
    ASSERT_EQ(err, XMODEM_DONE);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_COMPLETE);

    /* last sent byte should be ACK for EOT */
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_ACK);
}

TEST(test_bad_checksum_sends_nak)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0xCC);
    frame[131] = (uint8_t)(frame[131] + 1);  /* corrupt checksum */
    feed_bytes(&xm, frame, 132);

    /* block should NOT have been written */
    ASSERT_EQ(g_written.count, 0);
    /* NAK sent (start NAK + error NAK) */
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 0);
}

TEST(test_bad_crc_sends_nak)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, true);
    xmodem_start(&xm);

    uint8_t frame[133];
    build_crc_frame(frame, 1, 0xDD);
    frame[132] = (uint8_t)(frame[132] ^ 0xFF);  /* corrupt CRC low byte */
    feed_bytes(&xm, frame, 133);

    ASSERT_EQ(g_written.count, 0);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);
}

TEST(test_bad_sequence_error)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    /* send block 2 when block 1 is expected (not a duplicate, not expected) */
    uint8_t frame[132];
    build_checksum_frame(frame, 2, 0xEE);

    xmodem_err_t last = XMODEM_OK;
    for (int i = 0; i < 132; i++) {
        xmodem_err_t err = xmodem_feed(&xm, frame[i]);
        if (err != XMODEM_OK) last = err;
    }

    ASSERT_EQ(last, XMODEM_ERR_SEQUENCE);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_ERROR);
}

TEST(test_can_cancels)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    /* Per spec, 2 consecutive CAN bytes required to cancel */
    xmodem_err_t err = xmodem_feed(&xm, XMODEM_CAN);
    ASSERT_EQ(err, XMODEM_OK);  /* first CAN: not yet cancelled */
    err = xmodem_feed(&xm, XMODEM_CAN);
    ASSERT_EQ(err, XMODEM_ERR_CANCEL);  /* second CAN: cancelled */
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_ERROR);
}

TEST(test_cancel_sends_two_can)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    xmodem_cancel(&xm);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_ERROR);
    /* start NAK(1) + CAN(2) = 3 bytes */
    ASSERT_EQ(g_sent.count, 3);
    ASSERT_EQ(g_sent.bytes[1], XMODEM_CAN);
    ASSERT_EQ(g_sent.bytes[2], XMODEM_CAN);
}

TEST(test_multiple_blocks)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];

    /* block 1 */
    build_checksum_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 132);

    /* block 2 */
    build_checksum_frame(frame, 2, 0x22);
    feed_bytes(&xm, frame, 132);

    /* block 3 */
    build_checksum_frame(frame, 3, 0x33);
    feed_bytes(&xm, frame, 132);

    ASSERT_EQ(g_written.count, 3);
    ASSERT_EQ(g_written.offsets[0], 0);
    ASSERT_EQ(g_written.offsets[1], 128);
    ASSERT_EQ(g_written.offsets[2], 256);
    ASSERT_EQ(g_written.data[0][0], 0x11);
    ASSERT_EQ(g_written.data[1][0], 0x22);
    ASSERT_EQ(g_written.data[2][0], 0x33);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 384);
}

TEST(test_state_transitions)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_IDLE);

    xmodem_start(&xm);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_WAIT_HEADER);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x00);
    feed_bytes(&xm, frame, 132);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_RECEIVING);

    xmodem_feed(&xm, XMODEM_EOT);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_COMPLETE);
}

TEST(test_feed_in_wrong_state)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);

    /* feed before start — should reject */
    ASSERT_EQ(xmodem_feed(&xm, 0x01), XMODEM_ERR_PARAM);

    /* feed after complete — should reject */
    xmodem_start(&xm);
    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x00);
    feed_bytes(&xm, frame, 132);
    xmodem_feed(&xm, XMODEM_EOT);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_COMPLETE);
    ASSERT_EQ(xmodem_feed(&xm, 0x01), XMODEM_ERR_PARAM);
}

TEST(test_write_callback_error)
{
    xmodem_t xm;
    mock_reset();
    g_written.next_err = XMODEM_ERR_WRITE;
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0xFF);

    xmodem_err_t last = XMODEM_OK;
    for (int i = 0; i < 132; i++) {
        xmodem_err_t err = xmodem_feed(&xm, frame[i]);
        if (err != XMODEM_OK) last = err;
    }

    ASSERT_EQ(last, XMODEM_ERR_WRITE);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_ERROR);
}

TEST(test_get_state_null)
{
    ASSERT_EQ(xmodem_get_state(NULL), XMODEM_STATE_IDLE);
    ASSERT_EQ(xmodem_get_total_bytes(NULL), 0);
}

TEST(test_bad_complement_sends_nak)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0xAA);
    frame[2] = 0x00;  /* corrupt complement (should be ~1 = 0xFE) */
    feed_bytes(&xm, frame, 132);

    ASSERT_EQ(g_written.count, 0);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);
}

TEST(test_err_crc_constant_and_recovery)
{
    /* XMODEM_ERR_CRC is defined for caller-level CRC error reporting.
     * The receiver treats CRC mismatch as recoverable (NAK + retry).
     * Verify: (1) constant value, (2) CRC-mode bad frame triggers NAK + allows retry. */
    ASSERT_EQ(XMODEM_ERR_CRC, -3);

    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, true);  /* CRC mode */
    xmodem_start(&xm);

    /* Send frame with corrupted CRC */
    uint8_t frame[133];
    build_crc_frame(frame, 1, 0x77);
    frame[131] ^= 0xFF;  /* corrupt CRC high byte */
    feed_bytes(&xm, frame, 133);

    ASSERT_EQ(g_written.count, 0);  /* block rejected */
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);
    /* Receiver stays in RECEIVING state — can accept a retransmitted block */
    ASSERT_TRUE(xmodem_get_state(&xm) == XMODEM_STATE_RECEIVING ||
                xmodem_get_state(&xm) == XMODEM_STATE_WAIT_HEADER);

    /* Retransmit correct block — should succeed */
    build_crc_frame(frame, 1, 0x77);
    feed_bytes(&xm, frame, 133);
    ASSERT_EQ(g_written.count, 1);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_ACK);
}

/* Block 1 whole, block 2 with byte 50 lost, then the sender's timer resends
 * block 2. Resend byte 0 completes the short frame, so the other 131 bytes
 * land on a frame boundary, and data[20] of the block is 0x04. */
TEST(test_lost_byte_data_eot_is_not_done)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 132);

    build_checksum_frame(frame, 2, 0x22);
    feed_bytes(&xm, frame, 50);
    feed_bytes(&xm, frame + 51, 81);

    frame[3 + 20] = XMODEM_EOT;
    frame[131] = (uint8_t)(frame[131] + XMODEM_EOT - 0x22);
    for (int i = 0; i < 132; i++) {
        ASSERT_TRUE(xmodem_feed(&xm, frame[i]) != XMODEM_DONE);
    }
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_RECEIVING);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 128);

    /* The next clean resend still lands, and only then does EOT count. */
    feed_bytes(&xm, frame, 132);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 256);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_EOT), XMODEM_DONE);
}

/* SOH of block 2 lost: the whole block is read at a frame boundary, with no
 * bad frame to give it away, and data[20] of the block is 0x04. */
TEST(test_lost_soh_data_eot_is_not_done)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 132);

    build_checksum_frame(frame, 2, 0x22);
    frame[3 + 20] = XMODEM_EOT;
    frame[131] = (uint8_t)(frame[131] + XMODEM_EOT - 0x22);
    for (int i = 1; i < 132; i++) {
        ASSERT_TRUE(xmodem_feed(&xm, frame[i]) != XMODEM_DONE);
    }
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_RECEIVING);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 128);
}

/* SOH of block 4 lost: the block number itself is 0x04. */
TEST(test_lost_soh_of_block_4_is_not_done)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    for (uint8_t blk = 1; blk <= 3; blk++) {
        build_checksum_frame(frame, blk, blk);
        feed_bytes(&xm, frame, 132);
    }

    build_checksum_frame(frame, 4, 0x44);
    for (int i = 1; i < 132; i++) {
        ASSERT_TRUE(xmodem_feed(&xm, frame[i]) != XMODEM_DONE);
    }
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_RECEIVING);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 384);
}

/* A real EOT is resent until ACKed, so the block 4 rule costs one NAK. */
TEST(test_eot_after_three_blocks_needs_the_repeat)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    uint8_t frame[132];
    for (uint8_t blk = 1; blk <= 3; blk++) {
        build_checksum_frame(frame, blk, blk);
        feed_bytes(&xm, frame, 132);
    }

    ASSERT_EQ(xmodem_feed(&xm, XMODEM_EOT), XMODEM_OK);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_EOT), XMODEM_DONE);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_ACK);
}

TEST(test_line_idle_needs_enable)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_start(&xm);

    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_ERR_PARAM);
    ASSERT_EQ(xmodem_enable_line_idle(&xm), XMODEM_ERR_PARAM);  /* too late */
    ASSERT_EQ(xmodem_line_idle(NULL), XMODEM_ERR_PARAM);
    ASSERT_EQ(xmodem_enable_line_idle(NULL), XMODEM_ERR_PARAM);
}

TEST(test_line_idle_outside_a_transfer_refused)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    ASSERT_EQ(xmodem_enable_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_ERR_PARAM);  /* not started */

    xmodem_start(&xm);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(g_sent.count, 1);                          /* nothing to ask for */

    xmodem_cancel(&xm);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_ERR_PARAM);
}

/* ymodem.txt 7.4: wait for the line to clear, then NAK. */
TEST(test_line_idle_holds_the_nak_until_quiet)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_enable_line_idle(&xm);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x11);
    frame[131] = (uint8_t)(frame[131] + 1);
    feed_bytes(&xm, frame, 132);
    ASSERT_EQ(g_sent.count, 1);                          /* only the handshake */

    /* Everything is dropped now, even a good block and two CANs. */
    build_checksum_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 132);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_CAN), XMODEM_OK);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_CAN), XMODEM_OK);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_EOT), XMODEM_OK);
    ASSERT_EQ(g_written.count, 0);
    ASSERT_EQ(g_sent.count, 1);

    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(g_sent.count, 2);
    ASSERT_EQ(g_sent.bytes[1], XMODEM_NAK);

    /* A second quiet period asks for nothing more. */
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(g_sent.count, 2);

    feed_bytes(&xm, frame, 132);
    ASSERT_EQ(g_written.count, 1);
    ASSERT_EQ(g_sent.bytes[2], XMODEM_ACK);
}

/* The run from the finding, with the caller reporting the quiet line. */
TEST(test_line_idle_lost_byte_recovers)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_enable_line_idle(&xm);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 132);

    build_checksum_frame(frame, 2, 0x22);
    frame[3 + 20] = XMODEM_EOT;
    frame[3 + 21] = XMODEM_SOH;
    frame[131] = (uint8_t)(frame[131] + XMODEM_EOT + XMODEM_SOH - 2 * 0x22);
    feed_bytes(&xm, frame, 50);
    feed_bytes(&xm, frame + 51, 81);

    /* The sender's timer beat ours: the resend runs into the short frame. */
    for (int i = 0; i < 132; i++) {
        ASSERT_EQ(xmodem_feed(&xm, frame[i]), XMODEM_OK);
    }
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 128);

    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);

    feed_bytes(&xm, frame, 132);
    ASSERT_EQ(xmodem_get_total_bytes(&xm), 256);
    ASSERT_EQ(g_written.data[1][20], XMODEM_EOT);
}

TEST(test_line_idle_drops_a_part_frame)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_enable_line_idle(&xm);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 100);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);

    feed_bytes(&xm, frame, 132);
    ASSERT_EQ(g_written.count, 1);
}

/* NAK to a sender that has not started would select checksum mode. */
TEST(test_line_idle_crc_asks_with_c_until_a_block_lands)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, true);
    xmodem_enable_line_idle(&xm);
    xmodem_start(&xm);

    ASSERT_EQ(xmodem_feed(&xm, 'x'), XMODEM_OK);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(g_sent.count, 2);
    ASSERT_EQ(g_sent.bytes[1], XMODEM_C);

    uint8_t frame[133];
    build_crc_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 133);
    ASSERT_EQ(g_written.count, 1);

    ASSERT_EQ(xmodem_feed(&xm, 'x'), XMODEM_OK);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);
}

TEST(test_line_idle_proves_eot)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_enable_line_idle(&xm);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 132);
    int sent = g_sent.count;

    ASSERT_EQ(xmodem_feed(&xm, XMODEM_EOT), XMODEM_OK);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_EOT), XMODEM_OK);  /* sender's repeat */
    ASSERT_EQ(g_sent.count, sent);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_RECEIVING);

    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_DONE);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_ACK);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_COMPLETE);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_ERR_PARAM);
}

/* 0x04 with more bytes behind it was never an EOT. */
TEST(test_line_idle_eot_with_data_behind_it_is_not_done)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_enable_line_idle(&xm);
    xmodem_start(&xm);

    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x11);
    feed_bytes(&xm, frame, 132);

    ASSERT_EQ(xmodem_feed(&xm, XMODEM_EOT), XMODEM_OK);
    ASSERT_EQ(xmodem_feed(&xm, 0xFB), XMODEM_OK);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_RECEIVING);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], XMODEM_NAK);
}

/* Two CANs a quiet period apart are two line hits, not a cancel. */
TEST(test_line_idle_splits_cans)
{
    xmodem_t xm;
    mock_reset();
    xmodem_init(&xm, &g_io, false);
    xmodem_enable_line_idle(&xm);
    xmodem_start(&xm);

    ASSERT_EQ(xmodem_feed(&xm, XMODEM_CAN), XMODEM_OK);
    ASSERT_EQ(xmodem_line_idle(&xm), XMODEM_OK);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_CAN), XMODEM_OK);
    ASSERT_EQ(xmodem_feed(&xm, XMODEM_CAN), XMODEM_ERR_CANCEL);
}

/* ---------- one lost byte, every position ---------- */

#define SIM_BLOCKS 8
#define SIM_TRIES  10

enum { SIM_EXACT, SIM_FAILED, SIM_CUT_SHORT };

static uint8_t  sim_file[SIM_BLOCKS * 128];
static uint8_t  sim_out[SIM_BLOCKS * 128];
static uint8_t  sim_reply;
static uint32_t sim_seed;

static xmodem_err_t sim_write(void *ctx, uint32_t offset,
                              const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (offset + len > sizeof(sim_out)) return XMODEM_ERR_WRITE;
    memcpy(sim_out + offset, data, len);
    return XMODEM_OK;
}

static xmodem_err_t sim_send(void *ctx, uint8_t byte)
{
    (void)ctx;
    if (sim_reply == 0) sim_reply = byte;  /* oldest unread byte */
    return XMODEM_OK;
}

static const xmodem_io_t g_sim_io = {
    .write_block = sim_write,
    .send_byte   = sim_send,
    .ctx         = NULL,
};

/* Random data, heavy with the bytes that can pass for protocol. */
static void sim_make_file(uint32_t seed)
{
    static const uint8_t hot[] = { XMODEM_SOH, XMODEM_EOT, XMODEM_CAN };
    sim_seed = seed;
    for (size_t i = 0; i < sizeof(sim_file); i++) {
        sim_seed = sim_seed * 1664525u + 1013904223u;
        uint32_t r = sim_seed >> 8;
        sim_file[i] = (r & 3u) ? (uint8_t)(r >> 8) : hot[(r >> 2) % 3u];
    }
}

/* A sender as in ymodem.txt 7.3.3, up to SIM_TRIES a block. toss drops what
 * arrived while it was sending (7.4). impatient means its timer fires once
 * before the receiver's. The byte at stream position lost_at never arrives. */
static int sim_transfer(bool use_idle, bool toss, bool impatient, uint32_t lost_at)
{
    xmodem_t xm;
    uint8_t frame[133];
    uint32_t pos = 0;
    xmodem_err_t err;

    memset(sim_out, 0, sizeof(sim_out));
    sim_reply = 0;
    xmodem_init(&xm, &g_sim_io, true);
    if (use_idle) xmodem_enable_line_idle(&xm);
    xmodem_start(&xm);

    for (int blk = 0; blk < SIM_BLOCKS; blk++) {
        bool acked = false;
        build_crc_frame_of(frame, (uint8_t)(blk + 1), &sim_file[blk * 128]);

        for (int tries = 0; tries < SIM_TRIES && !acked; tries++) {
            sim_reply = 0;
            for (int i = 0; i < 133; i++) {
                if (toss && i == 132) sim_reply = 0;
                if (pos++ == lost_at) continue;
                err = xmodem_feed(&xm, frame[i]);
                if (err == XMODEM_DONE) return SIM_CUT_SHORT;
                if (err != XMODEM_OK) return SIM_FAILED;
            }
            if (sim_reply == 0 && use_idle) {
                if (impatient) {
                    impatient = false;
                } else {
                    err = xmodem_line_idle(&xm);
                    if (err != XMODEM_OK) return SIM_FAILED;
                }
            }
            acked = sim_reply == XMODEM_ACK;
        }
        if (!acked) return SIM_FAILED;
    }

    for (int tries = 0; tries < SIM_TRIES; tries++) {
        err = XMODEM_OK;
        if (pos++ != lost_at) err = xmodem_feed(&xm, XMODEM_EOT);
        if (err == XMODEM_OK && use_idle) err = xmodem_line_idle(&xm);
        if (err == XMODEM_DONE) {
            bool whole = xmodem_get_total_bytes(&xm) == sizeof(sim_file) &&
                         memcmp(sim_out, sim_file, sizeof(sim_file)) == 0;
            return whole ? SIM_EXACT : SIM_CUT_SHORT;
        }
        if (err != XMODEM_OK) return SIM_FAILED;
    }
    return SIM_FAILED;
}

/* The stream is SIM_BLOCKS frames and an EOT, so this loses each byte once. */
TEST(test_sim_line_idle_survives_any_lost_byte)
{
    for (uint32_t seed = 1; seed <= 3; seed++) {
        sim_make_file(seed);
        for (uint32_t lost = 0; lost <= SIM_BLOCKS * 133; lost++) {
            for (int mode = 0; mode < 4; mode++) {
                ASSERT_EQ(sim_transfer(true, (mode & 1) != 0, (mode & 2) != 0,
                                       lost), SIM_EXACT);
            }
        }
    }
}

/* Without the timer a lost byte may still end in failure, as it always did,
 * but a short file must never come back as complete. */
TEST(test_sim_no_timer_never_reports_a_short_file)
{
    int exact = 0;
    for (uint32_t seed = 1; seed <= 3; seed++) {
        sim_make_file(seed);
        for (uint32_t lost = 0; lost <= SIM_BLOCKS * 133; lost++) {
            for (int toss = 0; toss < 2; toss++) {
                int rc = sim_transfer(false, toss != 0, false, lost);
                ASSERT_TRUE(rc != SIM_CUT_SHORT);
                if (rc == SIM_EXACT) exact++;
            }
        }
    }
    ASSERT_GT(exact, 0);
}

TEST(test_err_timeout_constant)
{
    /* XMODEM_ERR_TIMEOUT is for the caller's timeout detection layer
     * (no bytes received within deadline). The protocol engine itself
     * does not track time — verify the constant value is correct. */
    ASSERT_EQ(XMODEM_ERR_TIMEOUT, -2);
}

TEST_MAIN("xmodem")
    RUN(test_init_null_params);
    RUN(test_init_ok);
    RUN(test_start_sends_nak_checksum_mode);
    RUN(test_start_sends_c_crc_mode);
    RUN(test_start_double_call_rejected);
    RUN(test_single_block_checksum);
    RUN(test_single_block_crc);
    RUN(test_total_bytes_counts_padding);
    RUN(test_eot_completes_transfer);
    RUN(test_bad_checksum_sends_nak);
    RUN(test_bad_crc_sends_nak);
    RUN(test_bad_sequence_error);
    RUN(test_can_cancels);
    RUN(test_cancel_sends_two_can);
    RUN(test_multiple_blocks);
    RUN(test_state_transitions);
    RUN(test_feed_in_wrong_state);
    RUN(test_write_callback_error);
    RUN(test_get_state_null);
    RUN(test_bad_complement_sends_nak);
    RUN(test_err_crc_constant_and_recovery);
    RUN(test_lost_byte_data_eot_is_not_done);
    RUN(test_lost_soh_data_eot_is_not_done);
    RUN(test_lost_soh_of_block_4_is_not_done);
    RUN(test_eot_after_three_blocks_needs_the_repeat);
    RUN(test_line_idle_needs_enable);
    RUN(test_line_idle_outside_a_transfer_refused);
    RUN(test_line_idle_holds_the_nak_until_quiet);
    RUN(test_line_idle_lost_byte_recovers);
    RUN(test_line_idle_drops_a_part_frame);
    RUN(test_line_idle_crc_asks_with_c_until_a_block_lands);
    RUN(test_line_idle_proves_eot);
    RUN(test_line_idle_eot_with_data_behind_it_is_not_done);
    RUN(test_line_idle_splits_cans);
    RUN(test_sim_line_idle_survives_any_lost_byte);
    RUN(test_sim_no_timer_never_reports_a_short_file);
    RUN(test_err_timeout_constant);
TEST_MAIN_END()
