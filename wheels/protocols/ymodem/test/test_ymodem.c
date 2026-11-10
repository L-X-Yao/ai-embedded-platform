// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ymodem.h"
#include <string.h>

/* ---------- mock infrastructure ---------- */

#define MAX_WRITTEN_BLOCKS  32
#define MAX_SENT_BYTES      64

static struct {
    uint32_t offsets[MAX_WRITTEN_BLOCKS];
    uint8_t  data[MAX_WRITTEN_BLOCKS][1024];
    uint16_t lens[MAX_WRITTEN_BLOCKS];
    int      count;
    ymodem_err_t next_err;
} g_written;

static struct {
    uint8_t  bytes[MAX_SENT_BYTES];
    int      count;
} g_sent;

static struct {
    char     filename[64];
    uint32_t file_size;
    int      count;
} g_file_start;

static void mock_reset(void)
{
    memset(&g_written, 0, sizeof(g_written));
    memset(&g_sent, 0, sizeof(g_sent));
    memset(&g_file_start, 0, sizeof(g_file_start));
}

static ymodem_err_t mock_write_block(void *ctx, uint32_t offset,
                                      const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (g_written.next_err != YMODEM_OK) return g_written.next_err;
    if (g_written.count >= MAX_WRITTEN_BLOCKS) return YMODEM_ERR_WRITE;
    g_written.offsets[g_written.count] = offset;
    g_written.lens[g_written.count] = len;
    if (len <= 1024) {
        memcpy(g_written.data[g_written.count], data, len);
    }
    g_written.count++;
    return YMODEM_OK;
}

static ymodem_err_t mock_send_byte(void *ctx, uint8_t byte)
{
    (void)ctx;
    if (g_sent.count < MAX_SENT_BYTES) {
        g_sent.bytes[g_sent.count] = byte;
    }
    g_sent.count++;
    return YMODEM_OK;
}

static void mock_on_file_start(void *ctx, const char *filename, uint32_t file_size)
{
    (void)ctx;
    size_t len = strlen(filename);
    if (len >= sizeof(g_file_start.filename)) {
        len = sizeof(g_file_start.filename) - 1;
    }
    memcpy(g_file_start.filename, filename, len);
    g_file_start.filename[len] = '\0';
    g_file_start.file_size = file_size;
    g_file_start.count++;
}

static const ymodem_io_t g_io = {
    .write_block    = mock_write_block,
    .send_byte      = mock_send_byte,
    .on_file_start  = mock_on_file_start,
    .ctx            = NULL,
};

static const ymodem_io_t g_io_no_callback = {
    .write_block    = mock_write_block,
    .send_byte      = mock_send_byte,
    .on_file_start  = NULL,
    .ctx            = NULL,
};

/* ---------- CRC-16/XMODEM helper ---------- */

static uint16_t calc_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc = (uint16_t)(crc ^ ((uint16_t)data[i] << 8));
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ---------- frame builders ---------- */

/* Build SOH block 0 (header): filename + file size in data field */
static void build_block0(uint8_t *frame, const char *filename, uint32_t file_size)
{
    frame[0] = YMODEM_SOH;
    frame[1] = 0x00;  /* block 0 */
    frame[2] = 0xFF;  /* ~0 */

    uint8_t payload[128];
    memset(payload, 0, sizeof(payload));

    /* copy filename */
    size_t flen = strlen(filename);
    if (flen > 60) flen = 60;
    memcpy(payload, filename, flen);
    /* null terminator after filename is already 0 from memset */

    /* write file size as decimal ASCII after filename+null */
    if (file_size > 0) {
        char size_str[12];
        int pos = 0;
        uint32_t tmp = file_size;
        /* convert to string (reverse then flip) */
        char rev[12];
        int rpos = 0;
        do {
            rev[rpos++] = (char)('0' + (char)(tmp % 10));
            tmp /= 10;
        } while (tmp > 0);
        for (int k = rpos - 1; k >= 0; k--) {
            size_str[pos++] = rev[k];
        }
        size_str[pos] = '\0';
        memcpy(&payload[flen + 1], size_str, (size_t)(pos + 1));
    }

    memcpy(&frame[3], payload, 128);

    uint16_t crc = calc_crc16(payload, 128);
    frame[131] = (uint8_t)(crc >> 8);
    frame[132] = (uint8_t)(crc & 0xFF);
}

/* Build empty block 0 (end of batch marker) */
static void build_empty_block0(uint8_t *frame)
{
    frame[0] = YMODEM_SOH;
    frame[1] = 0x00;
    frame[2] = 0xFF;

    uint8_t payload[128];
    memset(payload, 0, sizeof(payload));
    memcpy(&frame[3], payload, 128);

    uint16_t crc = calc_crc16(payload, 128);
    frame[131] = (uint8_t)(crc >> 8);
    frame[132] = (uint8_t)(crc & 0xFF);
}

/* Build SOH data block (128-byte) */
static void build_soh_data_frame(uint8_t *frame, uint8_t blk, uint8_t fill)
{
    frame[0] = YMODEM_SOH;
    frame[1] = blk;
    frame[2] = (uint8_t)(~blk);
    memset(&frame[3], fill, 128);

    uint16_t crc = calc_crc16(&frame[3], 128);
    frame[131] = (uint8_t)(crc >> 8);
    frame[132] = (uint8_t)(crc & 0xFF);
}

/* Build STX data block (1024-byte) */
static void build_stx_data_frame(uint8_t *frame, uint8_t blk, uint8_t fill)
{
    frame[0] = YMODEM_STX;
    frame[1] = blk;
    frame[2] = (uint8_t)(~blk);
    memset(&frame[3], fill, 1024);

    uint16_t crc = calc_crc16(&frame[3], 1024);
    frame[1027] = (uint8_t)(crc >> 8);
    frame[1028] = (uint8_t)(crc & 0xFF);
}

/* Build SOH block 0 from a raw payload: build_block0 caps names at 60. */
static void build_raw_block0(uint8_t *frame, const uint8_t *payload)
{
    frame[0] = YMODEM_SOH;
    frame[1] = 0x00;
    frame[2] = 0xFF;
    memcpy(&frame[3], payload, 128);

    uint16_t crc = calc_crc16(payload, 128);
    frame[131] = (uint8_t)(crc >> 8);
    frame[132] = (uint8_t)(crc & 0xFF);
}

/* Build SOH block 0 naming name_len 'a's, then NUL and size_text. */
static void build_named_block0(uint8_t *frame, size_t name_len, const char *size_text)
{
    uint8_t payload[128];
    memset(payload, 0, sizeof(payload));
    memset(payload, 'a', name_len);
    memcpy(&payload[name_len + 1], size_text, strlen(size_text));
    build_raw_block0(frame, payload);
}

static void feed_bytes(ymodem_t *ym, const uint8_t *data, int len)
{
    for (int i = 0; i < len; i++) {
        ymodem_feed(ym, data[i]);
    }
}

/* Feed a frame and return the last result that was not YMODEM_OK. */
static ymodem_err_t feed_frame(ymodem_t *ym, const uint8_t *data, int len)
{
    ymodem_err_t last = YMODEM_OK;
    for (int i = 0; i < len; i++) {
        ymodem_err_t err = ymodem_feed(ym, data[i]);
        if (err != YMODEM_OK) last = err;
    }
    return last;
}

/* ---------- tests ---------- */

TEST(test_init_null_params)
{
    ymodem_t ym;
    ASSERT_EQ(ymodem_init(NULL, &g_io), YMODEM_ERR_PARAM);
    ASSERT_EQ(ymodem_init(&ym, NULL), YMODEM_ERR_PARAM);
}

TEST(test_init_ok)
{
    ymodem_t ym;
    mock_reset();
    ASSERT_EQ(ymodem_init(&ym, &g_io), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_IDLE);
    ASSERT_EQ(ymodem_get_file_size(&ym), 0);
    ASSERT_EQ(strlen(ymodem_get_filename(&ym)), 0);
}

TEST(test_start_sends_c)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ASSERT_EQ(ymodem_start(&ym), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);
    ASSERT_EQ(g_sent.count, 1);
    ASSERT_EQ(g_sent.bytes[0], YMODEM_C);
}

TEST(test_start_double_call_rejected)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ASSERT_EQ(ymodem_start(&ym), YMODEM_OK);
    ASSERT_EQ(ymodem_start(&ym), YMODEM_ERR_PARAM);
}

TEST(test_block0_filename_parse)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "firmware.bin", 4096);
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(strcmp(ymodem_get_filename(&ym), "firmware.bin"), 0);
    ASSERT_EQ(ymodem_get_file_size(&ym), 4096);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);

    /* on_file_start callback should have been called */
    ASSERT_EQ(g_file_start.count, 1);
    ASSERT_EQ(strcmp(g_file_start.filename, "firmware.bin"), 0);
    ASSERT_EQ(g_file_start.file_size, 4096);

    /* ACK + 'C' should have been sent after block 0 */
    ASSERT_EQ(g_sent.bytes[g_sent.count - 2], YMODEM_ACK);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], YMODEM_C);
}

TEST(test_block0_no_callback)
{
    /* on_file_start is NULL — should not crash */
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io_no_callback);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "test.bin", 256);
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(strcmp(ymodem_get_filename(&ym), "test.bin"), 0);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);
}

TEST(test_data_blocks_soh)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 header */
    uint8_t hdr[133];
    build_block0(hdr, "data.bin", 256);
    feed_bytes(&ym, hdr, 133);

    /* data block 1 (128 bytes, fill 0xAA) */
    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0xAA);
    feed_bytes(&ym, frame, 133);

    /* data block 2 (128 bytes, fill 0xBB) */
    build_soh_data_frame(frame, 2, 0xBB);
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(g_written.count, 2);
    ASSERT_EQ(g_written.offsets[0], 0);
    ASSERT_EQ(g_written.offsets[1], 128);
    ASSERT_EQ(g_written.data[0][0], 0xAA);
    ASSERT_EQ(g_written.data[1][0], 0xBB);
    ASSERT_EQ(g_written.lens[0], 128);
    ASSERT_EQ(g_written.lens[1], 128);
}

TEST(test_data_blocks_stx)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 header */
    uint8_t hdr[133];
    build_block0(hdr, "big.bin", 2048);
    feed_bytes(&ym, hdr, 133);

    /* STX data block (1024 bytes, fill 0x55) */
    uint8_t frame[1029];
    build_stx_data_frame(frame, 1, 0x55);
    feed_bytes(&ym, frame, 1029);

    ASSERT_EQ(g_written.count, 1);
    ASSERT_EQ(g_written.offsets[0], 0);
    ASSERT_EQ(g_written.lens[0], 1024);
    ASSERT_EQ(g_written.data[0][0], 0x55);
    ASSERT_EQ(g_written.data[0][1023], 0x55);
}

TEST(test_eot_sequence)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 */
    uint8_t hdr[133];
    build_block0(hdr, "fw.bin", 128);
    feed_bytes(&ym, hdr, 133);

    /* one data block */
    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0xCC);
    feed_bytes(&ym, frame, 133);

    int sent_before = g_sent.count;

    /* first EOT -> NAK */
    ymodem_err_t err = ymodem_feed(&ym, YMODEM_EOT);
    ASSERT_EQ(err, YMODEM_OK);
    ASSERT_EQ(g_sent.bytes[sent_before], YMODEM_NAK);

    /* second EOT -> ACK + 'C' */
    err = ymodem_feed(&ym, YMODEM_EOT);
    ASSERT_EQ(err, YMODEM_OK);
    ASSERT_EQ(g_sent.bytes[sent_before + 1], YMODEM_ACK);
    ASSERT_EQ(g_sent.bytes[sent_before + 2], YMODEM_C);

    /* state should be HEADER (awaiting next file) */
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);
}

TEST(test_crc_check_bad_crc_nak)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 */
    uint8_t hdr[133];
    build_block0(hdr, "test.bin", 128);
    feed_bytes(&ym, hdr, 133);

    /* corrupt data block */
    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0xDD);
    frame[132] = (uint8_t)(frame[132] ^ 0xFF);  /* corrupt CRC low byte */
    feed_bytes(&ym, frame, 133);

    /* no block should have been written */
    ASSERT_EQ(g_written.count, 0);
    /* last sent byte should be NAK */
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], YMODEM_NAK);
}

TEST(test_cancel_from_sender)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* ymodem.txt 4.1: two consecutive CANs, so a line hit cannot abort. */
    ymodem_err_t err = ymodem_feed(&ym, YMODEM_CAN);
    ASSERT_EQ(err, YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);

    err = ymodem_feed(&ym, YMODEM_CAN);
    ASSERT_EQ(err, YMODEM_ERR_CANCEL);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_ERROR);
}

TEST(test_can_split_by_another_byte_does_not_cancel)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_CAN), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, 'x'), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_CAN), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);

    /* A block still gets through after the stray CAN. */
    uint8_t hdr[133];
    build_block0(hdr, "fw.bin", 128);
    feed_bytes(&ym, hdr, 133);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);
}

TEST(test_cancel_by_receiver)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    ymodem_cancel(&ym);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_ERROR);
    /* 'C' from start + CAN + CAN = 3 bytes */
    ASSERT_EQ(g_sent.count, 3);
    ASSERT_EQ(g_sent.bytes[1], YMODEM_CAN);
    ASSERT_EQ(g_sent.bytes[2], YMODEM_CAN);
}

TEST(test_batch_end_empty_block0)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 with file */
    uint8_t hdr[133];
    build_block0(hdr, "file1.bin", 128);
    feed_bytes(&ym, hdr, 133);

    /* one data block */
    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);

    /* EOT sequence */
    ymodem_feed(&ym, YMODEM_EOT);  /* NAK */
    ymodem_feed(&ym, YMODEM_EOT);  /* ACK + 'C' */

    /* empty block 0 = end of batch */
    uint8_t end[133];
    build_empty_block0(end);

    ymodem_err_t last = YMODEM_OK;
    for (int i = 0; i < 133; i++) {
        ymodem_err_t err = ymodem_feed(&ym, end[i]);
        if (err != YMODEM_OK) last = err;
    }

    ASSERT_EQ(last, YMODEM_DONE);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_COMPLETE);
}

TEST(test_state_transitions)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_IDLE);

    ymodem_start(&ym);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);

    /* block 0 -> DATA */
    uint8_t hdr[133];
    build_block0(hdr, "fw.bin", 128);
    feed_bytes(&ym, hdr, 133);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);

    /* data block */
    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0x00);
    feed_bytes(&ym, frame, 133);

    /* EOT sequence -> HEADER */
    ymodem_feed(&ym, YMODEM_EOT);
    ymodem_feed(&ym, YMODEM_EOT);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);

    /* empty block 0 -> COMPLETE */
    uint8_t end[133];
    build_empty_block0(end);
    feed_bytes(&ym, end, 133);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_COMPLETE);
}

TEST(test_feed_in_wrong_state)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);

    /* feed before start */
    ASSERT_EQ(ymodem_feed(&ym, 0x01), YMODEM_ERR_PARAM);

    /* feed after complete */
    ymodem_start(&ym);
    uint8_t end[133];
    build_empty_block0(end);
    feed_bytes(&ym, end, 133);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_COMPLETE);
    ASSERT_EQ(ymodem_feed(&ym, 0x01), YMODEM_ERR_PARAM);
}

TEST(test_file_size_tracking)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* file size 200: spans 2 SOH blocks, second block has 72 bytes padding */
    uint8_t hdr[133];
    build_block0(hdr, "small.bin", 200);
    feed_bytes(&ym, hdr, 133);

    ASSERT_EQ(ymodem_get_file_size(&ym), 200);

    /* block 1: 128 bytes (full) */
    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0xAA);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(g_written.lens[0], 128);

    /* block 2: only 72 bytes of real data (200-128) */
    build_soh_data_frame(frame, 2, 0xBB);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(g_written.lens[1], 72);
}

TEST(test_get_state_null)
{
    ASSERT_EQ(ymodem_get_state(NULL), YMODEM_STATE_IDLE);
    ASSERT_EQ(ymodem_get_file_size(NULL), 0);
    ASSERT_EQ(strlen(ymodem_get_filename(NULL)), 0);
}

TEST(test_bad_sequence_error)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 */
    uint8_t hdr[133];
    build_block0(hdr, "seq.bin", 256);
    feed_bytes(&ym, hdr, 133);

    /* send block 2 when block 1 expected */
    uint8_t frame[133];
    build_soh_data_frame(frame, 2, 0xEE);

    ymodem_err_t last = YMODEM_OK;
    for (int i = 0; i < 133; i++) {
        ymodem_err_t err = ymodem_feed(&ym, frame[i]);
        if (err != YMODEM_OK) last = err;
    }

    ASSERT_EQ(last, YMODEM_ERR_SEQ);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_ERROR);
}

TEST(test_write_callback_error)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 */
    uint8_t hdr[133];
    build_block0(hdr, "err.bin", 128);
    feed_bytes(&ym, hdr, 133);

    /* inject write error */
    g_written.next_err = YMODEM_ERR_WRITE;

    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0xFF);

    ymodem_err_t last = YMODEM_OK;
    for (int i = 0; i < 133; i++) {
        ymodem_err_t err = ymodem_feed(&ym, frame[i]);
        if (err != YMODEM_OK) last = err;
    }

    ASSERT_EQ(last, YMODEM_ERR_WRITE);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_ERROR);
}

TEST(test_bad_complement_sends_nak)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 */
    uint8_t hdr[133];
    build_block0(hdr, "comp.bin", 128);
    feed_bytes(&ym, hdr, 133);

    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0xAA);
    frame[2] = 0x00;  /* corrupt complement (should be ~1 = 0xFE) */
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(g_written.count, 0);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], YMODEM_NAK);
}

TEST(test_err_crc_constant_and_recovery)
{
    /* YMODEM_ERR_CRC is defined for caller-level CRC error reporting.
     * The receiver treats CRC mismatch as recoverable (NAK + retry).
     * Verify: (1) constant value, (2) bad CRC triggers NAK + allows retry. */
    ASSERT_EQ(YMODEM_ERR_CRC, -2);

    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* block 0 header */
    uint8_t hdr[133];
    build_block0(hdr, "crc.bin", 128);
    feed_bytes(&ym, hdr, 133);

    /* Send data block with corrupted CRC */
    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0x55);
    frame[131] ^= 0xFF;  /* corrupt CRC high byte */
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(g_written.count, 0);  /* block rejected */
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], YMODEM_NAK);
    /* Receiver stays in DATA state — can accept a retransmitted block */
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);

    /* Retransmit correct block — should succeed */
    build_soh_data_frame(frame, 1, 0x55);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(g_written.count, 1);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], YMODEM_ACK);
}

/* Block 3 with byte 50 lost, then the sender's timer resends it. Resend byte
 * 0 completes the short frame, so the rest lands on a frame boundary, and the
 * data holds 0x04 0x04, which used to pass for the sender's two EOTs. Block 3
 * because the numbers of blocks 1 and 2 are SOH and STX, which would lock a
 * new frame at resend byte 1 and hide the data from the boundary. */
TEST(test_lost_byte_data_eots_are_not_an_eot_pair)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t hdr[133];
    build_block0(hdr, "fw.bin", 512);
    feed_bytes(&ym, hdr, 133);

    uint8_t frame[133];
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);
    build_soh_data_frame(frame, 2, 0x12);
    feed_bytes(&ym, frame, 133);
    int sent = g_sent.count;

    build_soh_data_frame(frame, 3, 0x33);
    feed_bytes(&ym, frame, 50);
    feed_bytes(&ym, frame + 51, 82);

    memset(&frame[3 + 20], YMODEM_EOT, 2);
    uint16_t crc = calc_crc16(&frame[3], 128);
    frame[131] = (uint8_t)(crc >> 8);
    frame[132] = (uint8_t)(crc & 0xFF);
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);
    ASSERT_EQ(g_sent.count, sent + 1);                   /* the one NAK */
    ASSERT_EQ(g_sent.bytes[sent], YMODEM_NAK);

    /* The next clean resend still lands. */
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(g_written.count, 3);
    ASSERT_EQ(g_written.data[2][20], YMODEM_EOT);
}

/* Two EOTs with file data between them are not the sender's pair. */
TEST(test_eot_pair_split_by_data_is_not_a_pair)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t hdr[133];
    build_block0(hdr, "fw.bin", 256);
    feed_bytes(&ym, hdr, 133);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, 'x'), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);
}

/* A stray 0x04 and a good block: the next 0x04 is a first EOT, not a second. */
TEST(test_block_between_eots_restarts_the_pair)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "fw.bin", 256);
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);
    int sent = g_sent.count;

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(g_sent.bytes[sent], YMODEM_NAK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);
}

/* The run from the finding: 5000 announced, 128 sent, then the two EOTs. */
TEST(test_short_transfer_is_an_error)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "fw.bin", 5000);
    feed_bytes(&ym, frame, 133);
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(ymodem_get_total_received(&ym), 128);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    int sent = g_sent.count;
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_ERR_SHORT);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_ERROR);

    /* The sender hears CAN CAN, not an ACK for a file we refuse. */
    ASSERT_EQ(g_sent.count, sent + 2);
    ASSERT_EQ(g_sent.bytes[sent], YMODEM_CAN);
    ASSERT_EQ(g_sent.bytes[sent + 1], YMODEM_CAN);

    /* And the batch cannot go on to report DONE. */
    build_empty_block0(frame);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_ERR_PARAM);
}

/* One byte short of the size, the last block padded out to 128. */
TEST(test_one_byte_short_is_an_error)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "fw.bin", 129);
    feed_bytes(&ym, frame, 133);
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_ERR_SHORT);
}

/* Padding does not count: 100 announced, a 128-byte block, a whole file. */
TEST(test_padded_last_block_is_not_short)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "fw.bin", 100);
    feed_bytes(&ym, frame, 133);
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(ymodem_get_total_received(&ym), 100);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);

    build_empty_block0(frame);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_DONE);
}

/* No size in block 0: nothing to compare, and the count is the caller's. */
TEST(test_no_announced_size_is_not_short)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "fw.bin", 0);
    feed_bytes(&ym, frame, 133);
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);
    ASSERT_EQ(ymodem_get_total_received(&ym), 128);
}

/* A whole first file does not excuse a short second one. */
TEST(test_short_second_file_is_an_error)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "a.bin", 128);
    feed_bytes(&ym, frame, 133);
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);

    build_block0(frame, "b.bin", 256);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(ymodem_get_total_received(&ym), 0);
    build_soh_data_frame(frame, 1, 0x22);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_ERR_SHORT);
}

TEST(test_get_total_received_null)
{
    ASSERT_EQ(ymodem_get_total_received(NULL), 0);
}

TEST(test_line_idle_needs_enable)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_ERR_PARAM);
    ASSERT_EQ(ymodem_enable_line_idle(&ym), YMODEM_ERR_PARAM);  /* too late */
    ASSERT_EQ(ymodem_line_idle(NULL), YMODEM_ERR_PARAM);
    ASSERT_EQ(ymodem_enable_line_idle(NULL), YMODEM_ERR_PARAM);
}

TEST(test_line_idle_outside_a_transfer_refused)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ASSERT_EQ(ymodem_enable_line_idle(&ym), YMODEM_OK);
    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_ERR_PARAM);  /* not started */

    ymodem_start(&ym);
    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_OK);
    ASSERT_EQ(g_sent.count, 1);                          /* nothing to ask for */

    ymodem_cancel(&ym);
    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_ERR_PARAM);
}

/* ymodem.txt 7.4: wait for the line to clear, then NAK. */
TEST(test_line_idle_holds_the_nak_until_quiet)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_enable_line_idle(&ym);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "fw.bin", 128);
    feed_bytes(&ym, frame, 133);
    int sent = g_sent.count;

    build_soh_data_frame(frame, 1, 0x11);
    frame[132] = (uint8_t)(frame[132] ^ 0xFF);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(g_sent.count, sent);

    /* Everything is dropped now, even a good block, two CANs and two EOTs. */
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_CAN), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_CAN), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(g_written.count, 0);
    ASSERT_EQ(g_sent.count, sent);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);

    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_OK);
    ASSERT_EQ(g_sent.count, sent + 1);
    ASSERT_EQ(g_sent.bytes[sent], YMODEM_NAK);

    /* A second quiet period asks for nothing more. */
    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_OK);
    ASSERT_EQ(g_sent.count, sent + 1);

    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(g_written.count, 1);
    ASSERT_EQ(g_sent.bytes[sent + 1], YMODEM_ACK);
}

/* A sender that has not started would not read NAK as a request to. */
TEST(test_line_idle_asks_for_block0_with_c)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_enable_line_idle(&ym);
    ymodem_start(&ym);

    ASSERT_EQ(ymodem_feed(&ym, 'x'), YMODEM_OK);
    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_OK);
    ASSERT_EQ(g_sent.count, 2);
    ASSERT_EQ(g_sent.bytes[1], YMODEM_C);
}

TEST(test_line_idle_drops_a_part_frame)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_enable_line_idle(&ym);
    ymodem_start(&ym);

    uint8_t frame[1029];
    build_block0(frame, "fw.bin", 1024);
    feed_bytes(&ym, frame, 133);

    build_stx_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 700);
    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_OK);
    ASSERT_EQ(g_sent.bytes[g_sent.count - 1], YMODEM_NAK);

    feed_bytes(&ym, frame, 1029);
    ASSERT_EQ(g_written.count, 1);
}

/* Two CANs a quiet period apart are two line hits, not a cancel. */
TEST(test_line_idle_splits_cans)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_enable_line_idle(&ym);
    ymodem_start(&ym);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_CAN), YMODEM_OK);
    ASSERT_EQ(ymodem_line_idle(&ym), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_CAN), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_CAN), YMODEM_ERR_CANCEL);
}

/* ---------- one lost byte, every position ---------- */

#define SIM_TRIES      10
#define SIM_BLOCKS     5
#define SIM_PADDED     2432u
#define SIM_FILE_SIZE  (SIM_PADDED - 50u)  /* the last block is padded */

enum { SIM_EXACT, SIM_FAILED, SIM_CUT_SHORT };

/* Block 4 is there because its number looks like EOT. */
static const uint16_t sim_sizes[SIM_BLOCKS] = { 1024, 128, 128, 128, 1024 };

static uint8_t  sim_file[SIM_PADDED];
static uint8_t  sim_out[SIM_PADDED];
static uint32_t sim_out_len;
static uint8_t  sim_frames[SIM_BLOCKS][1029];
static uint8_t  sim_hdr[133];
static uint8_t  sim_end[133];
static uint8_t  sim_reply;

typedef struct {
    ymodem_t ym;
    bool     use_idle;
    bool     toss;       /* drops what arrived while sending (7.4) */
    bool     impatient;  /* sender's timer fires once before the receiver's */
    bool     done;
    uint32_t lost_at;    /* this stream position never arrives */
    uint32_t pos;
} sim_t;

static ymodem_err_t sim_write(void *ctx, uint32_t offset,
                              const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (offset + len > sizeof(sim_out)) return YMODEM_ERR_WRITE;
    memcpy(sim_out + offset, data, len);
    if (offset + len > sim_out_len) sim_out_len = offset + len;
    return YMODEM_OK;
}

static ymodem_err_t sim_send(void *ctx, uint8_t byte)
{
    (void)ctx;
    if (sim_reply == 0) sim_reply = byte;  /* oldest unread byte */
    return YMODEM_OK;
}

static const ymodem_io_t g_sim_io = {
    .write_block = sim_write,
    .send_byte   = sim_send,
    .ctx         = NULL,
};

/* Random data, heavy with the bytes that can pass for protocol. */
static void sim_make_file(uint32_t seed)
{
    static const uint8_t hot[] = { YMODEM_SOH, YMODEM_STX, YMODEM_EOT, YMODEM_CAN };
    for (size_t i = 0; i < sizeof(sim_file); i++) {
        seed = seed * 1664525u + 1013904223u;
        uint32_t r = seed >> 8;
        sim_file[i] = (r & 3u) ? (uint8_t)(r >> 8) : hot[(r >> 2) & 3u];
    }

    build_block0(sim_hdr, "sim.bin", SIM_FILE_SIZE);
    build_empty_block0(sim_end);

    const uint8_t *data = sim_file;
    for (int b = 0; b < SIM_BLOCKS; b++) {
        uint16_t size = sim_sizes[b];
        uint8_t blk = (uint8_t)(b + 1);
        uint8_t *f = sim_frames[b];
        f[0] = size == 1024 ? YMODEM_STX : YMODEM_SOH;
        f[1] = blk;
        f[2] = (uint8_t)(~blk);
        memcpy(&f[3], data, size);
        uint16_t crc = calc_crc16(data, size);
        f[3 + size] = (uint8_t)(crc >> 8);
        f[4 + size] = (uint8_t)(crc & 0xFF);
        data += size;
    }
}

/* A sender as in ymodem.txt 7.3.3, up to SIM_TRIES a block. */
static bool sim_put(sim_t *s, const uint8_t *frame, uint16_t len)
{
    for (int tries = 0; tries < SIM_TRIES; tries++) {
        sim_reply = 0;
        for (uint16_t i = 0; i < len; i++) {
            if (s->toss && i == len - 1) sim_reply = 0;
            if (s->pos++ == s->lost_at) continue;
            ymodem_err_t err = ymodem_feed(&s->ym, frame[i]);
            if (err == YMODEM_DONE) {
                s->done = true;
                return true;
            }
            if (err != YMODEM_OK) return false;
        }
        if (sim_reply == 0 && s->use_idle) {
            if (s->impatient) {
                s->impatient = false;
            } else if (ymodem_line_idle(&s->ym) != YMODEM_OK) {
                return false;
            }
        }
        if (sim_reply == YMODEM_ACK) return true;
    }
    return false;
}

static int sim_transfer(bool use_idle, bool toss, bool impatient, uint32_t lost_at)
{
    static sim_t s;

    memset(&s, 0, sizeof(s));
    memset(sim_out, 0, sizeof(sim_out));
    sim_out_len = 0;
    sim_reply = 0;
    s.use_idle = use_idle;
    s.toss = toss;
    s.impatient = impatient;
    s.lost_at = lost_at;

    ymodem_init(&s.ym, &g_sim_io);
    if (use_idle) ymodem_enable_line_idle(&s.ym);
    ymodem_start(&s.ym);

    if (!sim_put(&s, sim_hdr, 133)) return SIM_FAILED;
    for (int b = 0; b < SIM_BLOCKS; b++) {
        if (!sim_put(&s, sim_frames[b], (uint16_t)(sim_sizes[b] + 5))) return SIM_FAILED;
        if (s.done) return SIM_CUT_SHORT;
    }

    bool acked = false;
    for (int tries = 0; tries < SIM_TRIES && !acked; tries++) {
        sim_reply = 0;
        if (s.pos++ != lost_at && ymodem_feed(&s.ym, YMODEM_EOT) != YMODEM_OK) {
            return SIM_FAILED;
        }
        if (sim_reply == 0 && use_idle && ymodem_line_idle(&s.ym) != YMODEM_OK) {
            return SIM_FAILED;
        }
        acked = sim_reply == YMODEM_ACK;
    }
    if (!acked || !sim_put(&s, sim_end, 133) || !s.done) return SIM_FAILED;

    bool whole = sim_out_len == SIM_FILE_SIZE &&
                 memcmp(sim_out, sim_file, SIM_FILE_SIZE) == 0;
    return whole ? SIM_EXACT : SIM_CUT_SHORT;
}

/* header, data, two EOTs and the end block: this loses each byte once */
#define SIM_STREAM (133u + SIM_PADDED + SIM_BLOCKS * 5u + 2u + 133u)

TEST(test_sim_line_idle_survives_any_lost_byte)
{
    sim_make_file(1);
    for (uint32_t lost = 0; lost <= SIM_STREAM; lost++) {
        for (int mode = 0; mode < 4; mode++) {
            ASSERT_EQ(sim_transfer(true, (mode & 1) != 0, (mode & 2) != 0, lost),
                      SIM_EXACT);
        }
    }
}

/* Without the timer a lost byte usually fails the transfer, as it always
 * did, but it must fail out loud. */
TEST(test_sim_no_timer_never_reports_a_short_file)
{
    int exact = 0;
    sim_make_file(2);
    for (uint32_t lost = 0; lost <= SIM_STREAM; lost++) {
        for (int toss = 0; toss < 2; toss++) {
            int rc = sim_transfer(false, toss != 0, false, lost);
            ASSERT_TRUE(rc != SIM_CUT_SHORT);
            if (rc == SIM_EXACT) exact++;
        }
    }
    ASSERT_GT(exact, 0);
}

TEST(test_block0_longest_name_fits)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_named_block0(frame, 63, "100");
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);
    ASSERT_EQ(strlen(ymodem_get_filename(&ym)), 63);
    ASSERT_EQ(ymodem_get_file_size(&ym), 100);
    ASSERT_EQ(g_file_start.count, 1);

    build_soh_data_frame(frame, 1, 0x11);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);
    ASSERT_EQ(g_written.lens[0], 100);
}

static void assert_block0_refused(const uint8_t *frame)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_ERR_OVERFLOW);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_ERROR);
    ASSERT_EQ(g_file_start.count, 0);
    /* The sender hears CAN CAN instead of retrying a header we will refuse. */
    ASSERT_EQ(g_sent.count, 3);   /* 'C' from ymodem_start, then the two CANs */
    ASSERT_EQ(g_sent.bytes[1], YMODEM_CAN);
    ASSERT_EQ(g_sent.bytes[2], YMODEM_CAN);

    uint8_t data[133];
    build_soh_data_frame(data, 1, 0x22);
    feed_bytes(&ym, data, 133);
    ASSERT_EQ(g_written.count, 0);
}

TEST(test_block0_name_too_long_refused)
{
    uint8_t frame[133];
    uint8_t payload[128];

    /* One past the 63 that fit, with an honest size after it. */
    build_named_block0(frame, 64, "100");
    assert_block0_refused(frame);

    /* The tail of the name must not be read as the size: "12", then "7". */
    memset(payload, 0, sizeof(payload));
    memset(payload, 'a', 64);
    memcpy(&payload[64], "12\0" "5000 0", 9);
    build_raw_block0(frame, payload);
    assert_block0_refused(frame);

    memset(payload, 0, sizeof(payload));
    memset(payload, 'a', 64);
    memcpy(&payload[64], "7.bin\0" "100", 9);
    build_raw_block0(frame, payload);
    assert_block0_refused(frame);
}

TEST(test_block0_name_without_terminator_refused)
{
    uint8_t frame[133];
    uint8_t payload[128];
    memset(payload, 'a', sizeof(payload));
    build_raw_block0(frame, payload);
    assert_block0_refused(frame);
}

static uint32_t size_from_header(const char *text)
{
    ymodem_t ym;
    uint8_t frame[133];
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);
    build_named_block0(frame, 4, text);
    feed_bytes(&ym, frame, 133);
    return ymodem_get_file_size(&ym);
}

TEST(test_block0_size_at_32_bit_limit)
{
    ASSERT_EQ(size_from_header("4294967294"), 4294967294u);
    ASSERT_EQ(size_from_header("4294967295"), UINT32_MAX);
}

TEST(test_block0_size_past_32_bits_saturates)
{
    /* 2^32 and its neighbours must not wrap to a size of 0 to 3 bytes. */
    ASSERT_EQ(size_from_header("4294967296"), UINT32_MAX);
    ASSERT_EQ(size_from_header("4294967299"), UINT32_MAX);
    ASSERT_EQ(size_from_header("4294967300"), UINT32_MAX);
    ASSERT_EQ(size_from_header("99999999999"), UINT32_MAX);
    ASSERT_EQ(size_from_header("18446744073709551616"), UINT32_MAX);
}

/* A lost ACK makes the sender repeat the block: it is ACKed, not written twice. */
TEST(test_duplicate_block_reacked)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "dup.bin", 256);
    feed_bytes(&ym, frame, 133);

    build_soh_data_frame(frame, 1, 0x11);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);
    ASSERT_EQ(g_written.count, 1);

    int sent = g_sent.count;
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);
    ASSERT_EQ(g_written.count, 1);
    ASSERT_EQ(g_sent.count, sent + 1);
    ASSERT_EQ(g_sent.bytes[sent], YMODEM_ACK);

    /* The transfer carries on with the next block in its place. */
    build_soh_data_frame(frame, 2, 0x22);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);
    ASSERT_EQ(g_written.count, 2);
    ASSERT_EQ(g_written.offsets[1], 128);
    ASSERT_EQ(g_written.data[1][0], 0x22);
}

TEST(test_duplicate_block0_reacked)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    uint8_t frame[133];
    build_block0(frame, "dup.bin", 256);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);

    int sent = g_sent.count;
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_DATA);
    ASSERT_EQ(g_file_start.count, 1);
    ASSERT_EQ(g_sent.bytes[sent], YMODEM_ACK);
}

TEST(test_two_file_batch_resets_per_file)
{
    ymodem_t ym;
    mock_reset();
    ymodem_init(&ym, &g_io);
    ymodem_start(&ym);

    /* File one: 200 bytes in two blocks, the second trimmed to 72. */
    uint8_t frame[133];
    build_block0(frame, "one.bin", 200);
    feed_bytes(&ym, frame, 133);
    build_soh_data_frame(frame, 1, 0x11);
    feed_bytes(&ym, frame, 133);
    build_soh_data_frame(frame, 2, 0x22);
    feed_bytes(&ym, frame, 133);
    ASSERT_EQ(g_written.count, 2);
    ASSERT_EQ(g_written.lens[1], 72);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_HEADER);

    /* File two starts at offset 0, with its own name, size and numbering. */
    build_block0(frame, "two.bin", 100);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);
    ASSERT_EQ(g_file_start.count, 2);
    ASSERT_STR_EQ(ymodem_get_filename(&ym), "two.bin");
    ASSERT_EQ(ymodem_get_file_size(&ym), 100);

    build_soh_data_frame(frame, 1, 0x33);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_OK);
    ASSERT_EQ(g_written.count, 3);
    ASSERT_EQ(g_written.offsets[2], 0);
    ASSERT_EQ(g_written.lens[2], 100);
    ASSERT_EQ(g_written.data[2][0], 0x33);

    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    ASSERT_EQ(ymodem_feed(&ym, YMODEM_EOT), YMODEM_OK);
    build_empty_block0(frame);
    ASSERT_EQ(feed_frame(&ym, frame, 133), YMODEM_DONE);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_COMPLETE);
}

TEST_MAIN("ymodem")
    RUN(test_init_null_params);
    RUN(test_init_ok);
    RUN(test_start_sends_c);
    RUN(test_start_double_call_rejected);
    RUN(test_block0_filename_parse);
    RUN(test_block0_no_callback);
    RUN(test_data_blocks_soh);
    RUN(test_data_blocks_stx);
    RUN(test_eot_sequence);
    RUN(test_crc_check_bad_crc_nak);
    RUN(test_cancel_from_sender);
    RUN(test_can_split_by_another_byte_does_not_cancel);
    RUN(test_cancel_by_receiver);
    RUN(test_batch_end_empty_block0);
    RUN(test_state_transitions);
    RUN(test_feed_in_wrong_state);
    RUN(test_file_size_tracking);
    RUN(test_get_state_null);
    RUN(test_bad_sequence_error);
    RUN(test_write_callback_error);
    RUN(test_bad_complement_sends_nak);
    RUN(test_err_crc_constant_and_recovery);
    RUN(test_lost_byte_data_eots_are_not_an_eot_pair);
    RUN(test_eot_pair_split_by_data_is_not_a_pair);
    RUN(test_block_between_eots_restarts_the_pair);
    RUN(test_short_transfer_is_an_error);
    RUN(test_one_byte_short_is_an_error);
    RUN(test_padded_last_block_is_not_short);
    RUN(test_no_announced_size_is_not_short);
    RUN(test_short_second_file_is_an_error);
    RUN(test_get_total_received_null);
    RUN(test_line_idle_needs_enable);
    RUN(test_line_idle_outside_a_transfer_refused);
    RUN(test_line_idle_holds_the_nak_until_quiet);
    RUN(test_line_idle_asks_for_block0_with_c);
    RUN(test_line_idle_drops_a_part_frame);
    RUN(test_line_idle_splits_cans);
    RUN(test_sim_line_idle_survives_any_lost_byte);
    RUN(test_sim_no_timer_never_reports_a_short_file);
    RUN(test_block0_longest_name_fits);
    RUN(test_block0_name_too_long_refused);
    RUN(test_block0_name_without_terminator_refused);
    RUN(test_block0_size_at_32_bit_limit);
    RUN(test_block0_size_past_32_bits_saturates);
    RUN(test_duplicate_block_reacked);
    RUN(test_duplicate_block0_reacked);
    RUN(test_two_file_batch_resets_per_file);
TEST_MAIN_END()
