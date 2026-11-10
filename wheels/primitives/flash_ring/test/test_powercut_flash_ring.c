// SPDX-License-Identifier: MIT
/*
 * test_powercut_flash_ring.c — cut the power at every flash operation
 *
 * One script of pushes and pops runs about three laps on a small ring. For each
 * operation index N it runs again and the power dies at N: that erase or
 * program is lost or left half done, and everything after it fails. The ring
 * then comes back from the flash image and has to hold up: frames in order and
 * intact, nothing programmed over a cell that was not erased, the newest frame
 * still there unless the ring overwrote it.
 */
#include "test_harness.h"
#include <stdio.h>
#include <string.h>
#include "flash_ring.h"

#define SLOT        16u
#define SPS         4u                  /* slots per sector */
#define SECTOR      (SLOT * SPS)
#define HDR_ADDR    0u
#define DATA_ADDR   SECTOR
#define FLASH_SIZE  (SECTOR * 6u)

static uint8_t g_flash[FLASH_SIZE];
static int  g_ops;                      /* erases and programs so far */
static int  g_cut_at;                   /* operation the power dies on */
static bool g_dead;
static bool g_half;                     /* cut leaves that operation half done */
static bool g_hdr_open;                 /* header erased, not yet written */
static int  g_overprogram;

static bool cut_now(void)
{
    if (g_ops++ == g_cut_at) {
        g_dead = true;
        return true;
    }
    return false;
}

static int nor_erase(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (g_dead) return -1;
    uint32_t end = addr + SECTOR;
    if (end > FLASH_SIZE) end = FLASH_SIZE;
    if (cut_now()) {
        if (g_half) {
            memset(&g_flash[addr], 0xFF, (end - addr) / 2u);
            if (addr == HDR_ADDR) g_hdr_open = true;
        }
        return -1;
    }
    memset(&g_flash[addr], 0xFF, end - addr);
    if (addr == HDR_ADDR) g_hdr_open = true;
    return 0;
}

static int nor_write(void *ctx, uint32_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (g_dead) return -1;
    if (addr + len > FLASH_SIZE) return -1;
    uint16_t n = len;
    bool cut = cut_now();
    if (cut) n = g_half ? (uint16_t)(len / 2u) : 0u;
    for (uint16_t i = 0; i < n; i++) {
        if (g_flash[addr + i] != 0xFF) g_overprogram++;
        g_flash[addr + i] &= data[i];
    }
    if (cut) return -1;
    if (addr == HDR_ADDR) g_hdr_open = false;
    return 0;
}

static int nor_read(void *ctx, uint32_t addr, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    if (g_dead) return -1;
    if (addr + len > FLASH_SIZE) return -1;
    memcpy(buf, &g_flash[addr], len);
    return 0;
}

static const flash_ring_io_t g_io = {
    .erase_sector = nor_erase, .write = nor_write, .read = nor_read, .ctx = NULL,
};

/* ---------- frames that can be checked after the fact ---------- */

static void make_frame(uint8_t *f, uint32_t seq)
{
    for (uint32_t i = 0; i < 4u; i++) {
        f[i]      = (uint8_t)(seq >> (8u * i));
        f[4u + i] = (uint8_t)~f[i];
    }
    for (uint32_t i = 8u; i < SLOT; i++) {
        f[i] = (uint8_t)(seq * 31u + i);
    }
}

static bool frame_seq(const uint8_t *f, uint32_t *seq)
{
    uint8_t want[SLOT];
    uint32_t s = 0;
    for (uint32_t i = 0; i < 4u; i++) {
        s |= (uint32_t)f[i] << (8u * i);
    }
    make_frame(want, s);
    *seq = s;
    return memcmp(f, want, SLOT) == 0;
}

/* ---------- the run ---------- */

typedef struct {
    uint32_t next_seq;      /* next sequence number to push */
    uint32_t acked;         /* newest push that returned OK */
    uint32_t popped;        /* newest frame the script read and popped */
} script_t;

static uint32_t lcg(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s >> 16;
}

/* Runs until the steps are done or the power dies. */
static void run_script(flash_ring_t *ring, script_t *sc, uint32_t rng, uint32_t steps)
{
    uint8_t f[SLOT];
    for (uint32_t i = 0; i < steps && !g_dead; i++) {
        if (lcg(&rng) % 4u != 0u) {
            make_frame(f, sc->next_seq);
            if (flash_ring_push(ring, f, SLOT) == FLASH_RING_OK) {
                sc->acked = sc->next_seq;
            }
            sc->next_seq++;
        } else if (flash_ring_count(ring) > 0u) {
            uint32_t seq;
            if (flash_ring_peek(ring, ring->read_idx, f, SLOT) == FLASH_RING_OK &&
                frame_seq(f, &seq) &&
                flash_ring_pop(ring) == FLASH_RING_OK) {
                sc->popped = seq;
            }
        }
    }
}

/* The frames left must be whole, consecutive, and reach the newest one. */
static void check_ring(flash_ring_t *ring, const script_t *sc, uint32_t keep,
                       const char *what)
{
    uint8_t f[SLOT];
    uint32_t prev = 0, first = 0, n = 0;
    bool torn = false;
    uint32_t left = flash_ring_count(ring);

    ASSERT_LE(left, ring->cfg.total_slots - 1u);
    while (flash_ring_count(ring) > 0u) {
        uint32_t seq;
        ASSERT_EQ(FLASH_RING_OK, flash_ring_peek(ring, ring->read_idx, f, SLOT));
        if (!frame_seq(f, &seq)) {
            /* Only the push the power cut can be torn, and it comes last. */
            if (flash_ring_count(ring) != 1u) {
                printf("  %s: torn frame with %u behind it\n", what,
                       (unsigned)(flash_ring_count(ring) - 1u));
            }
            ASSERT_EQ(1u, flash_ring_count(ring));
            torn = true;
        } else {
            if (n > 0u) {
                ASSERT_EQ(prev + 1u, seq);
            } else {
                first = seq;
            }
            prev = seq;
        }
        n++;
        ASSERT_EQ(FLASH_RING_OK, flash_ring_pop(ring));
    }
    ASSERT_EQ(left, n);
    (void)torn;

    /* Newest acknowledged frame: there, unless the script already had it. */
    if (sc->acked > sc->popped) {
        ASSERT_TRUE(n > 0u);
        ASSERT_GE(prev, sc->acked);
    }
    /* Nothing went missing except what the writer overwrote. */
    if (n > 0u && sc->acked > keep) {
        uint32_t must = sc->popped + 1u;
        if (sc->acked - keep > must) must = sc->acked - keep;
        ASSERT_LE(first, must);
    }
}

static int g_cuts_seen;
static int g_resets_seen;

static void one_cut(const flash_ring_cfg_t *cfg, int cut_at, bool half,
                    uint32_t steps, bool *finished)
{
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_ops = 0;
    g_cut_at = cut_at;
    g_dead = false;
    g_half = half;
    g_hdr_open = false;
    g_overprogram = 0;

    script_t sc = { .next_seq = 1u, .acked = 0u, .popped = 0u };
    flash_ring_t ring;
    if (flash_ring_init(&ring, &g_io, cfg) == FLASH_RING_OK) {
        run_script(&ring, &sc, 12345u + cfg->persist_interval, steps);
    }
    *finished = !g_dead;
    if (*finished) return;
    g_cuts_seen++;

    /* Power back. The frames the writer may have overwritten by now: */
    bool hdr_lost = g_hdr_open;
    g_dead = false;
    g_cut_at = -1;
    char what[64];
    snprintf(what, sizeof(what), "cut %d half %d interval %u slots %u", cut_at,
             half ? 1 : 0, (unsigned)cfg->persist_interval,
             (unsigned)cfg->total_slots);

    flash_ring_t after;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&after, &g_io, cfg));
    ASSERT_EQ(0, g_overprogram);

    /*
     * W1A-9: there is one header copy, so a cut between its erase and its
     * program loses it and the ring starts again. Until a second copy exists
     * an empty ring is the correct outcome of that one window.
     */
    if (hdr_lost) {
        g_resets_seen++;
        ASSERT_EQ(0u, flash_ring_count(&after));
    } else {
        uint32_t keep = cfg->total_slots - 2u * SPS;
        check_ring(&after, &sc, keep, what);
    }
    if (tests_fail > 0) {
        printf("  failed at %s\n", what);
        return;
    }

    /* The ring keeps working: two more laps, then the same checks. */
    script_t more = { .next_seq = sc.next_seq + 1u, .acked = 0u, .popped = 0u };
    uint8_t f[SLOT];
    for (uint32_t i = 0; i < 2u * cfg->total_slots; i++) {
        make_frame(f, more.next_seq);
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&after, f, SLOT));
        more.acked = more.next_seq;
        more.next_seq++;
    }
    ASSERT_EQ(0, g_overprogram);

    flash_ring_t again;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&again, &g_io, cfg));
    check_ring(&again, &more, cfg->total_slots - 2u * SPS, what);
    if (tests_fail > 0) {
        printf("  failed after the laps at %s\n", what);
    }
}

static void sweep(uint32_t total_slots, uint16_t interval)
{
    const flash_ring_cfg_t cfg = {
        .slot_size = SLOT, .total_slots = total_slots, .sector_size = SECTOR,
        .header_addr = HDR_ADDR, .data_start_addr = DATA_ADDR,
        .persist_interval = interval,
    };
    uint32_t steps = total_slots * 5u;

    for (int half = 0; half <= 1; half++) {
        for (int cut = 0; ; cut++) {
            bool finished = false;
            one_cut(&cfg, cut, half != 0, steps, &finished);
            if (finished || tests_fail > 0) break;
        }
        if (tests_fail > 0) return;
    }
}

TEST(test_powercut_full_sectors_every_push)  { sweep(16u, 0u); }
TEST(test_powercut_full_sectors_interval_1)  { sweep(16u, 1u); }
TEST(test_powercut_full_sectors_interval_3)  { sweep(16u, 3u); }
TEST(test_powercut_full_sectors_interval_16) { sweep(16u, 16u); }
TEST(test_powercut_partial_sector_every_push)  { sweep(14u, 0u); }
TEST(test_powercut_partial_sector_interval_3)  { sweep(14u, 3u); }
TEST(test_powercut_partial_sector_interval_16) { sweep(14u, 16u); }

/* No cut at all, just resets every few pushes: the ring must keep its place. */
static void reset_storm(uint32_t total_slots, uint16_t interval, uint32_t every)
{
    const flash_ring_cfg_t cfg = {
        .slot_size = SLOT, .total_slots = total_slots, .sector_size = SECTOR,
        .header_addr = HDR_ADDR, .data_start_addr = DATA_ADDR,
        .persist_interval = interval,
    };
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_ops = 0;
    g_cut_at = -1;
    g_dead = false;
    g_overprogram = 0;

    script_t sc = { .next_seq = 1u, .acked = 0u, .popped = 0u };
    flash_ring_t ring;
    ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring, &g_io, &cfg));
    uint8_t f[SLOT];
    for (uint32_t i = 0; i < 6u * total_slots; i++) {
        make_frame(f, sc.next_seq);
        ASSERT_EQ(FLASH_RING_OK, flash_ring_push(&ring, f, SLOT));
        sc.acked = sc.next_seq++;
        if ((i + 1u) % every == 0u) {
            ASSERT_EQ(FLASH_RING_OK, flash_ring_init(&ring, &g_io, &cfg));
        }
    }
    ASSERT_EQ(0, g_overprogram);
    check_ring(&ring, &sc, total_slots - 2u * SPS, "reset storm");
}

TEST(test_reset_storms)
{
    static const uint16_t intervals[] = { 1u, 3u, 8u, 12u, 16u, 0xFFFFu };
    for (uint32_t k = 0; k < sizeof(intervals) / sizeof(intervals[0]); k++) {
        for (uint32_t every = 1u; every <= 20u; every++) {
            reset_storm(16u, intervals[k], every);
            reset_storm(14u, intervals[k], every);
            if (tests_fail > 0) {
                printf("  failed at interval %u, reset every %u\n",
                       (unsigned)intervals[k], (unsigned)every);
                return;
            }
        }
    }
}

/* The sweep must actually have cut, and hit the header window too. */
TEST(test_powercut_sweep_ran)
{
    ASSERT_TRUE(g_cuts_seen > 1000);
    ASSERT_TRUE(g_resets_seen > 0);
}

TEST_MAIN("flash_ring power cuts")
    RUN(test_powercut_full_sectors_every_push);
    RUN(test_powercut_full_sectors_interval_1);
    RUN(test_powercut_full_sectors_interval_3);
    RUN(test_powercut_full_sectors_interval_16);
    RUN(test_powercut_partial_sector_every_push);
    RUN(test_powercut_partial_sector_interval_3);
    RUN(test_powercut_partial_sector_interval_16);
    RUN(test_powercut_sweep_ran);
    RUN(test_reset_storms);
TEST_MAIN_END()
