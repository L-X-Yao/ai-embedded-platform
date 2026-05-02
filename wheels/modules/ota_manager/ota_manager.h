// SPDX-License-Identifier: MIT
/*
 * ota_manager -- firmware update orchestration module
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none -- all I/O via callbacks)
 *
 * State machine: idle -> checking -> downloading -> verifying -> ready.
 * Application drives the pace by calling download_step() in a loop.
 * All transport/flash/verification logic injected via ota_mgr_ops_t.
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Error codes ---------- */

typedef enum {
    OTA_MGR_OK           =  0,
    OTA_MGR_ERR_PARAM   = -1,
    OTA_MGR_ERR_VERSION  = -2,
    OTA_MGR_ERR_DOWNLOAD = -3,
    OTA_MGR_ERR_WRITE    = -4,
    OTA_MGR_ERR_VERIFY   = -5,
    OTA_MGR_ERR_STATE    = -6,
} ota_mgr_err_t;

/* ---------- State machine ---------- */

typedef enum {
    OTA_MGR_STATE_IDLE = 0,
    OTA_MGR_STATE_CHECKING,
    OTA_MGR_STATE_DOWNLOADING,
    OTA_MGR_STATE_VERIFYING,
    OTA_MGR_STATE_READY,
    OTA_MGR_STATE_ERROR,
} ota_mgr_state_t;

/* ---------- I/O callbacks (caller provides) ---------- */

typedef struct {
    ota_mgr_err_t (*check_version)(void *ctx, const char *current_ver,
                                   bool *update_available, uint32_t *new_size);
    ota_mgr_err_t (*download_chunk)(void *ctx, uint32_t offset,
                                    uint8_t *buf, uint16_t buf_size,
                                    uint16_t *bytes_read);
    ota_mgr_err_t (*write_chunk)(void *ctx, uint32_t offset,
                                 const uint8_t *data, uint16_t len);
    ota_mgr_err_t (*verify)(void *ctx, uint32_t total_size);
    ota_mgr_err_t (*activate)(void *ctx);
    void *ctx;
} ota_mgr_ops_t;

/* ---------- Configuration ---------- */

#ifndef OTA_MGR_CHUNK_SIZE
#define OTA_MGR_CHUNK_SIZE 256
#endif

#ifndef OTA_MGR_VERSION_MAX
#define OTA_MGR_VERSION_MAX 32
#endif

/* ---------- Manager context ---------- */

typedef struct {
    const ota_mgr_ops_t *ops;
    char                 current_version[OTA_MGR_VERSION_MAX];
    ota_mgr_state_t      state;
    uint32_t             total_size;
    uint32_t             downloaded;
    uint32_t             progress_pct;
} ota_manager_t;

/* ---------- API ---------- */

/**
 * Initialize manager with callbacks and current firmware version.
 * @thread_safe: no
 */
ota_mgr_err_t ota_manager_init(ota_manager_t *mgr,
                               const ota_mgr_ops_t *ops,
                               const char *current_version);

/**
 * Check for update via check_version callback.
 * @thread_safe: no
 */
ota_mgr_err_t ota_manager_check(ota_manager_t *mgr);

/**
 * Download one chunk. Call in a loop until state leaves DOWNLOADING.
 * @thread_safe: no
 */
ota_mgr_err_t ota_manager_download_step(ota_manager_t *mgr,
                                        uint8_t *chunk_buf,
                                        uint16_t buf_size);

/**
 * Verify downloaded firmware via verify callback.
 * @thread_safe: no
 */
ota_mgr_err_t ota_manager_verify(ota_manager_t *mgr);

/**
 * Activate new firmware (switch slot) via activate callback.
 * @thread_safe: no
 */
ota_mgr_err_t ota_manager_activate(ota_manager_t *mgr);

/**
 * Get current state.
 * @thread_safe: no
 */
ota_mgr_state_t ota_manager_get_state(const ota_manager_t *mgr);

/**
 * Get download progress (0-100).
 * @thread_safe: no
 */
uint32_t ota_manager_get_progress(const ota_manager_t *mgr);

/**
 * Reset manager to IDLE state (e.g., after error or abort).
 * @thread_safe: no
 */
void ota_manager_reset(ota_manager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* OTA_MANAGER_H */
