// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "ota_ab.h"

/* ---------- mock flash ---------- */

#define MOCK_SIZE 8192U
static uint8_t mock_mem[MOCK_SIZE];
static int mock_erase_count, mock_write_count, mock_read_count;
static int mock_fail_erase, mock_fail_write, mock_fail_read;

static void mock_reset(void)
{
    memset(mock_mem, 0xFF, MOCK_SIZE);
    mock_erase_count = mock_write_count = mock_read_count = 0;
    mock_fail_erase = mock_fail_write = mock_fail_read = 0;
}

static bool mock_erase(void *ctx, uint32_t addr, uint32_t size)
{
    (void)ctx;
    mock_erase_count++;
    if (mock_fail_erase) { mock_fail_erase = 0; return false; }
    if (addr + size > MOCK_SIZE) return false;
    memset(&mock_mem[addr], 0xFF, size);
    return true;
}

static bool mock_write(void *ctx, uint32_t addr, const uint8_t *data, uint32_t len)
{
    (void)ctx;
    mock_write_count++;
    if (mock_fail_write) { mock_fail_write = 0; return false; }
    if (addr + len > MOCK_SIZE) return false;
    memcpy(&mock_mem[addr], data, len);
    return true;
}

static bool mock_read(void *ctx, uint32_t addr, uint8_t *buf, uint32_t len)
{
    (void)ctx;
    mock_read_count++;
    if (mock_fail_read) { mock_fail_read = 0; return false; }
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
    RUN(test_init_err_params_size_too_small);
    RUN(test_boot_event_rejects_fw_size_past_slot);
    RUN(test_boot_event_rejects_zero_fw_size);
TEST_MAIN_END()

