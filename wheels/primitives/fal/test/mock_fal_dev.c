// SPDX-License-Identifier: MIT
/*
 * mock_fal_dev.c — shared RAM-backed flash mock for fal unit tests
 *
 * Provides mock_flash_dev (declared in fal_cfg.h) and the underlying
 * mock_flash_buf storage.  Linked by test_fal, test_null_fal, and prop_fal.
 */

#include <fal.h>
#include <string.h>

#define MOCK_FLASH_SIZE  (64u * 1024u)
#define MOCK_BLOCK_SIZE  4096u

uint8_t mock_flash_buf[MOCK_FLASH_SIZE];

static int mock_init(void)
{
    memset(mock_flash_buf, 0xFF, sizeof(mock_flash_buf));
    return 0;
}

static int mock_read(long offset, uint8_t *buf, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    memcpy(buf, mock_flash_buf + offset, size);
    return (int)size;
}

static int mock_write(long offset, const uint8_t *buf, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    for (size_t i = 0; i < size; i++)
        mock_flash_buf[(size_t)offset + i] &= buf[i];
    return (int)size;
}

static int mock_erase(long offset, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) return -1;
    memset(mock_flash_buf + offset, 0xFF, size);
    return (int)size;
}

const struct fal_flash_dev mock_flash_dev = {
    .name       = "mock_flash",
    .addr       = 0,
    .len        = MOCK_FLASH_SIZE,
    .blk_size   = MOCK_BLOCK_SIZE,
    .ops        = {
        .init   = mock_init,
        .read   = mock_read,
        .write  = mock_write,
        .erase  = mock_erase,
    },
    .write_gran = 1,
};
