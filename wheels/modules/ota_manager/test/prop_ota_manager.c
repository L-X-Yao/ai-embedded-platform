/*
 * prop_ota_manager.c — property-based tests for ota_manager
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_ota_manager.c ../ota_manager.c -o prop_ota_manager
 */

#include "test_harness.h"
#include "property_test.h"
#include "../ota_manager.h"
#include <string.h>

/* --- mock callbacks --- */
static ota_mgr_err_t mock_check(void *ctx, const char *ver,
                                 bool *avail, uint32_t *size) {
    (void)ctx; (void)ver;
    *avail = true;
    *size = 1024;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_download(void *ctx, uint32_t offset,
                                    uint8_t *buf, uint16_t buf_size,
                                    uint16_t *bytes_read) {
    (void)ctx; (void)offset;
    memset(buf, 0xAA, buf_size);
    *bytes_read = buf_size;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_write(void *ctx, uint32_t offset,
                                 const uint8_t *data, uint16_t len) {
    (void)ctx; (void)offset; (void)data; (void)len;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_verify(void *ctx, uint32_t total) {
    (void)ctx; (void)total;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_activate(void *ctx) {
    (void)ctx;
    return OTA_MGR_OK;
}

static const ota_mgr_ops_t mock_ops = {
    .check_version  = mock_check,
    .download_chunk = mock_download,
    .write_chunk    = mock_write,
    .verify         = mock_verify,
    .activate       = mock_activate,
    .ctx            = NULL,
};

/* Property: random operation sequences never crash, state always valid */
PROPERTY(random_ops_no_crash, 10000) {
    ota_manager_t mgr;
    ota_manager_init(&mgr, &mock_ops, "1.0.0");

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 20);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 5;
        if (op == 0) {
            ota_manager_check(&mgr);
        } else if (op == 1) {
            uint8_t chunk[OTA_MGR_CHUNK_SIZE];
            ota_manager_download_step(&mgr, chunk, sizeof(chunk));
        } else if (op == 2) {
            ota_manager_verify(&mgr);
        } else if (op == 3) {
            ota_manager_activate(&mgr);
        } else {
            ota_manager_reset(&mgr);
        }
    }

    ota_mgr_state_t st = ota_manager_get_state(&mgr);
    PROP_ASSERT(st >= OTA_MGR_STATE_IDLE && st <= OTA_MGR_STATE_ERROR);
    uint32_t progress = ota_manager_get_progress(&mgr);
    PROP_ASSERT(progress <= 100);
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    ota_manager_t m1, m2;
    ota_manager_init(&m1, &mock_ops, "1.0.0");
    ota_manager_init(&m2, &mock_ops, "1.0.0");
    ota_manager_init(&m2, &mock_ops, "1.0.0");

    PROP_ASSERT_EQ(ota_manager_get_state(&m1), ota_manager_get_state(&m2));
    PROP_ASSERT_EQ(ota_manager_get_progress(&m1), ota_manager_get_progress(&m2));
}

/* Property: reset always returns to IDLE */
PROPERTY(reset_to_idle, 5000) {
    ota_manager_t mgr;
    ota_manager_init(&mgr, &mock_ops, "1.0.0");

    /* Drive through some states */
    ota_manager_check(&mgr);
    uint8_t chunk[OTA_MGR_CHUNK_SIZE];
    ota_manager_download_step(&mgr, chunk, sizeof(chunk));

    ota_manager_reset(&mgr);
    PROP_ASSERT_EQ(ota_manager_get_state(&mgr), OTA_MGR_STATE_IDLE);
}

TEST_MAIN("ota_manager properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(reset_to_idle);
}
TEST_MAIN_END()
