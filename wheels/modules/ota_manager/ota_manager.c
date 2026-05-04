// SPDX-License-Identifier: MIT
/*
 * ota_manager -- firmware update orchestration module
 *
 * Layer:     L3
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ota_manager.h"
#include "contract.h"
#include <string.h>

/* ---------- helpers ---------- */

static void update_progress(ota_manager_t *mgr)
{
    if (mgr->total_size == 0) {
        mgr->progress_pct = 0;
        return;
    }
    /* avoid overflow: divide first if total_size > 42M */
    if (mgr->total_size > 0x02FFFFFFu) {
        mgr->progress_pct = mgr->downloaded / (mgr->total_size / 100u);
    } else {
        mgr->progress_pct = (mgr->downloaded * 100u) / mgr->total_size;
    }
    if (mgr->progress_pct > 100u) {
        mgr->progress_pct = 100u;
    }
}

/* ---------- API ---------- */

ota_mgr_err_t ota_manager_init(ota_manager_t *mgr,
                               const ota_mgr_ops_t *ops,
                               const char *current_version)
{
    if (!mgr || !ops || !current_version) {
        return OTA_MGR_ERR_PARAM;
    }
    if (!ops->check_version || !ops->download_chunk ||
        !ops->write_chunk || !ops->verify || !ops->activate) {
        return OTA_MGR_ERR_PARAM;
    }

    memset(mgr, 0, sizeof(*mgr));
    mgr->ops = ops;
    mgr->state = OTA_MGR_STATE_IDLE;
    ENSURE(mgr->downloaded == 0 && mgr->progress_pct == 0);

    size_t len = strlen(current_version);
    if (len >= OTA_MGR_VERSION_MAX) {
        len = OTA_MGR_VERSION_MAX - 1u;
    }
    memcpy(mgr->current_version, current_version, len);
    mgr->current_version[len] = '\0';

    return OTA_MGR_OK;
}

ota_mgr_err_t ota_manager_check(ota_manager_t *mgr)
{
    if (!mgr) {
        return OTA_MGR_ERR_PARAM;
    }
    if (mgr->state != OTA_MGR_STATE_IDLE) {
        return OTA_MGR_ERR_STATE;
    }

    mgr->state = OTA_MGR_STATE_CHECKING;

    bool update_available = false;
    uint32_t new_size = 0;
    ota_mgr_err_t err = mgr->ops->check_version(
        mgr->ops->ctx, mgr->current_version, &update_available, &new_size);

    if (err != OTA_MGR_OK) {
        mgr->state = OTA_MGR_STATE_ERROR;
        return err;
    }

    if (!update_available) {
        mgr->state = OTA_MGR_STATE_IDLE;
        return OTA_MGR_ERR_VERSION;
    }

    mgr->total_size = new_size;
    mgr->downloaded = 0;
    mgr->progress_pct = 0;
    mgr->state = OTA_MGR_STATE_DOWNLOADING;

    return OTA_MGR_OK;
}

ota_mgr_err_t ota_manager_download_step(ota_manager_t *mgr,
                                        uint8_t *chunk_buf,
                                        uint16_t buf_size)
{
    if (!mgr || !chunk_buf || buf_size == 0) {
        return OTA_MGR_ERR_PARAM;
    }
    if (mgr->state != OTA_MGR_STATE_DOWNLOADING) {
        return OTA_MGR_ERR_STATE;
    }

    uint16_t bytes_read = 0;
    ota_mgr_err_t err = mgr->ops->download_chunk(
        mgr->ops->ctx, mgr->downloaded, chunk_buf, buf_size, &bytes_read);

    if (err != OTA_MGR_OK) {
        mgr->state = OTA_MGR_STATE_ERROR;
        return OTA_MGR_ERR_DOWNLOAD;
    }

    /* write the chunk to storage */
    err = mgr->ops->write_chunk(
        mgr->ops->ctx, mgr->downloaded, chunk_buf, bytes_read);

    if (err != OTA_MGR_OK) {
        mgr->state = OTA_MGR_STATE_ERROR;
        return OTA_MGR_ERR_WRITE;
    }

    mgr->downloaded += bytes_read;
    update_progress(mgr);
    INVARIANT(mgr->progress_pct <= 100);

    /* check if download is complete */
    if (mgr->downloaded >= mgr->total_size) {
        mgr->progress_pct = 100;
        mgr->state = OTA_MGR_STATE_VERIFYING;
    }

    return OTA_MGR_OK;
}

ota_mgr_err_t ota_manager_verify(ota_manager_t *mgr)
{
    if (!mgr) {
        return OTA_MGR_ERR_PARAM;
    }
    if (mgr->state != OTA_MGR_STATE_VERIFYING) {
        return OTA_MGR_ERR_STATE;
    }

    ota_mgr_err_t err = mgr->ops->verify(mgr->ops->ctx, mgr->total_size);

    if (err != OTA_MGR_OK) {
        mgr->state = OTA_MGR_STATE_ERROR;
        return OTA_MGR_ERR_VERIFY;
    }

    mgr->state = OTA_MGR_STATE_READY;
    return OTA_MGR_OK;
}

ota_mgr_err_t ota_manager_activate(ota_manager_t *mgr)
{
    if (!mgr) {
        return OTA_MGR_ERR_PARAM;
    }
    if (mgr->state != OTA_MGR_STATE_READY) {
        return OTA_MGR_ERR_STATE;
    }

    ota_mgr_err_t err = mgr->ops->activate(mgr->ops->ctx);

    if (err != OTA_MGR_OK) {
        mgr->state = OTA_MGR_STATE_ERROR;
        return err;
    }

    /* after successful activation, return to idle */
    mgr->state = OTA_MGR_STATE_IDLE;
    return OTA_MGR_OK;
}

ota_mgr_state_t ota_manager_get_state(const ota_manager_t *mgr)
{
    if (!mgr) {
        return OTA_MGR_STATE_IDLE;
    }
    return mgr->state;
}

uint32_t ota_manager_get_progress(const ota_manager_t *mgr)
{
    if (!mgr) {
        return 0;
    }
    return mgr->progress_pct;
}

void ota_manager_reset(ota_manager_t *mgr)
{
    if (!mgr) {
        return;
    }
    mgr->state = OTA_MGR_STATE_IDLE;
    mgr->total_size = 0;
    mgr->downloaded = 0;
    mgr->progress_pct = 0;
}
