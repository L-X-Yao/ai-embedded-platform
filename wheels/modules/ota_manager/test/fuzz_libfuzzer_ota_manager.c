// SPDX-License-Identifier: MIT
// LibFuzzer harness for ota_manager -- coverage-guided fuzzing
//
// Plays a server and a flash slot that misbehave as the input says. The slot is
// exactly the announced size and the chunk buffer exactly the size passed in, so
// ASan sees any write past the end of either.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_ota_manager.c ../ota_manager.c -o fuzz_ota_manager
// Run:     ./fuzz_ota_manager -max_total_time=60 corpus_ota_manager/

#include "../ota_manager.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define SLOT_MAX 2048
#define CHUNK_MAX 300
#define BIG_MIN 40000000u
#define BIG_SPAN 20000000u
#define BIG_CHUNK 65535u

typedef struct {
    fuzz_input_t *in;
    uint8_t  *slot;
    uint32_t  announced;
    uint32_t  written;
    uint32_t  calls;
    bool      big;
} world_t;

static ota_mgr_err_t world_check(void *ctx, const char *version, bool *available,
                                 uint32_t *new_size) {
    world_t *w = ctx;
    if (strlen(version) >= OTA_MGR_VERSION_MAX) __builtin_trap();
    w->calls++;
    if (w->big) {
        *available = true;
        *new_size = w->announced;
        return OTA_MGR_OK;
    }

    uint8_t pick = fuzz_u8(w->in);
    if (pick % 8 == 0) return OTA_MGR_ERR_DOWNLOAD;

    free(w->slot);
    w->announced = fuzz_u16(w->in) % SLOT_MAX;
    w->slot = malloc(w->announced ? w->announced : 1);
    w->written = 0;
    *available = pick % 8 != 1;
    *new_size = w->announced;
    return OTA_MGR_OK;
}

static ota_mgr_err_t world_download(void *ctx, uint32_t offset, uint8_t *buf,
                                    uint16_t buf_size, uint16_t *bytes_read) {
    world_t *w = ctx;
    w->calls++;
    if (offset != w->written) __builtin_trap();

    if (w->big) {
        *bytes_read = buf_size;
        return OTA_MGR_OK;
    }

    uint8_t pick = fuzz_u8(w->in);
    if (pick % 16 == 0) return OTA_MGR_ERR_DOWNLOAD;

    // A transport can also claim more than it was asked for.
    size_t filled = buf_size ? fuzz_u16(w->in) % ((size_t)buf_size + 1) : 0;
    if (w->in->left == 0) filled = buf_size;
    memset(buf, pick, filled);
    *bytes_read = pick % 16 == 1 ? (uint16_t)(buf_size + 1 + pick) : (uint16_t)filled;
    return OTA_MGR_OK;
}

static ota_mgr_err_t world_write(void *ctx, uint32_t offset, const uint8_t *data,
                                 uint16_t len) {
    world_t *w = ctx;
    w->calls++;
    if (offset != w->written) __builtin_trap();
    if ((uint64_t)offset + len > w->announced) __builtin_trap();

    if (w->big) {
        w->written += len;
        return OTA_MGR_OK;
    }

    if (w->slot && len > 0) memcpy(w->slot + offset, data, len);
    if (fuzz_u8(w->in) % 32 == 0) return OTA_MGR_ERR_WRITE;
    w->written += len;
    return OTA_MGR_OK;
}

static ota_mgr_err_t world_verify(void *ctx, uint32_t total_size) {
    world_t *w = ctx;
    w->calls++;
    if (total_size != w->announced || w->written != w->announced) __builtin_trap();
    return fuzz_u8(w->in) % 8 == 0 ? OTA_MGR_ERR_VERIFY : OTA_MGR_OK;
}

static ota_mgr_err_t world_activate(void *ctx) {
    world_t *w = ctx;
    w->calls++;
    return fuzz_u8(w->in) % 8 == 0 ? OTA_MGR_ERR_WRITE : OTA_MGR_OK;
}

static const ota_mgr_ops_t *world_ops(ota_mgr_ops_t *ops, world_t *w) {
    ops->check_version = world_check;
    ops->download_chunk = world_download;
    ops->write_chunk = world_write;
    ops->verify = world_verify;
    ops->activate = world_activate;
    ops->ctx = w;
    return ops;
}

static void run_steps(fuzz_input_t *in) {
    ota_manager_t *mgr = malloc(sizeof(*mgr));
    if (!mgr) return;

    world_t w;
    ota_mgr_ops_t ops;
    char version[OTA_MGR_VERSION_MAX + 9];
    memset(&w, 0, sizeof(w));
    w.in = in;
    fuzz_take_str(in, version, fuzz_u8(in) % (OTA_MGR_VERSION_MAX + 8));
    if (ota_manager_init(mgr, world_ops(&ops, &w), version) != OTA_MGR_OK) __builtin_trap();

    uint32_t last_pct = 0;
    for (int step = 0; step < 256 && in->left > 0; step++) {
        ota_mgr_state_t before = ota_manager_get_state(mgr);
        ota_mgr_state_t legal = OTA_MGR_STATE_IDLE;
        uint32_t calls = w.calls;
        ota_mgr_err_t err = OTA_MGR_OK;
        uint8_t op = fuzz_u8(in) % 8;

        if (op == 0) {
            err = ota_manager_check(mgr);
            if (err == OTA_MGR_OK && w.announced == 0) __builtin_trap();
            last_pct = 0;
        } else if (op <= 4) {
            size_t size = 1 + fuzz_u16(in) % CHUNK_MAX;
            uint8_t *chunk = malloc(size);
            if (chunk) err = ota_manager_download_step(mgr, chunk, (uint16_t)size);
            free(chunk);
            legal = OTA_MGR_STATE_DOWNLOADING;
        } else if (op == 5) {
            err = ota_manager_verify(mgr);
            legal = OTA_MGR_STATE_VERIFYING;
        } else if (op == 6) {
            err = ota_manager_activate(mgr);
            legal = OTA_MGR_STATE_READY;
        } else {
            ota_manager_reset(mgr);
            if (ota_manager_get_state(mgr) != OTA_MGR_STATE_IDLE) __builtin_trap();
            last_pct = 0;
            continue;
        }

        // A call in the wrong state is refused and touches nothing.
        if (before != legal && (err != OTA_MGR_ERR_STATE || w.calls != calls)) __builtin_trap();

        uint32_t pct = ota_manager_get_progress(mgr);
        ota_mgr_state_t now = ota_manager_get_state(mgr);
        if (pct > 100) __builtin_trap();
        if (now == OTA_MGR_STATE_DOWNLOADING && pct < last_pct) __builtin_trap();
        if (now == OTA_MGR_STATE_VERIFYING && (pct != 100 || w.written != w.announced)) {
            __builtin_trap();
        }
        last_pct = pct;
    }
    free(w.slot);
    free(mgr);
}

// Totals around 43 MB, where the percentage arithmetic can outgrow 32 bits.
static void run_big(fuzz_input_t *in) {
    static uint8_t chunk[BIG_CHUNK];
    ota_manager_t *mgr = malloc(sizeof(*mgr));
    if (!mgr) return;

    world_t w;
    ota_mgr_ops_t ops;
    memset(&w, 0, sizeof(w));
    w.in = in;
    w.big = true;
    w.announced = BIG_MIN + fuzz_u16(in) * (BIG_SPAN / 65536u);
    if (ota_manager_init(mgr, world_ops(&ops, &w), "1") != OTA_MGR_OK) __builtin_trap();
    if (ota_manager_check(mgr) != OTA_MGR_OK) __builtin_trap();

    uint32_t last_pct = 0;
    while (ota_manager_get_state(mgr) == OTA_MGR_STATE_DOWNLOADING) {
        if (ota_manager_download_step(mgr, chunk, BIG_CHUNK) != OTA_MGR_OK) __builtin_trap();
        uint32_t pct = ota_manager_get_progress(mgr);
        if (pct < last_pct || pct > 100) __builtin_trap();
        last_pct = pct;
    }
    free(mgr);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_input_t in;

    fuzz_input_init(&in, data, size);
    run_steps(&in);

    fuzz_input_init(&in, data, size);
    run_big(&in);

    return 0;
}
