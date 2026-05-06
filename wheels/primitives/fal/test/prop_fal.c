/*
 * prop_fal.c — property-based tests for fal (Flash Abstraction Layer)
 *
 * Requires: property_test.h from tooling/test/
 * Build:
 *   gcc -std=c99 -I. -I.. -I../../../../tooling/test \
 *       prop_fal.c ../fal.c -o prop_fal
 *
 * Note: must compile from test/ directory so fal_cfg.h is found.
 */

#include "test_harness.h"
#include "property_test.h"
#include <fal.h>
#include <string.h>
#include <stdlib.h>

/* --- mock flash device (same as test_fal.c) ----------------------------- */

#define MOCK_FLASH_SIZE  (64 * 1024)
#define MOCK_BLOCK_SIZE  4096

static uint8_t mock_flash_buf[MOCK_FLASH_SIZE];

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
    .name      = "mock_flash",
    .addr      = 0,
    .len       = MOCK_FLASH_SIZE,
    .blk_size  = MOCK_BLOCK_SIZE,
    .ops       = {
        .init  = mock_init,
        .read  = mock_read,
        .write = mock_write,
        .erase = mock_erase,
    },
    .write_gran = 1,
};

static int g_fal_inited = 0;

static void ensure_init(void)
{
    if (!g_fal_inited) {
        memset(mock_flash_buf, 0xFF, sizeof(mock_flash_buf));
        fal_init();
        g_fal_inited = 1;
    }
}

/* --- properties --------------------------------------------------------- */

/* Property: write-read roundtrip on random offset and data */
PROPERTY(write_read_roundtrip, 50000) {
    ensure_init();
    const struct fal_partition *p = fal_partition_find("kvdb");
    PROP_ASSERT(p != NULL);

    fal_partition_erase(p, 0, p->len);

    uint32_t max_off = (uint32_t)(p->len > 256 ? p->len - 256 : 0);
    uint32_t offset = ARBI_UINT32(seed) % (max_off + 1);
    unsigned len = (unsigned)ARBI_RANGE(seed, 1, 128);
    if (offset + len > p->len) len = (unsigned)(p->len - offset);

    uint8_t wbuf[128];
    ARBI_FILL(seed, wbuf, len);

    int wret = fal_partition_write(p, offset, wbuf, len);
    PROP_ASSERT_EQ(wret, (int)len);

    uint8_t rbuf[128];
    int rret = fal_partition_read(p, offset, rbuf, len);
    PROP_ASSERT_EQ(rret, (int)len);
    PROP_ASSERT_MEM_EQ(rbuf, wbuf, len);
}

/* Property: determinism — same operations produce same flash content */
PROPERTY(deterministic_flash, 5000) {
    ensure_init();
    uint8_t snap1[256], snap2[256];

    for (int pass = 0; pass < 2; pass++) {
        const struct fal_partition *p = fal_partition_find("config");
        PROP_ASSERT(p != NULL);
        fal_partition_erase(p, 0, p->len);

        uint32_t local_seed = seed;
        unsigned ops = (unsigned)ARBI_RANGE(local_seed, 1, 10);
        for (unsigned i = 0; i < ops; i++) {
            uint8_t data[16];
            unsigned dlen = (unsigned)ARBI_RANGE(local_seed, 1, 16);
            ARBI_FILL(local_seed, data, dlen);
            uint32_t off = ARBI_UINT32(local_seed) % (uint32_t)(p->len - dlen);
            fal_partition_write(p, off, data, dlen);
        }

        fal_partition_read(p, 0, pass == 0 ? snap1 : snap2, 256);
    }
    PROP_ASSERT_MEM_EQ(snap1, snap2, 256);
}

/* Property: erase sets all bytes to 0xFF */
PROPERTY(erase_fills_ff, 5000) {
    ensure_init();
    const struct fal_partition *p = fal_partition_find("config");
    PROP_ASSERT(p != NULL);

    /* Write random data */
    uint8_t data[32];
    ARBI_FILL(seed, data, 32);
    fal_partition_write(p, 0, data, 32);

    /* Erase */
    fal_partition_erase(p, 0, MOCK_BLOCK_SIZE);

    /* Verify all 0xFF */
    uint8_t rbuf[32];
    fal_partition_read(p, 0, rbuf, 32);
    for (int i = 0; i < 32; i++) {
        PROP_ASSERT_EQ(rbuf[i], 0xFF);
    }
}

/* Property: out-of-bounds read/write returns -1 */
PROPERTY(oob_returns_error, 50000) {
    (void)seed;  /* property uses fixed offsets; seed unused */
    ensure_init();
    const struct fal_partition *p = fal_partition_find("config");
    PROP_ASSERT(p != NULL);

    uint8_t buf[8];
    uint32_t bad_off = (uint32_t)(p->len - 2u);
    int ret = fal_partition_read(p, bad_off, buf, 8);
    PROP_ASSERT_EQ(ret, -1);

    ret = fal_partition_write(p, (uint32_t)p->len, buf, 1);
    PROP_ASSERT_EQ(ret, -1);
}

/* Property: NOR flash write semantics — can only clear bits */
PROPERTY(nor_write_semantics, 5000) {
    ensure_init();
    const struct fal_partition *p = fal_partition_find("config");
    PROP_ASSERT(p != NULL);

    fal_partition_erase(p, 0, MOCK_BLOCK_SIZE);

    uint8_t w1 = ARBI_UINT8(seed);
    uint8_t w2 = ARBI_UINT8(seed);
    fal_partition_write(p, 0, &w1, 1);
    fal_partition_write(p, 0, &w2, 1);

    uint8_t result;
    fal_partition_read(p, 0, &result, 1);
    PROP_ASSERT_EQ(result, (uint8_t)(w1 & w2));
}

TEST_MAIN("fal properties") {
    RUN(write_read_roundtrip);
    RUN(deterministic_flash);
    RUN(erase_fills_ff);
    RUN(oob_returns_error);
    RUN(nor_write_semantics);
}
TEST_MAIN_END()
