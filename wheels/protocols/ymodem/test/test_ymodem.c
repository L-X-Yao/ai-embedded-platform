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

    ymodem_err_t err = ymodem_feed(&ym, YMODEM_CAN);
    ASSERT_EQ(err, YMODEM_ERR_CANCEL);
    ASSERT_EQ(ymodem_get_state(&ym), YMODEM_STATE_ERROR);
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
    RUN(test_block0_longest_name_fits);
    RUN(test_block0_name_too_long_refused);
    RUN(test_block0_name_without_terminator_refused);
TEST_MAIN_END()
