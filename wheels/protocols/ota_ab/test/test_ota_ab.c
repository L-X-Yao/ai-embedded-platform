#include <stdio.h>
#include <string.h>
#include "../ota_ab.h"

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

/* ---------- test harness ---------- */

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)

/* ---------- tests ---------- */

static void test_init_fresh(void)
{
    ota_ab_t m;

    printf("test_init_fresh\n");
    mock_reset();
    ota_ab_err_t e = ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK, "init ok on fresh flash");
    ASSERT_EQ(ota_ab_active_slot(&m), 0, "default slot A");
    ASSERT_EQ(ota_ab_target_slot(&m), 1, "target slot B");
}

static void test_init_with_saved_params(void)
{
    ota_ab_t m;

    printf("test_init_with_saved_params\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    m.params.active_slot = 1;
    m.params.ota_flag = OTA_AB_FLAG_IDLE;
    m.params.crc32 = ota_ab_crc32((const uint8_t *)&m.params,
                                    sizeof(m.params) - sizeof(uint32_t));
    mock_write(NULL, test_layout.params_addr, (uint8_t *)&m.params, sizeof(m.params));

    ota_ab_t m2;
    ota_ab_err_t e = ota_ab_init(&m2, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK, "init ok with saved");
    ASSERT_EQ(ota_ab_active_slot(&m2), 1, "restored slot B");
}

static void test_mark_pending(void)
{
    ota_ab_t m;

    printf("test_mark_pending\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ota_ab_err_t e = ota_ab_mark_pending(&m, 1024, 0xDEADBEEF);
    ASSERT_EQ(e, OTA_AB_OK, "mark pending ok");
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_PENDING, "flag=PENDING");
    ASSERT_EQ(m.params.fw_size, 1024, "fw_size saved");
    ASSERT_EQ(m.params.fw_crc, 0xDEADBEEF, "fw_crc saved");
}

static void test_mark_pending_size_zero(void)
{
    ota_ab_t m;

    printf("test_mark_pending_size_zero\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_mark_pending(&m, 0, 0), OTA_AB_ERR_SIZE, "size 0 rejected");
}

static void test_mark_pending_size_too_large(void)
{
    ota_ab_t m;

    printf("test_mark_pending_size_too_large\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_mark_pending(&m, 0x2000, 0), OTA_AB_ERR_SIZE, "oversized rejected");
}

static void test_boot_event_pending_switches_slot(void)
{
    ota_ab_t m;

    printf("test_boot_event_pending_switches_slot\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_active_slot(&m), 0, "starts at A");

    ota_ab_mark_pending(&m, 512, 0x1234);
    ota_ab_err_t e = ota_ab_boot_event(&m);
    ASSERT_EQ(e, OTA_AB_OK, "boot event ok");
    ASSERT_EQ(ota_ab_active_slot(&m), 1, "switched to B");
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_SUCCESS, "flag=SUCCESS");
    ASSERT_EQ(m.params.boot_count, 1, "boot_count=1");
}

static void test_confirm_clears_flag(void)
{
    ota_ab_t m;

    printf("test_confirm_clears_flag\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ota_ab_mark_pending(&m, 512, 0);
    ota_ab_boot_event(&m);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_SUCCESS, "in trial mode");

    ota_ab_err_t e = ota_ab_confirm(&m);
    ASSERT_EQ(e, OTA_AB_OK, "confirm ok");
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE, "flag=IDLE");
    ASSERT_EQ(m.params.boot_count, 0, "boot_count=0");
    ASSERT_EQ(ota_ab_active_slot(&m), 1, "stays on B");
}

static void test_rollback_after_max_boots(void)
{
    ota_ab_t m;

    printf("test_rollback_after_max_boots\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ota_ab_mark_pending(&m, 512, 0);

    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1, "switched to B");

    ota_ab_boot_event(&m);
    ASSERT_EQ(m.params.boot_count, 2, "boot 2");
    ota_ab_boot_event(&m);
    ASSERT_EQ(m.params.boot_count, 3, "boot 3");

    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 0, "rolled back to A");
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE, "flag=IDLE after rollback");
}

static void test_idle_boot_event_noop(void)
{
    ota_ab_t m;

    printf("test_idle_boot_event_noop\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    int writes_before = mock_write_count;

    ota_ab_err_t e = ota_ab_boot_event(&m);
    ASSERT_EQ(e, OTA_AB_OK, "idle boot ok");
    ASSERT_EQ(mock_write_count, writes_before, "no flash write on idle boot");
}

static void test_slot_addr(void)
{
    ota_ab_t m;

    printf("test_slot_addr\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_slot_addr(&m, 0), 0x0000, "slot A addr");
    ASSERT_EQ(ota_ab_slot_addr(&m, 1), 0x1000, "slot B addr");
}

static void test_crc32(void)
{
    printf("test_crc32\n");
    uint8_t data[] = "123456789";
    uint32_t crc = ota_ab_crc32(data, 9);
    ASSERT_EQ(crc, 0xCBF43926u, "CRC32 test vector");
}

static void test_null_safety(void)
{
    ota_ab_t m;

    printf("test_null_safety\n");
    ASSERT_EQ(ota_ab_init(NULL, &mock_flash, &test_layout), OTA_AB_ERR_NULL, "init null m");
    ASSERT_EQ(ota_ab_init(&m, NULL, &test_layout), OTA_AB_ERR_NULL, "init null flash");
    ASSERT_EQ(ota_ab_init(&m, &mock_flash, NULL), OTA_AB_ERR_NULL, "init null layout");
    ASSERT_EQ(ota_ab_active_slot(NULL), 0, "active null");
    ASSERT_EQ(ota_ab_target_slot(NULL), 1, "target null");
    ASSERT_EQ(ota_ab_slot_addr(NULL, 0), 0, "addr null");
    ASSERT_EQ(ota_ab_mark_pending(NULL, 100, 0), OTA_AB_ERR_NULL, "pending null");
    ASSERT_EQ(ota_ab_boot_event(NULL), OTA_AB_ERR_NULL, "boot null");
    ASSERT_EQ(ota_ab_confirm(NULL), OTA_AB_ERR_NULL, "confirm null");
}

static void test_flash_fail_on_write(void)
{
    ota_ab_t m;

    printf("test_flash_fail_on_write\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    mock_fail_write = 1;
    ASSERT_EQ(ota_ab_mark_pending(&m, 512, 0), OTA_AB_ERR_FLASH, "write fail");
}

static void test_full_ota_cycle(void)
{
    ota_ab_t m;

    printf("test_full_ota_cycle\n");
    mock_reset();

    ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(ota_ab_active_slot(&m), 0, "start A");

    ota_ab_mark_pending(&m, 512, 0xABCD);
    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1, "trial B");

    ota_ab_confirm(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1, "confirmed B");
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE, "idle after confirm");

    ota_ab_mark_pending(&m, 256, 0x1111);
    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 0, "trial A");

    ota_ab_boot_event(&m);
    ota_ab_boot_event(&m);
    ota_ab_boot_event(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1, "rolled back to B");
}

static void test_double_confirm_noop(void)
{
    ota_ab_t m;

    printf("test_double_confirm_noop\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ota_ab_mark_pending(&m, 512, 0);
    ota_ab_boot_event(&m);
    ota_ab_confirm(&m);
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE, "idle after first confirm");

    int writes_before = mock_write_count;
    ota_ab_err_t e = ota_ab_confirm(&m);
    ASSERT_EQ(e, OTA_AB_OK, "double confirm ok");
    ASSERT_EQ(mock_write_count, writes_before, "no flash write on idle confirm");
}

static void test_flash_read_fail_on_init(void)
{
    ota_ab_t m;

    printf("test_flash_read_fail_on_init\n");
    mock_reset();
    mock_fail_read = 1;
    ota_ab_err_t e = ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_ERR_FLASH, "read fail on init");
}

static void test_flash_erase_fail_on_pending(void)
{
    ota_ab_t m;

    printf("test_flash_erase_fail_on_pending\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    mock_fail_erase = 1;
    ota_ab_err_t e = ota_ab_mark_pending(&m, 512, 0);
    ASSERT_EQ(e, OTA_AB_ERR_FLASH, "erase fail on pending");
}

static void test_corrupted_params_recovery(void)
{
    ota_ab_t m;

    printf("test_corrupted_params_recovery\n");
    mock_reset();

    ota_ab_params_t bad;
    memset(&bad, 0, sizeof(bad));
    bad.magic = OTA_AB_PARAMS_MAGIC;
    bad.active_slot = 1;
    bad.crc32 = 0xDEADDEAD;
    mock_write(NULL, test_layout.params_addr, (uint8_t *)&bad, sizeof(bad));

    ota_ab_err_t e = ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK, "init ok despite bad CRC");
    ASSERT_EQ(ota_ab_active_slot(&m), 0, "reset to default slot A");
    ASSERT_EQ(m.params.ota_flag, OTA_AB_FLAG_IDLE, "reset to idle");
}

static void test_reinit_persists_state(void)
{
    ota_ab_t m;

    printf("test_reinit_persists_state\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ota_ab_mark_pending(&m, 512, 0xABCD);
    ota_ab_boot_event(&m);
    ota_ab_confirm(&m);
    ASSERT_EQ(ota_ab_active_slot(&m), 1, "on slot B");

    ota_ab_t m2;
    ota_ab_err_t e = ota_ab_init(&m2, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK, "re-init ok");
    ASSERT_EQ(ota_ab_active_slot(&m2), 1, "persisted slot B");
    ASSERT_EQ(m2.params.ota_flag, OTA_AB_FLAG_IDLE, "persisted idle");
    ASSERT_EQ(m2.params.boot_count, 0, "persisted boot_count=0");
}

static void test_mark_pending_overwrite(void)
{
    ota_ab_t m;

    printf("test_mark_pending_overwrite\n");
    mock_reset();
    ota_ab_init(&m, &mock_flash, &test_layout);
    ota_ab_mark_pending(&m, 512, 0x1111);
    ASSERT_EQ(m.params.fw_crc, 0x1111, "first pending");

    ota_ab_err_t e = ota_ab_mark_pending(&m, 256, 0x2222);
    ASSERT_EQ(e, OTA_AB_OK, "overwrite ok");
    ASSERT_EQ(m.params.fw_size, 256, "overwritten size");
    ASSERT_EQ(m.params.fw_crc, 0x2222, "overwritten crc");
}

static void test_invalid_active_slot_recovery(void)
{
    ota_ab_t m;

    printf("test_invalid_active_slot_recovery\n");
    mock_reset();

    ota_ab_params_t bad;
    memset(&bad, 0, sizeof(bad));
    bad.magic = OTA_AB_PARAMS_MAGIC;
    bad.active_slot = 5;
    bad.crc32 = ota_ab_crc32((const uint8_t *)&bad,
                              sizeof(bad) - sizeof(uint32_t));
    mock_write(NULL, test_layout.params_addr, (uint8_t *)&bad, sizeof(bad));

    ota_ab_err_t e = ota_ab_init(&m, &mock_flash, &test_layout);
    ASSERT_EQ(e, OTA_AB_OK, "init ok despite invalid slot");
    ASSERT_EQ(ota_ab_active_slot(&m), 0, "reset to slot A");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_init_fresh();
    test_init_with_saved_params();
    test_mark_pending();
    test_mark_pending_size_zero();
    test_mark_pending_size_too_large();
    test_boot_event_pending_switches_slot();
    test_confirm_clears_flag();
    test_rollback_after_max_boots();
    test_idle_boot_event_noop();
    test_slot_addr();
    test_crc32();
    test_null_safety();
    test_flash_fail_on_write();
    test_full_ota_cycle();
    test_double_confirm_noop();
    test_flash_read_fail_on_init();
    test_flash_erase_fail_on_pending();
    test_corrupted_params_recovery();
    test_reinit_persists_state();
    test_mark_pending_overwrite();
    test_invalid_active_slot_recovery();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
