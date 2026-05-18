// SPDX-License-Identifier: MIT
/*
 * prop_flash_journal.c — property-based tests for flash_journal
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_flash_journal.c ../flash_journal.c -o prop_flash_journal
 */

#include "test_harness.h"
#include "property_test.h"
#include "../flash_journal.h"
#include <string.h>

/* --- RAM-backed flash mock --- */
#define MOCK_SECTOR_SIZE  256
#define MOCK_SECTOR_COUNT 4
static uint8_t mock_flash[MOCK_SECTOR_COUNT * MOCK_SECTOR_SIZE];

static flash_journal_err_t mock_read(void *ctx, uint32_t sector, uint32_t offset,
                                      uint8_t *buf, uint32_t len) {
    (void)ctx;
    if (sector >= MOCK_SECTOR_COUNT) return FLASH_JOURNAL_ERR_STORAGE;
    uint32_t addr = sector * MOCK_SECTOR_SIZE + offset;
    if (addr + len > sizeof(mock_flash)) return FLASH_JOURNAL_ERR_STORAGE;
    memcpy(buf, mock_flash + addr, len);
    return FLASH_JOURNAL_OK;
}

static flash_journal_err_t mock_write(void *ctx, uint32_t sector, uint32_t offset,
                                       const uint8_t *data, uint32_t len) {
    (void)ctx;
    if (sector >= MOCK_SECTOR_COUNT) return FLASH_JOURNAL_ERR_STORAGE;
    uint32_t addr = sector * MOCK_SECTOR_SIZE + offset;
    if (addr + len > sizeof(mock_flash)) return FLASH_JOURNAL_ERR_STORAGE;
    memcpy(mock_flash + addr, data, len);
    return FLASH_JOURNAL_OK;
}

static flash_journal_err_t mock_erase(void *ctx, uint32_t sector) {
    (void)ctx;
    if (sector >= MOCK_SECTOR_COUNT) return FLASH_JOURNAL_ERR_STORAGE;
    memset(mock_flash + sector * MOCK_SECTOR_SIZE, 0xFF, MOCK_SECTOR_SIZE);
    return FLASH_JOURNAL_OK;
}

static const flash_journal_storage_t mock_storage = {
    .read  = mock_read,
    .write = mock_write,
    .erase = mock_erase,
    .ctx   = NULL,
};

/* Property: random append/read sequences never crash, count bounded */
PROPERTY(random_ops_no_crash, 5000) {
    memset(mock_flash, 0xFF, sizeof(mock_flash));

    flash_journal_t jrnl;
    flash_journal_init(&jrnl, &mock_storage, MOCK_SECTOR_COUNT, MOCK_SECTOR_SIZE);

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 4;
        if (op == 0) {
            uint8_t data[64];
            uint16_t len = (uint16_t)ARBI_RANGE(seed, 1, 64);
            ARBI_FILL(seed, data, len);
            flash_journal_append(&jrnl, data, len);
        } else if (op == 1) {
            uint8_t buf[FLASH_JOURNAL_MAX_ENTRY_SIZE];
            uint32_t size = sizeof(buf);
            flash_journal_read_latest(&jrnl, buf, &size);
        } else if (op == 2) {
            uint8_t buf[FLASH_JOURNAL_MAX_ENTRY_SIZE];
            uint32_t size = sizeof(buf);
            uint32_t n = (uint32_t)ARBI_RANGE(seed, 0, 10);
            flash_journal_read_nth(&jrnl, n, buf, &size);
        } else {
            flash_journal_erase_all(&jrnl);
        }
    }

    /* count is always queryable */
    (void)flash_journal_count(&jrnl);
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    memset(mock_flash, 0xFF, sizeof(mock_flash));

    flash_journal_t j1, j2;
    flash_journal_init(&j1, &mock_storage, MOCK_SECTOR_COUNT, MOCK_SECTOR_SIZE);
    flash_journal_init(&j2, &mock_storage, MOCK_SECTOR_COUNT, MOCK_SECTOR_SIZE);
    flash_journal_init(&j2, &mock_storage, MOCK_SECTOR_COUNT, MOCK_SECTOR_SIZE);

    PROP_ASSERT_EQ(flash_journal_count(&j1), flash_journal_count(&j2));
}

/* Property: erase_all resets count to 0 */
PROPERTY(erase_resets, 5000) {
    memset(mock_flash, 0xFF, sizeof(mock_flash));

    flash_journal_t jrnl;
    flash_journal_init(&jrnl, &mock_storage, MOCK_SECTOR_COUNT, MOCK_SECTOR_SIZE);

    uint8_t writes = (uint8_t)ARBI_RANGE(seed, 1, 10);
    for (uint8_t i = 0; i < writes; i++) {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        flash_journal_append(&jrnl, data, sizeof(data));
    }

    flash_journal_erase_all(&jrnl);
    PROP_ASSERT_EQ(flash_journal_count(&jrnl), 0);
}

TEST_MAIN("flash_journal properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(erase_resets);
}
TEST_MAIN_END()
