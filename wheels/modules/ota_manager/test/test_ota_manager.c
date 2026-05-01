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

/* Written data tracking */
static uint8_t  written_buf[4096];
static uint32_t written_total;

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

    memset(written_buf, 0, sizeof(written_buf));
    written_total = 0;
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
    uint16_t to_read = chunk_return_size;
    if (to_read > buf_size) {
        to_read = buf_size;
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
    ASSERT_EQ(ota_manager_init(NULL, &test_ops, "1.0.0"), OTA_MGR_ERR_PARAMS);
}

TEST(test_init_null_ops)
{
    mock_reset();
    ASSERT_EQ(ota_manager_init(&mgr, NULL, "1.0.0"), OTA_MGR_ERR_PARAMS);
}

TEST(test_init_null_version)
{
    mock_reset();
    ASSERT_EQ(ota_manager_init(&mgr, &test_ops, NULL), OTA_MGR_ERR_PARAMS);
}

TEST(test_init_missing_callback)
{
    mock_reset();
    ota_mgr_ops_t bad = test_ops;
    bad.write_chunk = NULL;
    ASSERT_EQ(ota_manager_init(&mgr, &bad, "1.0.0"), OTA_MGR_ERR_PARAMS);
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
    ASSERT_EQ(ota_manager_check(NULL), OTA_MGR_ERR_PARAMS);
}

TEST(test_download_step_null_buf)
{
    mock_reset();
    mock_new_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    ASSERT_EQ(ota_manager_download_step(&mgr, NULL, 256), OTA_MGR_ERR_PARAMS);
}

TEST(test_download_step_zero_size)
{
    mock_reset();
    mock_new_size = 256;
    ota_manager_init(&mgr, &test_ops, "1.0.0");
    ota_manager_check(&mgr);

    uint8_t buf[1];
    ASSERT_EQ(ota_manager_download_step(&mgr, buf, 0), OTA_MGR_ERR_PARAMS);
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
TEST_MAIN_END()
