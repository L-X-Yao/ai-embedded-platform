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
static void build_crc_frame(uint8_t *frame, uint8_t blk, uint8_t fill)
{
    frame[0] = XMODEM_SOH;
    frame[1] = blk;
    frame[2] = (uint8_t)(~blk);
    memset(&frame[3], fill, 128);
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
    ASSERT_EQ(xmodem_init(NULL, &g_io, false), XMODEM_ERR_PARAMS);
    ASSERT_EQ(xmodem_init(&xm, NULL, false), XMODEM_ERR_PARAMS);
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
    ASSERT_EQ(xmodem_start(&xm), XMODEM_ERR_PARAMS);
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

    xmodem_err_t err = xmodem_feed(&xm, XMODEM_CAN);
    ASSERT_EQ(err, XMODEM_ERR_CANCEL);
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
    ASSERT_EQ(xmodem_feed(&xm, 0x01), XMODEM_ERR_PARAMS);

    /* feed after complete — should reject */
    xmodem_start(&xm);
    uint8_t frame[132];
    build_checksum_frame(frame, 1, 0x00);
    feed_bytes(&xm, frame, 132);
    xmodem_feed(&xm, XMODEM_EOT);
    ASSERT_EQ(xmodem_get_state(&xm), XMODEM_STATE_COMPLETE);
    ASSERT_EQ(xmodem_feed(&xm, 0x01), XMODEM_ERR_PARAMS);
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

TEST_MAIN("xmodem")
    RUN(test_init_null_params);
    RUN(test_init_ok);
    RUN(test_start_sends_nak_checksum_mode);
    RUN(test_start_sends_c_crc_mode);
    RUN(test_start_double_call_rejected);
    RUN(test_single_block_checksum);
    RUN(test_single_block_crc);
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
TEST_MAIN_END()
