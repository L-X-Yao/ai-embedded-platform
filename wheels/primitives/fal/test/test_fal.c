#include <stdio.h>
#include <string.h>
#include <fal.h>

/* ---------- test harness ---------- */

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

/* ---------- mock flash device ---------- */

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
    if (offset < 0 || (size_t)(offset + size) > MOCK_FLASH_SIZE) return -1;
    memcpy(buf, mock_flash_buf + offset, size);
    return (int)size;
}

static int mock_write(long offset, const uint8_t *buf, size_t size)
{
    if (offset < 0 || (size_t)(offset + size) > MOCK_FLASH_SIZE) return -1;
    for (size_t i = 0; i < size; i++)
        mock_flash_buf[offset + i] &= buf[i];
    return (int)size;
}

static int mock_erase(long offset, size_t size)
{
    if (offset < 0 || (size_t)(offset + size) > MOCK_FLASH_SIZE) return -1;
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

/* ---------- tests ---------- */

static void test_init(void)
{
    printf("test_init\n");
    int n = fal_init();
    ASSERT(n == 3, "3 partitions");
    ASSERT(fal_init_check() == 1, "init ok");
}

static void test_find_partition(void)
{
    printf("test_find_partition\n");
    const struct fal_partition *p;

    p = fal_partition_find("config");
    ASSERT(p != NULL, "config found");
    ASSERT(p->len == 4096, "config len");
    ASSERT(p->offset == 0, "config offset");

    p = fal_partition_find("kvdb");
    ASSERT(p != NULL, "kvdb found");
    ASSERT(p->len == 32768, "kvdb len");

    p = fal_partition_find("firmware");
    ASSERT(p != NULL, "firmware found");
    ASSERT(p->offset == 4096 + 32768, "firmware offset");
}

static void test_find_not_exist(void)
{
    printf("test_find_not_exist\n");
    const struct fal_partition *p = fal_partition_find("nonexistent");
    ASSERT(p == NULL, "not found");
}

static void test_find_flash_device(void)
{
    printf("test_find_flash_device\n");
    const struct fal_flash_dev *d = fal_flash_device_find("mock_flash");
    ASSERT(d != NULL, "mock_flash found");
    ASSERT(d->len == MOCK_FLASH_SIZE, "device len");

    d = fal_flash_device_find("no_such_dev");
    ASSERT(d == NULL, "unknown device");
}

static void test_get_partition_table(void)
{
    printf("test_get_partition_table\n");
    size_t len = 0;
    const struct fal_partition *t = fal_get_partition_table(&len);
    ASSERT(t != NULL, "table not null");
    ASSERT(len == 3, "3 entries");
}

static void test_write_read(void)
{
    printf("test_write_read\n");
    const struct fal_partition *p = fal_partition_find("config");

    fal_partition_erase(p, 0, p->len);

    uint8_t wbuf[] = {0xDE, 0xAD, 0xBE, 0xEF};
    int ret = fal_partition_write(p, 0, wbuf, 4);
    ASSERT(ret == 4, "write 4 bytes");

    uint8_t rbuf[4] = {0};
    ret = fal_partition_read(p, 0, rbuf, 4);
    ASSERT(ret == 4, "read 4 bytes");
    ASSERT(memcmp(rbuf, wbuf, 4) == 0, "data matches");
}

static void test_write_read_offset(void)
{
    printf("test_write_read_offset\n");
    const struct fal_partition *p = fal_partition_find("kvdb");

    fal_partition_erase(p, 0, p->len);

    uint8_t data[] = {0x01, 0x02, 0x03};
    fal_partition_write(p, 100, data, 3);

    uint8_t rbuf[3] = {0};
    fal_partition_read(p, 100, rbuf, 3);
    ASSERT(rbuf[0] == 0x01 && rbuf[1] == 0x02 && rbuf[2] == 0x03,
           "offset read matches");
}

static void test_erase(void)
{
    printf("test_erase\n");
    const struct fal_partition *p = fal_partition_find("config");

    uint8_t data[] = {0x55, 0xAA};
    fal_partition_write(p, 0, data, 2);

    fal_partition_erase(p, 0, MOCK_BLOCK_SIZE);

    uint8_t rbuf[2];
    fal_partition_read(p, 0, rbuf, 2);
    ASSERT(rbuf[0] == 0xFF && rbuf[1] == 0xFF, "erased to 0xFF");
}

static void test_erase_all(void)
{
    printf("test_erase_all\n");
    const struct fal_partition *p = fal_partition_find("kvdb");

    uint8_t data = 0x42;
    fal_partition_write(p, 0, &data, 1);
    fal_partition_write(p, p->len - 1, &data, 1);

    fal_partition_erase_all(p);

    uint8_t r1, r2;
    fal_partition_read(p, 0, &r1, 1);
    fal_partition_read(p, p->len - 1, &r2, 1);
    ASSERT(r1 == 0xFF && r2 == 0xFF, "all erased");
}

static void test_out_of_bounds(void)
{
    printf("test_out_of_bounds\n");
    const struct fal_partition *p = fal_partition_find("config");
    uint8_t buf[8];

    int ret = fal_partition_read(p, p->len - 2, buf, 8);
    ASSERT(ret == -1, "read out of bounds");

    ret = fal_partition_write(p, p->len, buf, 1);
    ASSERT(ret == -1, "write out of bounds");

    ret = fal_partition_erase(p, p->len - 10, 20);
    ASSERT(ret == -1, "erase out of bounds");
}

static void test_partition_isolation(void)
{
    printf("test_partition_isolation\n");
    const struct fal_partition *config = fal_partition_find("config");
    const struct fal_partition *kvdb = fal_partition_find("kvdb");

    fal_partition_erase_all(config);
    fal_partition_erase_all(kvdb);

    uint8_t data = 0xAB;
    fal_partition_write(config, config->len - 1, &data, 1);

    uint8_t rbuf;
    fal_partition_read(kvdb, 0, &rbuf, 1);
    ASSERT(rbuf == 0xFF, "kvdb unaffected by config write");
}

static void test_flash_write_semantics(void)
{
    printf("test_flash_write_semantics\n");
    const struct fal_partition *p = fal_partition_find("config");

    fal_partition_erase(p, 0, MOCK_BLOCK_SIZE);

    uint8_t w1 = 0xF0;
    fal_partition_write(p, 0, &w1, 1);

    uint8_t w2 = 0x0F;
    fal_partition_write(p, 0, &w2, 1);

    uint8_t r;
    fal_partition_read(p, 0, &r, 1);
    ASSERT(r == 0x00, "flash write is AND (bits only clear)");
}

static void test_show_part_table(void)
{
    printf("test_show_part_table\n");
    fal_show_part_table();
    ASSERT(1, "show_part_table no crash");
}

/* ---------- main ---------- */

int main(void)
{
    tests_run = tests_fail = 0;

    test_init();
    test_find_partition();
    test_find_not_exist();
    test_find_flash_device();
    test_get_partition_table();
    test_write_read();
    test_write_read_offset();
    test_erase();
    test_erase_all();
    test_out_of_bounds();
    test_partition_isolation();
    test_flash_write_semantics();
    test_show_part_table();

    printf("\n%d tests, %d failures\n", tests_run, tests_fail);
    return tests_fail ? 1 : 0;
}
