// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "boot_counter.h"
#include <string.h>

/* ================================================================
 * Mock storage backend
 * ================================================================ */

static uint8_t g_storage[sizeof(boot_counter_record_t)];
static bool    g_fail_read;
static bool    g_fail_write;
static int     g_write_count;

static void mock_reset(void)
{
    memset(g_storage, 0, sizeof(g_storage));
    g_fail_read  = false;
    g_fail_write = false;
    g_write_count = 0;
}

static int mock_read(uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (g_fail_read) { return -1; }
    if (size > sizeof(g_storage)) { return -1; }
    memcpy(buf, g_storage, size);
    return 0;
}

static int mock_write(const uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (g_fail_write) { return -1; }
    if (size > sizeof(g_storage)) { return -1; }
    memcpy(g_storage, buf, size);
    g_write_count++;
    return 0;
}

static boot_counter_storage_t make_storage(void)
{
    boot_counter_storage_t s;
    s.read     = mock_read;
    s.write    = mock_write;
    s.user_ctx = NULL;
    return s;
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();

    boot_counter_err_t err = boot_counter_init(&ctx, &stor);
    ASSERT_EQ(err, BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 0);
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx), 0);
    ASSERT_EQ(boot_counter_get_uptime_seconds(&ctx), 0);
}

TEST(test_init_null_ctx)
{
    boot_counter_storage_t stor = make_storage();
    ASSERT_EQ(boot_counter_init(NULL, &stor), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_init_null_storage)
{
    boot_counter_t ctx;
    ASSERT_EQ(boot_counter_init(&ctx, NULL), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_init_null_read_cb)
{
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    stor.read = NULL;
    ASSERT_EQ(boot_counter_init(&ctx, &stor), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_init_null_write_cb)
{
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    stor.write = NULL;
    ASSERT_EQ(boot_counter_init(&ctx, &stor), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_load_save_roundtrip)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);

    /* Increment to set count=1, then save to mock storage */
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 1);

    /* Create a fresh context and load from same storage */
    boot_counter_t ctx2;
    boot_counter_init(&ctx2, &stor);
    ASSERT_EQ(boot_counter_load(&ctx2), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx2), 1);
}

TEST(test_increment)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);

    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 1);
}

TEST(test_multiple_increments)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);

    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    }
    ASSERT_EQ(boot_counter_get_count(&ctx), 10);

    /* Verify persistence: reload from storage */
    boot_counter_t ctx2;
    boot_counter_init(&ctx2, &stor);
    boot_counter_load(&ctx2);
    ASSERT_EQ(boot_counter_get_count(&ctx2), 10);
}

TEST(test_reset_reason)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);

    ASSERT_EQ(boot_counter_set_reset_reason(&ctx, 0x04), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx), 0x04);

    /* Verify persistence */
    boot_counter_t ctx2;
    boot_counter_init(&ctx2, &stor);
    boot_counter_load(&ctx2);
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx2), 0x04);
}

/* A timer writes this field and any context may read it. Without volatile,
 * -flto hoists the read out of a wait loop, and a unit test cannot show a
 * hang, so pin the type: the pointer type keeps the qualifier to compare. */
TEST(test_uptime_field_is_volatile)
{
    ASSERT_TRUE(__builtin_types_compatible_p(
        __typeof__(&((boot_counter_t *)0)->uptime_seconds), volatile uint32_t *));
}

TEST(test_uptime_ticks)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    ASSERT_EQ(boot_counter_get_uptime_seconds(&ctx), 0);

    boot_counter_tick_second(&ctx);
    boot_counter_tick_second(&ctx);
    boot_counter_tick_second(&ctx);
    ASSERT_EQ(boot_counter_get_uptime_seconds(&ctx), 3);

    for (int i = 0; i < 100; i++) {
        boot_counter_tick_second(&ctx);
    }
    ASSERT_EQ(boot_counter_get_uptime_seconds(&ctx), 103);
}

TEST(test_storage_read_failure)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    g_fail_read = true;
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_ERR_STORAGE);
    /* After failed load, count should be zero (safe default) */
    ASSERT_EQ(boot_counter_get_count(&ctx), 0);
}

TEST(test_storage_write_failure)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);

    g_fail_write = true;
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_ERR_STORAGE);
    /* In-memory count still incremented (write failed after bump) */
    ASSERT_EQ(boot_counter_get_count(&ctx), 1);
}

TEST(test_set_reset_reason_write_failure)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);

    g_fail_write = true;
    ASSERT_EQ(boot_counter_set_reset_reason(&ctx, 7), BOOT_COUNTER_ERR_STORAGE);
    /* In-memory reason still set */
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx), 7);
}

TEST(test_null_ctx_getters)
{
    ASSERT_EQ(boot_counter_get_count(NULL), 0);
    ASSERT_EQ(boot_counter_get_reset_reason(NULL), 0);
    ASSERT_EQ(boot_counter_get_uptime_seconds(NULL), 0);
}

TEST(test_null_ctx_mutators)
{
    ASSERT_EQ(boot_counter_load(NULL), BOOT_COUNTER_ERR_PARAM);
    ASSERT_EQ(boot_counter_increment(NULL), BOOT_COUNTER_ERR_PARAM);
    ASSERT_EQ(boot_counter_set_reset_reason(NULL, 1), BOOT_COUNTER_ERR_PARAM);
    /* tick_second with NULL should not crash */
    boot_counter_tick_second(NULL);
    ASSERT_TRUE(1);
}

TEST(test_increment_persists_each_call)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);

    g_write_count = 0;
    boot_counter_increment(&ctx);
    boot_counter_increment(&ctx);
    boot_counter_increment(&ctx);
    /* Each increment should trigger a write */
    ASSERT_EQ(g_write_count, 3);
}

/* ================================================================ */

TEST(test_increment_without_load)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    /* Must call load() before increment/set_reset_reason */
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_ERR_PARAM);
    ASSERT_EQ(boot_counter_set_reset_reason(&ctx, 1), BOOT_COUNTER_ERR_PARAM);
}

/* Erased flash reads as 0xFF: that is no record, not a count of UINT32_MAX. */
TEST(test_erased_storage_starts_from_zero)
{
    mock_reset();
    memset(g_storage, 0xFF, sizeof(g_storage));
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 0);
    ASSERT_EQ(boot_counter_get_reset_reason(&ctx), 0);

    /* The counter moves from there, and the next boot sees it. */
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 1);

    boot_counter_t next;
    boot_counter_init(&next, &stor);
    ASSERT_EQ(boot_counter_load(&next), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&next), 1);
}

/* A backend that erased and lost power before programming, at count 42. */
TEST(test_erase_then_power_loss_is_not_a_count)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);
    for (int i = 0; i < 42; i++) boot_counter_increment(&ctx);

    memset(g_storage, 0xFF, sizeof(g_storage));

    boot_counter_t next;
    boot_counter_init(&next, &stor);
    ASSERT_EQ(boot_counter_load(&next), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&next), 0);
}

/* One flipped bit anywhere in the record: the whole record is untrustworthy. */
TEST(test_damaged_record_starts_from_zero)
{
    for (size_t byte = 0; byte < sizeof(boot_counter_record_t); byte++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            mock_reset();
            boot_counter_t ctx;
            boot_counter_storage_t stor = make_storage();
            boot_counter_init(&ctx, &stor);
            boot_counter_load(&ctx);
            boot_counter_increment(&ctx);
            boot_counter_set_reset_reason(&ctx, 7);

            g_storage[byte] ^= (uint8_t)(1u << bit);

            boot_counter_t next;
            boot_counter_init(&next, &stor);
            ASSERT_EQ(boot_counter_load(&next), BOOT_COUNTER_OK);
            ASSERT_EQ(boot_counter_get_count(&next), 0);
            ASSERT_EQ(boot_counter_get_reset_reason(&next), 0);
        }
    }
}

/* ================================================================
 * Two copies, on a backend that erases and then programs
 * ================================================================ */

#define SECTOR_SIZE sizeof(boot_counter_record_t)

static uint8_t g_sector[2][SECTOR_SIZE];
static int     g_cut_at, g_ops;  /* power dies in flash op g_cut_at, from 0 */
static size_t  g_tear;           /* bytes of that op that land */
static bool    g_dead;
static int     g_fail_read2;

static void sector_reset(void)
{
    memset(g_sector, 0xFF, sizeof(g_sector));
    g_cut_at = -1;
    g_ops = 0;
    g_tear = 0;
    g_dead = false;
    g_fail_read2 = 0;
}

/* True if this op does not finish; *landed is how much of it reached flash. */
static bool power_cut(size_t len, size_t *landed)
{
    *landed = 0;
    if (g_cut_at < 0) return false;
    if (g_dead) return true;
    if (g_ops++ != g_cut_at) return false;
    *landed = g_tear < len ? g_tear : len;
    g_dead = true;
    return true;
}

static int sector_write(int s, const uint8_t *buf, size_t size)
{
    size_t landed;

    if (g_fail_write) return -1;
    if (size > SECTOR_SIZE) return -1;

    if (power_cut(SECTOR_SIZE, &landed)) {
        memset(g_sector[s], 0xFF, landed);
        return -1;
    }
    memset(g_sector[s], 0xFF, SECTOR_SIZE);

    if (power_cut(size, &landed)) {
        memcpy(g_sector[s], buf, landed);
        return -1;
    }
    memcpy(g_sector[s], buf, size);
    return 0;
}

static int sector_read(int s, uint8_t *buf, size_t size)
{
    if (size > SECTOR_SIZE) return -1;
    memcpy(buf, g_sector[s], size);
    return 0;
}

static int read_a(uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    return sector_read(0, buf, size);
}

static int write_a(const uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    return sector_write(0, buf, size);
}

static int read_b(uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (g_fail_read2) return -1;
    return sector_read(1, buf, size);
}

static int write_b(const uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    return sector_write(1, buf, size);
}

static void reboot(boot_counter_t *ctx)
{
    boot_counter_storage_t s;
    memset(&s, 0, sizeof(s));
    s.read = read_a;
    s.write = write_a;
    boot_counter_init(ctx, &s);
    boot_counter_set_second_copy(ctx, read_b, write_b);
}

/* CRC-32 (IEEE 802.3) written out here so the test does not lean on the code */
static uint32_t crc32_ref(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

static void put_record(int s, uint32_t count, uint32_t seq)
{
    boot_counter_record_t r;
    memset(&r, 0, sizeof(r));
    r.boot_count = count;
    r.magic = BOOT_COUNTER_MAGIC;
    r.seq = seq;
    r.crc = crc32_ref((const uint8_t *)&r, sizeof(r) - sizeof(r.crc));
    memcpy(g_sector[s], &r, sizeof(r));
}

static const struct { uint32_t count, reason; } run_states[] = {
    { 0, 0 },   /* erased flash */
    { 1, 0 },   /* 1: increment */
    { 1, 7 },   /* 2: reset reason */
    { 2, 7 },   /* 3: increment */
    { 3, 7 },   /* 4: increment */
    { 3, 9 },   /* 5: reset reason */
    { 4, 9 },   /* 6: increment */
};
#define RUN_STEPS 6

static void run_step(boot_counter_t *ctx, int step)
{
    switch (step) {
    case 2:  (void)boot_counter_set_reset_reason(ctx, 7); break;
    case 5:  (void)boot_counter_set_reset_reason(ctx, 9); break;
    default: (void)boot_counter_increment(ctx); break;
    }
}

static bool state_is(const boot_counter_t *ctx, int i)
{
    return boot_counter_get_count(ctx) == run_states[i].count &&
           boot_counter_get_reset_reason(ctx) == run_states[i].reason;
}

/* The run from the finding, now with a second copy: count 42 survives. */
TEST(test_cut_between_erase_and_program_keeps_the_count)
{
    sector_reset();
    g_fail_write = false;
    boot_counter_t ctx, next;
    reboot(&ctx);
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);
    for (int i = 0; i < 42; i++) boot_counter_increment(&ctx);

    g_cut_at = 1;   /* the erase lands, the program does not */
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_ERR_STORAGE);
    g_cut_at = -1;
    g_dead = false;

    reboot(&next);
    ASSERT_EQ(boot_counter_load(&next), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&next), 42);
}

/* Lose power in every erase and program of a run, at every byte: reboot finds
 * the record from before that save or after it, and the run goes on. */
TEST(test_power_cut_anywhere_keeps_the_record)
{
    g_fail_write = false;
    for (int cut = 0; cut < 2 * RUN_STEPS; cut++) {
        int step = 1 + cut / 2;
        bool is_program = (cut & 1) != 0;

        for (size_t tear = 0; tear <= SECTOR_SIZE; tear++) {
            boot_counter_t ctx, next;
            sector_reset();
            reboot(&ctx);
            ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);

            g_cut_at = cut;
            g_tear = tear;
            for (int s = 1; s <= step; s++) run_step(&ctx, s);
            g_cut_at = -1;
            g_dead = false;

            reboot(&next);
            ASSERT_EQ(boot_counter_load(&next), BOOT_COUNTER_OK);
            bool before = state_is(&next, step - 1);
            bool after = state_is(&next, step);
            ASSERT_TRUE(before || after);
            ASSERT_EQ(after, is_program && tear == SECTOR_SIZE);

            for (int s = after ? step + 1 : step; s <= RUN_STEPS; s++) {
                run_step(&next, s);
            }
            ASSERT_TRUE(state_is(&next, RUN_STEPS));

            reboot(&ctx);
            ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);
            ASSERT_TRUE(state_is(&ctx, RUN_STEPS));
        }
    }
}

TEST(test_second_copy_needs_both_callbacks_before_load)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);

    ASSERT_EQ(boot_counter_set_second_copy(NULL, read_b, write_b), BOOT_COUNTER_ERR_PARAM);
    ASSERT_EQ(boot_counter_set_second_copy(&ctx, NULL, write_b), BOOT_COUNTER_ERR_PARAM);
    ASSERT_EQ(boot_counter_set_second_copy(&ctx, read_b, NULL), BOOT_COUNTER_ERR_PARAM);

    boot_counter_load(&ctx);
    ASSERT_EQ(boot_counter_set_second_copy(&ctx, read_b, write_b), BOOT_COUNTER_ERR_PARAM);
}

TEST(test_copies_alternate)
{
    sector_reset();
    g_fail_write = false;
    boot_counter_t ctx;
    boot_counter_record_t a, b;
    reboot(&ctx);
    boot_counter_load(&ctx);

    boot_counter_increment(&ctx);
    boot_counter_increment(&ctx);
    memcpy(&a, g_sector[0], sizeof(a));
    memcpy(&b, g_sector[1], sizeof(b));
    ASSERT_EQ(b.boot_count, 1);
    ASSERT_EQ(b.seq, 1);
    ASSERT_EQ(a.boot_count, 2);
    ASSERT_EQ(a.seq, 2);
}

/* seq wraps, and "newer" is the nearer way round. */
TEST(test_newest_copy_wins)
{
    boot_counter_t ctx;

    sector_reset();
    put_record(0, 5, 10);
    put_record(1, 6, 11);
    reboot(&ctx);
    boot_counter_load(&ctx);
    ASSERT_EQ(boot_counter_get_count(&ctx), 6);

    sector_reset();
    put_record(0, 6, 11);
    put_record(1, 5, 10);
    reboot(&ctx);
    boot_counter_load(&ctx);
    ASSERT_EQ(boot_counter_get_count(&ctx), 6);

    sector_reset();
    put_record(0, 5, 0xFFFFFFFFu);
    put_record(1, 6, 0);
    reboot(&ctx);
    boot_counter_load(&ctx);
    ASSERT_EQ(boot_counter_get_count(&ctx), 6);

    sector_reset();
    put_record(0, 6, 3);
    put_record(1, 5, 0xC0000000u);
    reboot(&ctx);
    boot_counter_load(&ctx);
    ASSERT_EQ(boot_counter_get_count(&ctx), 6);
}

TEST(test_equal_seq_takes_the_first_copy)
{
    boot_counter_t ctx;
    sector_reset();
    put_record(0, 6, 4);
    put_record(1, 5, 4);
    reboot(&ctx);
    boot_counter_load(&ctx);
    ASSERT_EQ(boot_counter_get_count(&ctx), 6);
}

TEST(test_damaged_newest_copy_falls_back)
{
    boot_counter_t ctx;
    sector_reset();
    put_record(0, 5, 10);
    put_record(1, 6, 11);
    g_sector[1][0] ^= 0x01;
    reboot(&ctx);
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 5);

    /* The next save goes over the damaged copy, not the good one. */
    uint8_t good[SECTOR_SIZE];
    memcpy(good, g_sector[0], sizeof(good));
    g_fail_write = false;
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    ASSERT_MEMEQ_N(good, g_sector[0], sizeof(good));
}

/* Which copy is newest cannot be known with one of them unread. */
TEST(test_second_copy_read_failure_refuses_to_save)
{
    boot_counter_t ctx;
    sector_reset();
    put_record(0, 5, 10);
    g_fail_read2 = 1;
    reboot(&ctx);
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_ERR_STORAGE);
    ASSERT_EQ(boot_counter_get_count(&ctx), 0);
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_ERR_PARAM);
}

/* A failed write leaves the good copy alone, and the retry keeps the seq. */
TEST(test_failed_write_keeps_the_good_copy)
{
    boot_counter_t ctx, next;
    boot_counter_record_t r;
    sector_reset();
    g_fail_write = false;
    reboot(&ctx);
    boot_counter_load(&ctx);
    boot_counter_increment(&ctx);

    g_fail_write = true;
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_ERR_STORAGE);
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_ERR_STORAGE);
    g_fail_write = false;

    reboot(&next);
    boot_counter_load(&next);
    ASSERT_EQ(boot_counter_get_count(&next), 1);

    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
    memcpy(&r, g_sector[0], sizeof(r));
    ASSERT_EQ(r.seq, 2);
}

/* A load that fails after a good one must not leave a zero record to save. */
/* A good CRC on someone else's data is still not a record of ours. */
TEST(test_wrong_magic_with_good_crc_starts_from_zero)
{
    mock_reset();
    boot_counter_record_t r;
    memset(&r, 0, sizeof(r));
    r.boot_count = 9;
    r.magic = BOOT_COUNTER_MAGIC ^ 1u;
    r.crc = crc32_ref((const uint8_t *)&r, sizeof(r) - sizeof(r.crc));
    memcpy(g_storage, &r, sizeof(r));

    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 0);
}

TEST(test_failed_reload_refuses_to_save)
{
    mock_reset();
    boot_counter_t ctx;
    boot_counter_storage_t stor = make_storage();
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);
    for (int i = 0; i < 5; i++) boot_counter_increment(&ctx);

    g_fail_read = true;
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_ERR_STORAGE);
    g_fail_read = false;
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_ERR_PARAM);

    boot_counter_t next;
    boot_counter_init(&next, &stor);
    boot_counter_load(&next);
    ASSERT_EQ(boot_counter_get_count(&next), 5);
}

/* ================================================================
 * A key-value backend: reading a key nobody has written
 * ================================================================ */

static bool g_kv_exists;

static int kv_read(uint8_t *buf, size_t size, void *user_ctx)
{
    if (!g_kv_exists) return BOOT_COUNTER_READ_NO_RECORD;
    return mock_read(buf, size, user_ctx);
}

static int kv_write(const uint8_t *buf, size_t size, void *user_ctx)
{
    int rc = mock_write(buf, size, user_ctx);
    if (rc == 0) g_kv_exists = true;
    return rc;
}

static boot_counter_storage_t make_kv_storage(void)
{
    boot_counter_storage_t s;
    memset(&s, 0, sizeof(s));
    s.read = kv_read;
    s.write = kv_write;
    return s;
}

/* The run from the finding: three boots used to give load=-2 increment=-1
 * set_reset_reason=-1 and never one write. */
TEST(test_no_record_yet_is_created_by_the_first_save)
{
    mock_reset();
    g_kv_exists = false;
    boot_counter_storage_t stor = make_kv_storage();

    for (uint32_t boot = 1; boot <= 3; boot++) {
        boot_counter_t ctx;
        boot_counter_init(&ctx, &stor);
        ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);
        ASSERT_EQ(boot_counter_get_count(&ctx), boot - 1);
        ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
        ASSERT_EQ(boot_counter_set_reset_reason(&ctx, boot), BOOT_COUNTER_OK);
    }
    ASSERT_EQ(g_write_count, 6);
}

/* Any other failed read stays a failure and leaves the stored count alone. */
TEST(test_other_read_failure_is_not_no_record)
{
    mock_reset();
    g_kv_exists = false;
    boot_counter_storage_t stor = make_kv_storage();
    boot_counter_t ctx;
    boot_counter_init(&ctx, &stor);
    boot_counter_load(&ctx);
    boot_counter_increment(&ctx);

    boot_counter_t next;
    boot_counter_init(&next, &stor);
    g_fail_read = true;
    ASSERT_EQ(boot_counter_load(&next), BOOT_COUNTER_ERR_STORAGE);
    ASSERT_EQ(boot_counter_increment(&next), BOOT_COUNTER_ERR_PARAM);
    g_fail_read = false;
    ASSERT_EQ(g_write_count, 1);
}

static int no_record(uint8_t *buf, size_t size, void *user_ctx)
{
    (void)buf; (void)size; (void)user_ctx;
    return BOOT_COUNTER_READ_NO_RECORD;
}

/* With two copies, one key missing is just one copy missing. */
TEST(test_second_copy_with_no_record_yet)
{
    sector_reset();
    g_fail_write = false;
    put_record(0, 5, 10);

    boot_counter_t ctx;
    boot_counter_storage_t s;
    memset(&s, 0, sizeof(s));
    s.read = read_a;
    s.write = write_a;
    boot_counter_init(&ctx, &s);
    boot_counter_set_second_copy(&ctx, no_record, write_b);
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 5);

    /* Neither copy written yet: a clean start. */
    s.read = no_record;
    boot_counter_init(&ctx, &s);
    boot_counter_set_second_copy(&ctx, no_record, write_b);
    ASSERT_EQ(boot_counter_load(&ctx), BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&ctx), 0);
    ASSERT_EQ(boot_counter_increment(&ctx), BOOT_COUNTER_OK);
}

TEST_MAIN("boot_counter")
    RUN(test_init_valid);
    RUN(test_init_null_ctx);
    RUN(test_init_null_storage);
    RUN(test_init_null_read_cb);
    RUN(test_init_null_write_cb);
    RUN(test_load_save_roundtrip);
    RUN(test_increment);
    RUN(test_multiple_increments);
    RUN(test_reset_reason);
    RUN(test_uptime_field_is_volatile);
    RUN(test_uptime_ticks);
    RUN(test_storage_read_failure);
    RUN(test_storage_write_failure);
    RUN(test_set_reset_reason_write_failure);
    RUN(test_null_ctx_getters);
    RUN(test_null_ctx_mutators);
    RUN(test_increment_persists_each_call);
    RUN(test_increment_without_load);
    RUN(test_erased_storage_starts_from_zero);
    RUN(test_erase_then_power_loss_is_not_a_count);
    RUN(test_damaged_record_starts_from_zero);
    RUN(test_cut_between_erase_and_program_keeps_the_count);
    RUN(test_power_cut_anywhere_keeps_the_record);
    RUN(test_second_copy_needs_both_callbacks_before_load);
    RUN(test_copies_alternate);
    RUN(test_newest_copy_wins);
    RUN(test_equal_seq_takes_the_first_copy);
    RUN(test_damaged_newest_copy_falls_back);
    RUN(test_second_copy_read_failure_refuses_to_save);
    RUN(test_failed_write_keeps_the_good_copy);
    RUN(test_wrong_magic_with_good_crc_starts_from_zero);
    RUN(test_failed_reload_refuses_to_save);
    RUN(test_no_record_yet_is_created_by_the_first_save);
    RUN(test_other_read_failure_is_not_no_record);
    RUN(test_second_copy_with_no_record_yet);
TEST_MAIN_END()
