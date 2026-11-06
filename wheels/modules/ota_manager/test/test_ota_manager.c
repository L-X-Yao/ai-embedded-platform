// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ota_manager.h"
#include <string.h>

/* ---- Mock state ---- */

static bool     mock_update_available;
static uint32_t mock_new_size;
static ota_mgr_err_t mock_check_ret;
static ota_mgr_err_t mock_download_ret;
static ota_mgr_err_t mock_write_ret;
static ota_mgr_err_t mock_verify_ret;
static ota_mgr_err_t mock_activate_ret;

static int check_called;
static int download_called;
static int write_called;
static int verify_called;
static int activate_called;

/* Track what offset download_chunk sees */
static uint32_t last_download_offset;

/* Simulated firmware data: each chunk returns chunk_fill_byte */
static uint8_t  chunk_fill_byte;
static uint16_t chunk_return_size;
static uint32_t server_image_size;   /* 0 = as long as announced */
static uint32_t idle_reads_per_chunk; /* empty reads before each chunk */
static uint32_t idle_reads_left;

/* Written data tracking */
static uint8_t  written_buf[4096];
static uint32_t written_total;
static int      empty_writes;

static void mock_reset(void)
{
    mock_update_available = true;
    mock_new_size         = 1024;
    mock_check_ret        = OTA_MGR_OK;
    mock_download_ret     = OTA_MGR_OK;
    mock_write_ret        = OTA_MGR_OK;
    mock_verify_ret       = OTA_MGR_OK;
    mock_activate_ret     = OTA_MGR_OK;

    check_called    = 0;
    download_called = 0;
    write_called    = 0;
    verify_called   = 0;
    activate_called = 0;

    last_download_offset = 0;
    chunk_fill_byte      = 0xAA;
    chunk_return_size     = OTA_MGR_CHUNK_SIZE;
    server_image_size     = 0;
    idle_reads_per_chunk  = 0;
    idle_reads_left       = 0;

    memset(written_buf, 0, sizeof(written_buf));
    written_total = 0;
    empty_writes  = 0;
}

/* ---- Mock callbacks ---- */

static ota_mgr_err_t mock_check_version(void *ctx, const char *current_ver,
                                        bool *avail, uint32_t *new_size)
{
    (void)ctx;
    (void)current_ver;
    check_called++;
    if (mock_check_ret != OTA_MGR_OK) {
        return mock_check_ret;
    }
    *avail    = mock_update_available;
    *new_size = mock_new_size;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_download_chunk(void *ctx, uint32_t offset,
                                         uint8_t *buf, uint16_t buf_size,
                                         uint16_t *bytes_read)
{
    (void)ctx;
    download_called++;
    last_download_offset = offset;
    if (mock_download_ret != OTA_MGR_OK) {
        return mock_download_ret;
    }
    if (idle_reads_left > 0) {
        idle_reads_left--;
        *bytes_read = 0;
        return OTA_MGR_OK;
    }
    idle_reads_left = idle_reads_per_chunk;
    uint16_t to_read = chunk_return_size;
    if (to_read > buf_size) {
        to_read = buf_size;
    }
    if (server_image_size != 0 && offset + to_read > server_image_size) {
        to_read = (uint16_t)(server_image_size - offset);
    }
    memset(buf, chunk_fill_byte, to_read);
    *bytes_read = to_read;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_write_chunk(void *ctx, uint32_t offset,
                                      const uint8_t *data, uint16_t len)
{
    (void)ctx;
    write_called++;
    if (len == 0) empty_writes++;
    if (mock_write_ret != OTA_MGR_OK) {
        return mock_write_ret;
    }
    if (offset + len <= sizeof(written_buf)) {
        memcpy(written_buf + offset, data, len);
    }
    written_total += len;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_verify(void *ctx, uint32_t total_size)
{
    (void)ctx;
    (void)total_size;
    verify_called++;
    return mock_verify_ret;
}

static ota_mgr_err_t mock_activate(void *ctx)
{
    (void)ctx;
    activate_called++;
    return mock_activate_ret;
}

static const ota_mgr_ops_t test_ops = {
    .check_version  = mock_check_version,
    .download_chunk = mock_download_chunk,
    .write_chunk    = mock_write_chunk,
    .verify         = mock_verify,
    .activate       = mock_activate,
    .ctx            = NULL,
};

static ota_manager_t mgr;

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    mock_reset();
    ota_mgr_err_t err = ota_manager_init(&mgr, &test_ops, "1.0.0");
    ASSERT_EQ(err, OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_IDLE);
    ASSERT_EQ(ota_manager_get_progress(&mgr), 0);
    ASSERT_TRUE(strcmp(mgr.current_version, "1.0.0") == 0);
}

TEST(test_init_null_mgr)
{
    mock_reset();
    ASSERT_EQ(ota_manager_init(NULL, &test_ops, "1.0.0"), OTA_MGR_ERR_PARAM);
}

TEST(test_init_null_ops)
{
    mock_reset();
    ASSERT_EQ(ota_manager_init(&mgr, NULL, "1.0.0"), OTA_MGR_ERR_PARAM);
}

TEST(test_init_null_version)
{
    mock_reset();
    ASSERT_EQ(ota_manager_init(&mgr, &test_ops, NULL), OTA_MGR_ERR_PARAM);
}

TEST(test_init_missing_callback)
{
    mock_reset();
    ota_mgr_ops_t bad = test_ops;
    bad.write_chunk = NULL;
    ASSERT_EQ(ota_manager_init(&mgr, &bad, "1.0.0"), OTA_MGR_ERR_PARAM);
}

TEST(test_check_update_available)
{
    mock_reset();
    ota_manager_init(&mgr, &test_ops, "1.0.0");

    mock_update_available = true;
    mock_new_size = 2048;
    ota_mgr_err_t err = ota_manager_check(&mgr);
    ASSERT_EQ(err, OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_DOWNLOADING);
    ASSERT_EQ(mgr.total_size, 2048);
    ASSERT_EQ(check_called, 1);
}

TEST(test_check_no_update)
{
    mock_reset();
    ota_manager_init(&mgr, &test_ops, "2.0.0");

    mock_update_available = false;
    ota_mgr_err_t err = ota_manager_check(&mgr);
    ASSERT_EQ(err, OTA_MGR_ERR_VERSION);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_IDLE);
}

TEST(test_check_callback_error)
{
    mock_reset();
    ota_manager_init(&mgr, &test_ops, "1.0.0");

    mock_check_ret = OTA_MGR_ERR_DOWNLOAD;
    ota_mgr_err_t err = ota_manager_check(&mgr);
    ASSERT_EQ(err, OTA_MGR_ERR_DOWNLOAD);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);
}

TEST(test_download_full)
{
    mock_reset();
    mock_new_size = 512;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];

    /* step 1: 0..255 */
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_DOWNLOADING);
    ASSERT_EQ(ota_manager_get_progress(&mgr), 50);

    /* step 2: 256..511 -- completes download */
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_VERIFYING);
    ASSERT_EQ(ota_manager_get_progress(&mgr), 100);
    ASSERT_EQ(download_called, 2);
    ASSERT_EQ(write_called, 2);
}

TEST(test_download_progress_tracking)
{
    mock_reset();
    mock_new_size = 1000;
    chunk_return_size = 100;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];

    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_progress(&mgr), 10);

    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_progress(&mgr), 20);

    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_progress(&mgr), 30);
}

TEST(test_download_offset_advances)
{
    mock_reset();
    mock_new_size = 768;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];

    ota_manager_download_step(&mgr, buf, sizeof(buf));
    /* first call: offset 0, next call should be 256 */

    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(last_download_offset, 256);

    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(last_download_offset, 512);
}

TEST(test_verify_success)
{
    mock_reset();
    mock_new_size = 256;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];
    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_VERIFYING);

    ASSERT_EQ(ota_manager_verify(&mgr), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_READY);
    ASSERT_EQ(verify_called, 1);
}

TEST(test_verify_failure)
{
    mock_reset();
    mock_new_size = 256;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];
    ota_manager_download_step(&mgr, buf, sizeof(buf));

    mock_verify_ret = OTA_MGR_ERR_VERIFY;
    ASSERT_EQ(ota_manager_verify(&mgr), OTA_MGR_ERR_VERIFY);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);
}

TEST(test_activate_success)
{
    mock_reset();
    mock_new_size = 256;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];
    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ota_manager_verify(&mgr);

    ASSERT_EQ(ota_manager_activate(&mgr), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_IDLE);
    ASSERT_EQ(activate_called, 1);
}

TEST(test_download_without_check)
{
    mock_reset();
    ota_manager_init(&mgr, &test_ops, "1.0.0");

    /* try download in IDLE state -- should fail */
    uint8_t buf[256];
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)),
              OTA_MGR_ERR_STATE);
}

TEST(test_verify_before_complete)
{
    mock_reset();
    mock_new_size = 512;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];
    ota_manager_download_step(&mgr, buf, sizeof(buf));
    /* only 256 of 512 downloaded -- still DOWNLOADING */
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_DOWNLOADING);
    ASSERT_EQ(ota_manager_verify(&mgr), OTA_MGR_ERR_STATE);
}

TEST(test_activate_before_verify)
{
    mock_reset();
    mock_new_size = 256;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];
    ota_manager_download_step(&mgr, buf, sizeof(buf));
    /* in VERIFYING state, not READY */
    ASSERT_EQ(ota_manager_activate(&mgr), OTA_MGR_ERR_STATE);
}

TEST(test_reset_from_error)
{
    mock_reset();
    ota_manager_init(&mgr, &test_ops, "1.0.0");

    mock_check_ret = OTA_MGR_ERR_DOWNLOAD;
    ota_manager_check(&mgr);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);

    ota_manager_reset(&mgr);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_IDLE);
    ASSERT_EQ(ota_manager_get_progress(&mgr), 0);
}

TEST(test_reset_clears_progress)
{
    mock_reset();
    mock_new_size = 1000;
    chunk_return_size = 500;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[512];
    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_progress(&mgr), 50);

    ota_manager_reset(&mgr);
    ASSERT_EQ(ota_manager_get_progress(&mgr), 0);
    ASSERT_EQ(mgr.downloaded, 0);
    ASSERT_EQ(mgr.total_size, 0);
}

TEST(test_write_failure_mid_download)
{
    mock_reset();
    mock_new_size = 512;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];

    /* first chunk OK */
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_OK);

    /* second chunk: write fails */
    mock_write_ret = OTA_MGR_ERR_WRITE;
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)),
              OTA_MGR_ERR_WRITE);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);
}

TEST(test_download_failure)
{
    mock_reset();
    mock_new_size = 512;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];

    mock_download_ret = OTA_MGR_ERR_DOWNLOAD;
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)),
              OTA_MGR_ERR_DOWNLOAD);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);
}

TEST(test_chunk_smaller_than_buf)
{
    mock_reset();
    mock_new_size = 300;
    chunk_return_size = 100;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];

    /* 3 chunks of 100 bytes to reach 300 */
    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_DOWNLOADING);

    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_DOWNLOADING);

    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_VERIFYING);
}

/* The server holds 900 of the 1000 bytes announced and answers 0 bytes with
 * OK at the end of the file: the loop the header prescribes must end. */
TEST(test_short_image_ends_the_download_loop)
{
    mock_reset();
    mock_new_size = 1000;
    server_image_size = 900;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];
    ota_mgr_err_t err = OTA_MGR_OK;
    uint32_t steps = 0;
    const uint32_t cap = 4u + OTA_MGR_MAX_EMPTY_READS + 1000u;

    while (ota_manager_get_state(&mgr) == OTA_MGR_STATE_DOWNLOADING &&
           steps < cap) {
        err = ota_manager_download_step(&mgr, buf, sizeof(buf));
        steps++;
    }

    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);
    ASSERT_EQ(err, OTA_MGR_ERR_DOWNLOAD);
    ASSERT_EQ(steps, 4u + OTA_MGR_MAX_EMPTY_READS);  /* 256 256 256 132, then 0s */
    ASSERT_EQ(written_total, 900);
    ASSERT_EQ(empty_writes, 0);
    ASSERT_EQ(write_called, 4);
    ASSERT_EQ(verify_called, 0);
}

/* A non-blocking transport may have nothing yet: as long as chunks keep
 * coming, empty reads cost nothing and never reach storage. */
TEST(test_empty_reads_do_not_stop_a_live_download)
{
    mock_reset();
    mock_new_size = 768;
    chunk_return_size = 256;
    idle_reads_per_chunk = OTA_MGR_MAX_EMPTY_READS - 1u;
    idle_reads_left = OTA_MGR_MAX_EMPTY_READS - 1u;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];
    while (ota_manager_get_state(&mgr) == OTA_MGR_STATE_DOWNLOADING) {
        ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_OK);
    }

    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_VERIFYING);
    ASSERT_EQ(ota_manager_get_progress(&mgr), 100);
    ASSERT_EQ(written_total, 768);
    ASSERT_EQ(write_called, 3);
    ASSERT_EQ(empty_writes, 0);
}

/* A failed download must not eat into the allowance of the next one. */
TEST(test_empty_read_count_restarts_with_each_download)
{
    mock_reset();
    mock_new_size = 256;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");

    uint8_t buf[256];
    ota_manager_check(&mgr);
    idle_reads_left = OTA_MGR_MAX_EMPTY_READS;
    while (ota_manager_get_state(&mgr) == OTA_MGR_STATE_DOWNLOADING) {
        (void)ota_manager_download_step(&mgr, buf, sizeof(buf));
    }
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);

    ota_manager_reset(&mgr);
    ASSERT_EQ(ota_manager_check(&mgr), OTA_MGR_OK);
    idle_reads_left = OTA_MGR_MAX_EMPTY_READS - 1u;
    while (ota_manager_get_state(&mgr) == OTA_MGR_STATE_DOWNLOADING) {
        ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_OK);
    }
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_VERIFYING);
}

TEST(test_get_state_null)
{
    ASSERT_EQ(ota_manager_get_state(NULL), OTA_MGR_STATE_IDLE);
}

TEST(test_get_progress_null)
{
    ASSERT_EQ(ota_manager_get_progress(NULL), 0);
}

TEST(test_reset_null)
{
    /* should not crash */
    ota_manager_reset(NULL);
}

TEST(test_check_null)
{
    ASSERT_EQ(ota_manager_check(NULL), OTA_MGR_ERR_PARAM);
}

TEST(test_download_step_null_buf)
{
    mock_reset();
    mock_new_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    ASSERT_EQ(ota_manager_download_step(&mgr, NULL, 256), OTA_MGR_ERR_PARAM);
}

TEST(test_download_step_zero_size)
{
    mock_reset();
    mock_new_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[1];
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, 0), OTA_MGR_ERR_PARAM);
}

TEST(test_full_flow)
{
    mock_reset();
    mock_new_size = 256;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");

    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_IDLE);
    ota_manager_check(&mgr);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_DOWNLOADING);

    uint8_t buf[256];
    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_VERIFYING);
    ASSERT_EQ(ota_manager_get_progress(&mgr), 100);

    ota_manager_verify(&mgr);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_READY);

    ota_manager_activate(&mgr);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_IDLE);

    /* all callbacks invoked exactly once */
    ASSERT_EQ(check_called, 1);
    ASSERT_EQ(download_called, 1);
    ASSERT_EQ(write_called, 1);
    ASSERT_EQ(verify_called, 1);
    ASSERT_EQ(activate_called, 1);
}

TEST(test_version_string_truncation)
{
    mock_reset();
    /* version longer than OTA_MGR_VERSION_MAX (32) */
    char long_ver[64];
    memset(long_ver, 'v', sizeof(long_ver) - 1);
    long_ver[sizeof(long_ver) - 1] = '\0';

    ota_mgr_err_t err = ota_manager_init(&mgr, &test_ops, long_ver);
    ASSERT_EQ(err, OTA_MGR_OK);
    /* version should be truncated but null-terminated */
    ASSERT_EQ(strlen(mgr.current_version), OTA_MGR_VERSION_MAX - 1);
    ASSERT_EQ(mgr.current_version[OTA_MGR_VERSION_MAX - 1], '\0');
}

TEST(test_check_not_from_downloading)
{
    mock_reset();
    mock_new_size = 1024;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_DOWNLOADING);

    /* check again while downloading -- wrong state */
    ASSERT_EQ(ota_manager_check(&mgr), OTA_MGR_ERR_STATE);
}

TEST(test_reset_then_restart)
{
    mock_reset();
    mock_new_size = 256;
    chunk_return_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[256];
    ota_manager_download_step(&mgr, buf, sizeof(buf));
    ota_manager_reset(&mgr);

    /* should be able to check again after reset */
    ASSERT_EQ(ota_manager_check(&mgr), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_DOWNLOADING);
}

/* ================================================================ */

TEST(test_last_chunk_stays_inside_the_image)
{
    uint8_t buf[256];
    mock_reset();
    mock_new_size = 300;
    chunk_return_size = 256;
    ASSERT_EQ(ota_manager_init(&mgr, &test_ops, "1.0.0"), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_check(&mgr), OTA_MGR_OK);

    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_OK);

    ASSERT_EQ(written_total, 300);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_VERIFYING);
}

static ota_mgr_err_t claiming_download(void *ctx, uint32_t offset, uint8_t *buf,
                                       uint16_t buf_size, uint16_t *bytes_read)
{
    (void)ctx; (void)offset; (void)buf;
    *bytes_read = (uint16_t)(buf_size + 1);
    return OTA_MGR_OK;
}

TEST(test_transport_claiming_too_much_is_refused)
{
    uint8_t buf[64];
    ota_mgr_ops_t claiming = test_ops;
    claiming.download_chunk = claiming_download;
    mock_reset();
    ASSERT_EQ(ota_manager_init(&mgr, &claiming, "1.0.0"), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_check(&mgr), OTA_MGR_OK);

    ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_ERR_DOWNLOAD);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);
    ASSERT_EQ(write_called, 0);
}

TEST(test_empty_image_is_refused)
{
    mock_reset();
    mock_new_size = 0;
    ASSERT_EQ(ota_manager_init(&mgr, &test_ops, "1.0.0"), OTA_MGR_OK);

    ASSERT_EQ(ota_manager_check(&mgr), OTA_MGR_ERR_DOWNLOAD);
    ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_ERROR);
}

static ota_mgr_err_t full_download(void *ctx, uint32_t offset, uint8_t *buf,
                                   uint16_t buf_size, uint16_t *bytes_read)
{
    (void)ctx; (void)offset; (void)buf;
    *bytes_read = buf_size;
    return OTA_MGR_OK;
}

static ota_mgr_err_t discard_write(void *ctx, uint32_t offset, const uint8_t *data,
                                   uint16_t len)
{
    (void)ctx; (void)offset; (void)data; (void)len;
    return OTA_MGR_OK;
}

TEST(test_progress_never_falls_on_a_large_image)
{
    static uint8_t buf[60000];
    ota_mgr_ops_t big = test_ops;
    big.download_chunk = full_download;
    big.write_chunk = discard_write;
    mock_reset();
    /* past 42949672 bytes, downloaded * 100 no longer fits in 32 bits */
    mock_new_size = 45000000;
    ASSERT_EQ(ota_manager_init(&mgr, &big, "1.0.0"), OTA_MGR_OK);
    ASSERT_EQ(ota_manager_check(&mgr), OTA_MGR_OK);

    uint32_t last = 0;
    while (ota_manager_get_state(&mgr) == OTA_MGR_STATE_DOWNLOADING) {
        ASSERT_EQ(ota_manager_download_step(&mgr, buf, sizeof(buf)), OTA_MGR_OK);
        ASSERT_TRUE(ota_manager_get_progress(&mgr) >= last);
        last = ota_manager_get_progress(&mgr);
    }
    ASSERT_EQ(last, 100);
}

TEST_MAIN("ota_manager")
    RUN(test_init_valid);
    RUN(test_init_null_mgr);
    RUN(test_init_null_ops);
    RUN(test_init_null_version);
    RUN(test_init_missing_callback);
    RUN(test_check_update_available);
    RUN(test_check_no_update);
    RUN(test_check_callback_error);
    RUN(test_download_full);
    RUN(test_download_progress_tracking);
    RUN(test_download_offset_advances);
    RUN(test_verify_success);
    RUN(test_verify_failure);
    RUN(test_activate_success);
    RUN(test_download_without_check);
    RUN(test_verify_before_complete);
    RUN(test_activate_before_verify);
    RUN(test_reset_from_error);
    RUN(test_reset_clears_progress);
    RUN(test_write_failure_mid_download);
    RUN(test_download_failure);
    RUN(test_chunk_smaller_than_buf);
    RUN(test_get_state_null);
    RUN(test_get_progress_null);
    RUN(test_reset_null);
    RUN(test_check_null);
    RUN(test_download_step_null_buf);
    RUN(test_download_step_zero_size);
    RUN(test_full_flow);
    RUN(test_version_string_truncation);
    RUN(test_check_not_from_downloading);
    RUN(test_reset_then_restart);
    RUN(test_last_chunk_stays_inside_the_image);
    RUN(test_transport_claiming_too_much_is_refused);
    RUN(test_empty_image_is_refused);
    RUN(test_progress_never_falls_on_a_large_image);
    RUN(test_short_image_ends_the_download_loop);
    RUN(test_empty_reads_do_not_stop_a_live_download);
    RUN(test_empty_read_count_restarts_with_each_download);
TEST_MAIN_END()
