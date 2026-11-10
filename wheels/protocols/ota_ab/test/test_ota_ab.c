// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "ota_ab.h"

/* ---------- mock flash ---------- */

#define MOCK_SIZE 8192U
static uint8_t mock_mem[MOCK_SIZE];
static int mock_erase_count, mock_write_count, mock_read_count;
static int mock_fail_erase, mock_fail_write, mock_fail_read, mock_read_skip;

/* Power dies during flash op number mock_cut_at (erases and writes, from 0)
 * once mock_tear bytes of it have landed, and nothing lands after that. */
static int      mock_cut_at, mock_ops;
static uint32_t mock_tear;
static bool     mock_dead;

static void mock_reset(void)
{
    memset(mock_mem, 0xFF, MOCK_SIZE);
    mock_erase_count = mock_write_count = mock_read_count = 0;
    mock_fail_erase = mock_fail_write = mock_fail_read = mock_read_skip = 0;
    mock_cut_at = -1;
    mock_ops = 0;
    mock_tear = 0;
    mock_dead = false;
}

/* True if this op does not finish; *landed is how much of it reached flash. */
static bool mock_power_cut(uint32_t len, uint32_t *landed)
{
    *landed = 0;
    if (mock_cut_at < 0) return false;
    if (mock_dead) return true;
    if (mock_ops++ != mock_cut_at) return false;
    *landed = mock_tear < len ? mock_tear : len;
    mock_dead = true;
    return true;
}

static bool mock_erase(void *ctx, uint32_t addr, uint32_t size)
{
    (void)ctx;
    uint32_t landed;
    mock_erase_count++;
    if (mock_fail_erase) { mock_fail_erase = 0; return false; }
    if (addr + size > MOCK_SIZE) return false;
    if (mock_power_cut(size, &landed)) {
        memset(&mock_mem[addr], 0xFF, landed);
        return false;
    }
    memset(&mock_mem[addr], 0xFF, size);
    return true;
}

static bool mock_write(void *ctx, uint32_t addr, const uint8_t *data, uint32_t len)
{
    (void)ctx;
    uint32_t landed;
    mock_write_count++;
    if (mock_fail_write) { mock_fail_write = 0; return false; }
    if (addr + len > MOCK_SIZE) return false;
    if (mock_power_cut(len, &landed)) {
        memcpy(&mock_mem[addr], data, landed);
        return false;
    }
    memcpy(&mock_mem[addr], data, len);
    return true;
}

static bool mock_read(void *ctx, uint32_t addr, uint8_t *buf, uint32_t len)
{
    (void)ctx;
    mock_read_count++;
    if (mock_fail_read && mock_read_skip-- == 0) { mock_fail_read = 0; mock_read_skip = 0; return false; }
    if (addr + len > MOCK_SIZE) return false;
    memcpy(buf, &mock_mem[addr], len);
    return true;
}

static const ota_ab_flash_t mock_flash = {
    .erase = mock_erase,
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL,
};

static const ota_ab_layout_t test_layout = {
    .slot_a_addr   = 0x0000,
    .slot_b_addr   = 0x1000,
    .slot_max_size = 0x1000,
    .params_addr   = 0x1F00,
    .params_size   = 256,
    .max_boot_count = 3,
};

static uint32_t write_test_firmware(uint32_t addr, uint32_t size)
{
    uint8_t buf[64];
    uint32_t remaining = size;
    uint32_t offset = 0;
    while (remaining > 0) {
        uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        for (uint32_t i = 0; i < chunk; i++)
            buf[i] = (uint8_t)((offset + i) & 0xFF);
        mock_write(NULL, addr + offset, buf, chunk);
        offset += chunk;
        remaining -= chunk;
    }
    uint32_t crc = ota_ab_crc32(&mock_mem[addr], size);
    return crc;
}

/* ---------- tests ---------- */

TEST(test_init_fresh)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_err_t e = ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
    ASSERT_EQ(ota_ab_target_slot(&m), 1);
}

TEST(test_init_with_saved_params)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    m.params.active_slot = 1;
    m.params.ota_flag = OTA_AB_FLAG_IDLE;
    m.params.crc32 = ota_ab_crc32((const uint8_t *)&m.params,
                                    sizeof(m.params) - sizeof(uint32_t));
    mock_write(NULL, test_layout.params_addr, (uint8_t *)&m.params, sizeof(m.params));

    ota_ab_t m2;
    ota_ab_err_t e = ota_ab_init(&m2, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m2), 1);
}

TEST(test_mark_pending)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ota_ab_err_t e = ota_ab_mark_pending(&m, 1024, 0xDEADBEEF);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_PENDING);
    ASSERT_EQ(m.params.fw_size, 1024);
    ASSERT_EQ(m.params.fw_crc, 0xDEADBEEF);
}

TEST(test_mark_pending_size_zero)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_mark_pending(&m, 0, 0), OTA_AB_ERR_SIZE);
}

TEST(test_mark_pending_size_too_large)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_mark_pending(&m, 0x2000, 0), OTA_AB_ERR_SIZE);
}

TEST(test_boot_event_pending_switches_slot)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);

    uint32_t crc = write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, crc);
    ota_ab_err_t e = ota_ab_boot_event(&m);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_SUCCESS);
    ASSERT_EQ(m.params.boot_count, 1);
}

TEST(test_confirm_clears_flag)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    uint32_t crc = write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, crc);
    ota_ab_boot_event(&m);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_SUCCESS);

    ota_ab_err_t e = ota_ab_confirm(&m);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE);
    ASSERT_EQ(m.params.boot_count, 0);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
}

TEST(test_rollback_after_max_boots)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    uint32_t crc = write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, crc);

    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);

    ota_ab_boot_event(&m);
    ASSERT_EQ(m.params.boot_count, 2);
    ota_ab_boot_event(&m);
    ASSERT_EQ(m.params.boot_count, 3);

    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE);
}

TEST(test_idle_boot_event_noop)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    int writes_before = mock_write_count;

    ota_ab_err_t e = ota_ab_boot_event(&m);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(mock_write_count, writes_before);
}

TEST(test_slot_addr)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_slot_addr(&m, 0), 0x0000);
    ASSERT_EQ(ota_ab_slot_addr(&m, 1), 0x1000);
}

TEST(test_crc32)
{
    uint8_t data[] = "123456789";
    uint32_t crc = ota_ab_crc32(data, 9);
    ASSERT_EQ(crc, 0xCBF43926u);
}

TEST(test_null_safety)
{
    ota_ab_t m;
    ASSERT_EQ(ota_ab_init(NULL, &mock_flash, &test_layout), OTA_AB_ERR_NULL);
    ASSERT_EQ(ota_ab_init(&m, NULL, &test_layout), OTA_AB_ERR_NULL);
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, NULL), OTA_AB_ERR_NULL);
    ASSERT_EQ(ota_ab_active_slot(NULL), 0);
    ASSERT_EQ(ota_ab_target_slot(NULL), 1);
    ASSERT_EQ(ota_ab_slot_addr(NULL, 0), 0);
    ASSERT_EQ(ota_ab_mark_pending(NULL, 100, 0), OTA_AB_ERR_NULL);
    ASSERT_EQ(ota_ab_boot_event(NULL), OTA_AB_ERR_NULL);
    ASSERT_EQ(ota_ab_confirm(NULL), OTA_AB_ERR_NULL);
}

TEST(test_flash_fail_on_write)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    mock_fail_write = 1;
    ASSERT_EQ(ota_ab_mark_pending(&m, 512, 0), OTA_AB_ERR_FLASH);
}

TEST(test_full_ota_cycle)
{
    ota_ab_t m;
    mock_reset();

    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);

    uint32_t crc_b = write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, crc_b);
    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);

    ota_ab_confirm(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE);

    uint32_t crc_a = write_test_firmware(test_layout.slot_a_addr, 256);
    ota_ab_mark_pending(&m, 256, crc_a);
    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);

    ota_ab_boot_event(&m);
    ota_ab_boot_event(&m);
    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
}

TEST(test_double_confirm_noop)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    uint32_t crc = write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, crc);
    ota_ab_boot_event(&m);
    ota_ab_confirm(&m);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE);

    int writes_before = mock_write_count;
    ota_ab_err_t e = ota_ab_confirm(&m);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(mock_write_count, writes_before);
}

TEST(test_flash_read_fail_on_init)
{
    ota_ab_t m;
    mock_reset();
    mock_fail_read = 1;
    ota_ab_err_t e = ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_ERR_FLASH);
}

TEST(test_flash_erase_fail_on_pending)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    mock_fail_erase = 1;
    ota_ab_err_t e = ota_ab_mark_pending(&m, 512, 0);
    ASSERT_EQ(e, OTA_AB_ERR_FLASH);
}

TEST(test_corrupted_params_recovery)
{
    ota_ab_t m;
    mock_reset();

    ota_ab_params_t bad;
    memset(&bad, 0, sizeof(bad));
    bad.magic = OTA_AB_PARAMS_MAGIC;
    bad.active_slot = 1;
    bad.crc32 = 0xDEADDEAD;
    mock_write(NULL, test_layout.params_addr, (uint8_t *)&bad, sizeof(bad));

    ota_ab_err_t e = ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE);
}

TEST(test_reinit_persists_state)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    uint32_t crc = write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, crc);
    ota_ab_boot_event(&m);
    ota_ab_confirm(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);

    ota_ab_t m2;
    ota_ab_err_t e = ota_ab_init(&m2, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m2), 1);
    ASSERT_EQ(m2.params.ota_flag, OTA_AB_FLAG_IDLE);
    ASSERT_EQ(m2.params.boot_count, 0);
}

TEST(test_mark_pending_overwrite)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ota_ab_mark_pending(&m, 512, 0x1111);
    ASSERT_EQ(m.params.fw_crc, 0x1111);

    ota_ab_err_t e = ota_ab_mark_pending(&m, 256, 0x2222);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(m.params.fw_size, 256);
    ASSERT_EQ(m.params.fw_crc, 0x2222);
}

TEST(test_invalid_active_slot_recovery)
{
    ota_ab_t m;
    mock_reset();

    ota_ab_params_t bad;
    memset(&bad, 0, sizeof(bad));
    bad.magic = OTA_AB_PARAMS_MAGIC;
    bad.active_slot = 5;
    bad.crc32 = ota_ab_crc32((const uint8_t *)&bad,
                              sizeof(bad) - sizeof(uint32_t));
    mock_write(NULL, test_layout.params_addr, (uint8_t *)&bad, sizeof(bad));

    ota_ab_err_t e = ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
}

TEST(test_boot_count_exact_boundary)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    uint32_t crc = write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, crc);

    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
    ASSERT_EQ(m.params.boot_count, 1);

    ota_ab_boot_event(&m);
    ASSERT_EQ(m.params.boot_count, 2);

    ota_ab_boot_event(&m);
    ASSERT_EQ(m.params.boot_count, 3);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);

    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE);
}

TEST(test_target_slot_both_directions)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
    ASSERT_EQ(ota_ab_target_slot(&m), 1);

    uint32_t crc = write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, crc);
    ota_ab_boot_event(&m);
    ota_ab_confirm(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
    ASSERT_EQ(ota_ab_target_slot(&m), 0);
}

TEST(test_crc32_known_vector)
{
    const uint8_t data[] = "123456789";
    uint32_t crc = ota_ab_crc32(data, 9);
    ASSERT_EQ(crc, 0xCBF43926u);
}

TEST(test_boot_event_crc_mismatch)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    write_test_firmware(test_layout.slot_b_addr, 512);
    ota_ab_mark_pending(&m, 512, 0xDEADBEEF);
    ota_ab_err_t e = ota_ab_boot_event(&m);
    ASSERT_EQ(e, OTA_AB_ERR_CRC);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_PENDING);
}

TEST(test_init_err_params_max_boot_zero)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_layout_t bad_layout = test_layout;
    bad_layout.max_boot_count = 0;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &bad_layout), OTA_AB_ERR_PARAM);
}

/* boot_count is 8 bits, so it can never exceed 255 and that limit never
 * triggers a rollback. */
TEST(test_init_err_params_max_boot_255)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_layout_t bad_layout = test_layout;
    bad_layout.max_boot_count = 255;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &bad_layout), OTA_AB_ERR_PARAM);
}

/* 254 is the largest limit: boot_count reaches 255, one past it, on boot 255. */
TEST(test_rollback_at_largest_max_boot_count)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_layout_t layout = test_layout;
    layout.max_boot_count = 254;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &layout), OTA_AB_OK);
    uint32_t crc = write_test_firmware(layout.slot_b_addr, 512);
    ASSERT_EQ(ota_ab_mark_pending(&m, 512, crc), OTA_AB_OK);

    for (int boot = 1; boot <= 254; boot++) {
        ASSERT_EQ(ota_ab_boot_event(&m), OTA_AB_OK);
        ASSERT_EQ(ota_ab_active_slot(&m), 1);
        ASSERT_EQ(m.params.boot_count, boot);
    }

    ASSERT_EQ(ota_ab_boot_event(&m), OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE);
}

TEST(test_init_err_params_size_too_small)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_layout_t bad_layout = test_layout;
    bad_layout.params_size = sizeof(ota_ab_params_t) - 1;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &bad_layout), OTA_AB_ERR_PARAM);
}

TEST(test_boot_event_rejects_fw_size_past_slot)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);

    /* A size no slot can hold, as a damaged but CRC-valid record would carry. */
    m.params.ota_flag = OTA_AB_FLAG_PENDING;
    m.params.fw_size = test_layout.slot_max_size + 1U;
    m.params.fw_crc = 0;

    mock_read_count = 0;
    ASSERT_EQ(ota_ab_boot_event(&m), OTA_AB_ERR_SIZE);
    ASSERT_EQ(mock_read_count, 0);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
}

TEST(test_boot_event_rejects_zero_fw_size)
{
    ota_ab_t m;
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);

    /* An empty image has CRC 0, so it would otherwise pass as verified. */
    m.params.ota_flag = OTA_AB_FLAG_PENDING;
    m.params.fw_size = 0;
    m.params.fw_crc = 0;

    ASSERT_EQ(ota_ab_boot_event(&m), OTA_AB_ERR_SIZE);
    ASSERT_EQ(ota_ab_active_slot(&m), 0);
}

/* ---------- two params copies and power cuts ---------- */

#define FW_SIZE 0x100U
#define COPY_1  0x1F00U
#define COPY_2  0x1E00U

static const ota_ab_layout_t two_copies = {
    .slot_a_addr    = 0x0000,
    .slot_b_addr    = 0x1000,
    .slot_max_size  = 0x0E00,
    .params_addr    = COPY_1,
    .params_size    = 256,
    .max_boot_count = 3,
    .params_addr2   = COPY_2,
};

typedef struct {
    uint8_t active, flag, count;
} ota_state_t;

/* What each step must leave: a good update, then one that rolls back. */
static const ota_state_t lifecycle[] = {
    { 0, OTA_AB_FLAG_IDLE,    0 },  /* fresh flash */
    { 0, OTA_AB_FLAG_PENDING, 0 },  /* 1: mark_pending */
    { 1, OTA_AB_FLAG_SUCCESS, 1 },  /* 2: boot, trial of slot B */
    { 1, OTA_AB_FLAG_IDLE,    0 },  /* 3: confirm */
    { 1, OTA_AB_FLAG_PENDING, 0 },  /* 4: mark_pending */
    { 0, OTA_AB_FLAG_SUCCESS, 1 },  /* 5: boot, trial of slot A */
    { 0, OTA_AB_FLAG_SUCCESS, 2 },  /* 6: boot */
    { 0, OTA_AB_FLAG_SUCCESS, 3 },  /* 7: boot */
    { 1, OTA_AB_FLAG_IDLE,    0 },  /* 8: boot, past 3, roll back */
};
#define LIFECYCLE_STEPS 8

static uint32_t fw_crc[2];

static void lifecycle_setup(void)
{
    mock_reset();
    fw_crc[0] = write_test_firmware(two_copies.slot_a_addr, FW_SIZE);
    fw_crc[1] = write_test_firmware(two_copies.slot_b_addr, FW_SIZE);
}

static void lifecycle_step(ota_ab_t *m, int step)
{
    switch (step) {
    case 1:  (void)ota_ab_mark_pending(m, FW_SIZE, fw_crc[1]); break;
    case 3:  (void)ota_ab_confirm(m); break;
    case 4:  (void)ota_ab_mark_pending(m, FW_SIZE, fw_crc[0]); break;
    default: (void)ota_ab_boot_event(m); break;
    }
}

static bool state_is(const ota_ab_t *m, const ota_state_t *s)
{
    return m->params.active_slot == s->active &&
           m->params.ota_flag == s->flag &&
           m->params.boot_count == s->count;
}

static void power_back(void)
{
    mock_cut_at = -1;
    mock_dead = false;
}

static void put_record(uint32_t addr, uint8_t seq, uint8_t active)
{
    ota_ab_params_t p;
    memset(&p, 0, sizeof(p));
    p.magic = OTA_AB_PARAMS_MAGIC;
    p.active_slot = active;
    p.seq = seq;
    p.crc32 = ota_ab_crc32((const uint8_t *)&p, sizeof(p) - sizeof(p.crc32));
    mock_write(NULL, addr, (const uint8_t *)&p, sizeof(p));
}

/* The run from the finding: confirmed on slot 1, then mark_pending loses its
 * write after the erase. */
TEST(test_cut_after_erase_keeps_the_running_slot)
{
    ota_ab_t m, r;
    lifecycle_setup();
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    for (int s = 1; s <= 3; s++) lifecycle_step(&m, s);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);

    mock_cut_at = 1;
    mock_ops = 0;
    ASSERT_EQ(ota_ab_mark_pending(&m, FW_SIZE, fw_crc[0]), OTA_AB_ERR_FLASH);
    power_back();

    ASSERT_EQ(ota_ab_init(&r, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&r), 1);
    ASSERT_EQ(r.params.ota_flag, OTA_AB_FLAG_IDLE);
}

/* The same window during a trial: the rollback count must not be lost. */
TEST(test_cut_during_a_trial_keeps_the_trial)
{
    ota_ab_t m, r;
    lifecycle_setup();
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    for (int s = 1; s <= 6; s++) lifecycle_step(&m, s);

    mock_cut_at = 1;
    mock_ops = 0;
    ASSERT_EQ(ota_ab_boot_event(&m), OTA_AB_ERR_FLASH);
    power_back();

    ASSERT_EQ(ota_ab_init(&r, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_TRUE(state_is(&r, &lifecycle[6]));
}

/* Lose power in every erase and write of two updates, at every byte: reboot
 * finds the record from before that write or after it, and the update runs
 * on to the end from there. */
TEST(test_power_cut_anywhere_leaves_a_good_record)
{
    for (int cut = 0; cut < 2 * LIFECYCLE_STEPS; cut++) {
        int step = 1 + cut / 2;
        bool is_write = (cut & 1) != 0;

        for (uint32_t tear = 0; tear <= sizeof(ota_ab_params_t); tear++) {
            ota_ab_t m, r;
            lifecycle_setup();
            ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);

            mock_cut_at = cut;
            mock_tear = tear;
            for (int s = 1; s <= step; s++) lifecycle_step(&m, s);
            power_back();

            ASSERT_EQ(ota_ab_init(&r, &mock_flash, &two_copies), OTA_AB_OK);
            bool before = state_is(&r, &lifecycle[step - 1]);
            bool after = state_is(&r, &lifecycle[step]);
            ASSERT_TRUE(before || after);
            ASSERT_EQ(after, is_write && tear == sizeof(ota_ab_params_t));

            for (int s = after ? step + 1 : step; s <= LIFECYCLE_STEPS; s++) {
                lifecycle_step(&r, s);
            }
            ASSERT_TRUE(state_is(&r, &lifecycle[LIFECYCLE_STEPS]));

            ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
            ASSERT_TRUE(state_is(&m, &lifecycle[LIFECYCLE_STEPS]));
        }
    }
}

TEST(test_two_copies_alternate_and_newest_loads)
{
    ota_ab_t m, r;
    ota_ab_params_t c1, c2;
    lifecycle_setup();
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    lifecycle_step(&m, 1);
    lifecycle_step(&m, 2);

    memcpy(&c1, &mock_mem[COPY_1], sizeof(c1));
    memcpy(&c2, &mock_mem[COPY_2], sizeof(c2));
    ASSERT_EQ(c2.seq, 1);
    ASSERT_EQ(c2.ota_flag, OTA_AB_FLAG_PENDING);
    ASSERT_EQ(c1.seq, 2);
    ASSERT_EQ(c1.ota_flag, OTA_AB_FLAG_SUCCESS);

    ASSERT_EQ(ota_ab_init(&r, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_TRUE(state_is(&r, &lifecycle[2]));
}

/* seq is one byte: 0 follows 255, and "newer" is the nearer way round. */
TEST(test_seq_wraps)
{
    ota_ab_t m;
    mock_reset();
    put_record(COPY_1, 255, 0);
    put_record(COPY_2, 0, 1);
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);

    mock_reset();
    put_record(COPY_1, 0, 1);
    put_record(COPY_2, 200, 0);     /* 56 behind 0, not 200 ahead */
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);

    mock_reset();
    put_record(COPY_1, 0, 1);
    put_record(COPY_2, 255, 0);
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
}

TEST(test_equal_seq_takes_the_first_copy)
{
    ota_ab_t m;
    mock_reset();
    put_record(COPY_1, 7, 1);
    put_record(COPY_2, 7, 0);
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
}

TEST(test_damaged_newest_copy_falls_back)
{
    ota_ab_t m;
    mock_reset();
    put_record(COPY_1, 3, 1);
    put_record(COPY_2, 4, 0);
    mock_mem[COPY_2 + 8] ^= 0x01;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&m), 1);
}

TEST(test_both_copies_bad_starts_fresh)
{
    ota_ab_t m;
    mock_reset();
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_TRUE(state_is(&m, &lifecycle[0]));
}

/* Which copy is newest cannot be known with one of them unread. */
TEST(test_either_copy_read_failure_is_an_error)
{
    ota_ab_t m;
    mock_reset();
    put_record(COPY_1, 3, 1);
    put_record(COPY_2, 4, 0);

    mock_fail_read = 1;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_ERR_FLASH);

    mock_fail_read = 1;
    mock_read_skip = 1;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_ERR_FLASH);
}

/* A device that ran with one copy: its record loads, and the first write
 * leaves it alone. */
TEST(test_record_from_one_copy_firmware_carries_over)
{
    ota_ab_t m, r;
    lifecycle_setup();
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &test_layout), OTA_AB_OK);
    for (int s = 1; s <= 3; s++) lifecycle_step(&m, s);

    uint8_t old[sizeof(ota_ab_params_t)];
    memcpy(old, &mock_mem[COPY_1], sizeof(old));

    ASSERT_EQ(ota_ab_init(&r, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_TRUE(state_is(&r, &lifecycle[3]));

    ASSERT_EQ(ota_ab_mark_pending(&r, FW_SIZE, fw_crc[0]), OTA_AB_OK);
    ASSERT_MEMEQ_N(old, &mock_mem[COPY_1], sizeof(old));
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_TRUE(state_is(&m, &lifecycle[4]));
}

/* A write that fails with the power on never touches the good copy, and
 * however often it fails, the retry still reads as the newer record. */
TEST(test_failed_writes_keep_the_good_copy)
{
    ota_ab_t m, r;
    lifecycle_setup();
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &two_copies), OTA_AB_OK);
    for (int s = 1; s <= 3; s++) lifecycle_step(&m, s);

    /* 130: a seq that counted failed tries would now read as the older one. */
    for (int i = 0; i < 130; i++) {
        mock_fail_write = 1;
        ASSERT_EQ(ota_ab_mark_pending(&m, FW_SIZE, fw_crc[0]), OTA_AB_ERR_FLASH);
    }
    ASSERT_EQ(ota_ab_init(&r, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_TRUE(state_is(&r, &lifecycle[3]));

    ASSERT_EQ(ota_ab_mark_pending(&m, FW_SIZE, fw_crc[0]), OTA_AB_OK);
    ASSERT_EQ(ota_ab_init(&r, &mock_flash, &two_copies), OTA_AB_OK);
    ASSERT_TRUE(state_is(&r, &lifecycle[4]));
}

TEST(test_overlapping_params_regions_refused)
{
    ota_ab_t m;
    ota_ab_layout_t layout = two_copies;
    mock_reset();

    layout.params_addr2 = COPY_1;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &layout), OTA_AB_ERR_PARAM);
    layout.params_addr2 = COPY_1 + 255U;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &layout), OTA_AB_ERR_PARAM);
    layout.params_addr2 = COPY_1 - 255U;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &layout), OTA_AB_ERR_PARAM);

    /* Regions that touch do not overlap, from either side. */
    layout.params_addr = COPY_2;
    layout.params_addr2 = COPY_2 + 256U;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &layout), OTA_AB_OK);
    layout.params_addr = COPY_1;
    layout.params_addr2 = COPY_1 - 256U;
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &layout), OTA_AB_OK);
}

/* params_addr2 = 0 must leave the bytes on flash as they always were. */
TEST(test_one_copy_record_is_unchanged)
{
    ota_ab_t m;
    ota_ab_params_t want;
    lifecycle_setup();
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &test_layout), OTA_AB_OK);
    ASSERT_EQ(ota_ab_mark_pending(&m, FW_SIZE, 0x12345678U), OTA_AB_OK);
    ASSERT_EQ(ota_ab_confirm(&m), OTA_AB_OK);       /* no trial: no write */
    ASSERT_EQ(ota_ab_mark_pending(&m, FW_SIZE, 0x12345678U), OTA_AB_OK);

    memset(&want, 0, sizeof(want));
    want.magic = OTA_AB_PARAMS_MAGIC;
    want.ota_flag = OTA_AB_FLAG_PENDING;
    want.fw_size = FW_SIZE;
    want.fw_crc = 0x12345678U;
    want.crc32 = ota_ab_crc32((const uint8_t *)&want,
                              sizeof(want) - sizeof(want.crc32));
    ASSERT_MEMEQ_N(&mock_mem[COPY_1], &want, sizeof(want));
    ASSERT_EQ(mock_mem[COPY_2], 0xFF);              /* nothing else written */
}

/* The documented limit of one copy, so that it stays documented. */
TEST(test_one_copy_loses_the_record_in_the_window)
{
    ota_ab_t m, r;
    lifecycle_setup();
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, &test_layout), OTA_AB_OK);
    for (int s = 1; s <= 3; s++) lifecycle_step(&m, s);

    mock_cut_at = 1;
    mock_ops = 0;
    ASSERT_EQ(ota_ab_mark_pending(&m, FW_SIZE, fw_crc[0]), OTA_AB_ERR_FLASH);
    power_back();

    ASSERT_EQ(ota_ab_init(&r, &mock_flash, &test_layout), OTA_AB_OK);
    ASSERT_EQ(ota_ab_active_slot(&r), 0);
}

TEST_MAIN("ota_ab")
    RUN(test_init_fresh);
    RUN(test_init_with_saved_params);
    RUN(test_mark_pending);
    RUN(test_mark_pending_size_zero);
    RUN(test_mark_pending_size_too_large);
    RUN(test_boot_event_pending_switches_slot);
    RUN(test_confirm_clears_flag);
    RUN(test_rollback_after_max_boots);
    RUN(test_idle_boot_event_noop);
    RUN(test_slot_addr);
    RUN(test_crc32);
    RUN(test_null_safety);
    RUN(test_flash_fail_on_write);
    RUN(test_full_ota_cycle);
    RUN(test_double_confirm_noop);
    RUN(test_flash_read_fail_on_init);
    RUN(test_flash_erase_fail_on_pending);
    RUN(test_corrupted_params_recovery);
    RUN(test_reinit_persists_state);
    RUN(test_mark_pending_overwrite);
    RUN(test_invalid_active_slot_recovery);
    RUN(test_boot_count_exact_boundary);
    RUN(test_target_slot_both_directions);
    RUN(test_crc32_known_vector);
    RUN(test_boot_event_crc_mismatch);
    RUN(test_init_err_params_max_boot_zero);
    RUN(test_init_err_params_max_boot_255);
    RUN(test_rollback_at_largest_max_boot_count);
    RUN(test_init_err_params_size_too_small);
    RUN(test_cut_after_erase_keeps_the_running_slot);
    RUN(test_cut_during_a_trial_keeps_the_trial);
    RUN(test_power_cut_anywhere_leaves_a_good_record);
    RUN(test_two_copies_alternate_and_newest_loads);
    RUN(test_seq_wraps);
    RUN(test_equal_seq_takes_the_first_copy);
    RUN(test_damaged_newest_copy_falls_back);
    RUN(test_both_copies_bad_starts_fresh);
    RUN(test_either_copy_read_failure_is_an_error);
    RUN(test_record_from_one_copy_firmware_carries_over);
    RUN(test_failed_writes_keep_the_good_copy);
    RUN(test_overlapping_params_regions_refused);
    RUN(test_one_copy_record_is_unchanged);
    RUN(test_one_copy_loses_the_record_in_the_window);
    RUN(test_boot_event_rejects_fw_size_past_slot);
    RUN(test_boot_event_rejects_zero_fw_size);
TEST_MAIN_END()

