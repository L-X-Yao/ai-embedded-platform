// SPDX-License-Identifier: MIT
/*
 * test_faults_flash_ring.c — random operations with power cuts and I/O faults
 *
 * Each run takes a small ring and a fault rate from its seed, then pushes,
 * pops, discards, flushes the header and resets at random. Any erase or
 * program can fail, fail after it landed, land half, or be where the power
 * dies, and any read can fail. A dead flash refuses everything until the next
 * boot.
 *
 * The oracle does not model the ring. It records what was programmed and which
 * pushes returned OK, and after every operation it checks that
 *   - a live slot is never blank and never holds foreign data;
 *   - whole frames in the live range are in push order;
 *   - a frame whose push returned OK stays live until a documented rule lets
 *     it go: pop, discard, the sector erased ahead, the reset rule, a lost
 *     header;
 *   - after a reset the writer is where the last program ended;
 *   - nothing is programmed over cells that are not blank, or outside the ring.
 *
 * Left out, as the README says: a half-written header, a frame of all 0xFF and
 * flash written by older code.
 */
#include "test_harness.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "flash_ring.h"

#define MAX_SLOTS   64u
#define MAX_SLOT    16u                 /* bytes */
#define MAX_SEQ     2048u               /* pushes in one run, with room to spare */
#define FLASH_BYTES ((MAX_SLOTS + 16u) * MAX_SLOT)
#define SEEDS       150u                /* runs per geometry and read-back rate */

typedef enum {
    GEOM_SMALL,                         /* 2 to 5 sectors of 2, 4 or 8 slots */
    GEOM_ONE_SLOT_SECTORS,              /* 2 to 5 sectors of one slot */
    GEOM_LARGER,                        /* 2 to 8 sectors of 2, 4 or 8 slots */
} geom_t;

/* ---------- simulated flash ---------- */

static uint8_t  g_mem[FLASH_BYTES];
static uint32_t g_slot, g_sps, g_sect, g_total, g_data, g_data_end;
static uint64_t g_rng;
static uint32_t g_fault_pm;             /* faults per mille, per flash operation */
static uint32_t g_readback_pm;          /* more read faults right after a failed program */
static bool     g_faults_on;
static bool     g_dead;                 /* power is off until the next boot */
static bool     g_program_failed;       /* the last program reported an error */
static bool     g_read_failed;          /* a read failed during this operation */
static uint32_t g_overprogram, g_outside;

/* ---------- what the oracle knows ---------- */

static uint32_t g_true_w;               /* slot after the last one a program touched */
static uint32_t g_touched[MAX_SLOTS];   /* push that last touched the slot, 0 if none since its erase */
static uint8_t  g_acked[MAX_SEQ];       /* pushes that returned OK */
static uint32_t g_next_seq, g_in_flight;
static uint32_t g_live_seq[MAX_SLOTS], g_live_slot[MAX_SLOTS], g_live_n;
static bool     g_allow_all;            /* a discard or a lost header: anything may go */
static uint32_t g_allow_slot;           /* slot a pop may take */
static bool     g_allow_sector[MAX_SLOTS];

static flash_ring_t     g_ring;
static flash_ring_cfg_t g_cfg;

static bool     g_bad;
static char     g_why[200];
static uint32_t g_runs, g_cuts, g_unread, g_lost_headers;

#define BAD(...) do { \
    if (!g_bad) { \
        g_bad = true; \
        (void)snprintf(g_why, sizeof(g_why), __VA_ARGS__); \
    } } while (0)

static uint32_t rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t)(g_rng >> 16);
}

/* A frame names its push, and no byte pattern of it is all 0xFF. */
static void frame_of(uint32_t seq, uint8_t *f)
{
    f[0] = 0xA5;
    for (uint32_t i = 0; i < 4u; i++) {
        f[1u + i] = (uint8_t)(seq >> (8u * i));
    }
    for (uint32_t i = 5u; i < g_slot; i++) {
        f[i] = (uint8_t)((seq * 31u + i * 7u) & 0x7Fu);
    }
}

typedef enum {
    F_NONE,
    F_CUT_BEFORE,                       /* power dies, nothing done */
    F_CUT_HALF,                         /* power dies part way */
    F_CUT_AFTER,                        /* power dies, the operation done */
    F_ERR_NOOP,                         /* reports an error, nothing done */
    F_ERR_LANDED,                       /* reports an error, the operation done */
    F_ERR_HALF,                         /* reports an error, done part way */
} fault_t;

static fault_t pick_fault(bool header)
{
    static const fault_t on_data[] = {
        F_CUT_BEFORE, F_CUT_HALF, F_CUT_AFTER, F_ERR_NOOP, F_ERR_LANDED, F_ERR_HALF,
    };
    /* The header has no checksum, so it is never left half done. */
    static const fault_t on_header[] = {
        F_CUT_BEFORE, F_CUT_AFTER, F_ERR_NOOP, F_ERR_LANDED,
    };

    if (!g_faults_on || rnd() % 1000u >= g_fault_pm) {
        return F_NONE;
    }
    return header ? on_header[rnd() % 4u] : on_data[rnd() % 6u];
}

static int sim_erase(void *ctx, uint32_t addr)
{
    (void)ctx;
    if (g_dead) {
        return -1;
    }
    bool header = addr == 0u;
    if (!header && (addr < g_data || addr >= g_data_end || addr % g_sect != 0u)) {
        g_outside++;
        return -1;
    }
    fault_t f = pick_fault(header);
    if (f == F_CUT_BEFORE) {
        g_dead = true;
        return -1;
    }
    if (f == F_ERR_NOOP) {
        return -1;
    }
    bool half = f == F_CUT_HALF || f == F_ERR_HALF;
    for (uint32_t i = 0; i < g_sect; i++) {
        if (!half || (rnd() & 1u) != 0u) {
            g_mem[addr + i] = 0xFF;
        }
    }
    if (!header && !half) {
        /* A half erased slot keeps its tag: what is left is debris of its frame. */
        uint32_t first = (addr - g_data) / g_slot;
        for (uint32_t s = first; s < first + g_sps && s < g_total; s++) {
            g_touched[s] = 0u;
        }
    }
    if (f == F_CUT_HALF || f == F_CUT_AFTER) {
        g_dead = true;
        return -1;
    }
    return (f == F_ERR_LANDED || f == F_ERR_HALF) ? -1 : 0;
}

static int sim_write(void *ctx, uint32_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (g_dead) {
        return -1;
    }
    bool header = addr < g_sect;
    if (header) {
        if (addr + len > g_sect) {
            g_outside++;
            return -1;
        }
    } else if (addr < g_data || addr + len > g_data + g_total * g_slot ||
               (addr - g_data) % g_slot != 0u) {
        g_outside++;
        return -1;
    }
    fault_t f = pick_fault(header);
    if (f == F_CUT_BEFORE) {
        g_dead = true;
        return -1;
    }
    if (f == F_ERR_NOOP) {
        g_program_failed = !header;
        return -1;
    }
    uint32_t n = len;
    if (f == F_CUT_HALF || f == F_ERR_HALF) {
        n = rnd() % (uint32_t)len;      /* 0 to len - 1 bytes land */
    }
    for (uint32_t i = 0; i < n; i++) {
        if (g_mem[addr + i] != 0xFF) {
            g_overprogram++;
        }
        g_mem[addr + i] &= data[i];
    }
    if (!header && n > 0u) {
        uint32_t s = (addr - g_data) / g_slot;
        g_touched[s] = g_in_flight;
        g_true_w = (s + 1u) % g_total;
    }
    if (f == F_CUT_HALF || f == F_CUT_AFTER) {
        g_dead = true;
        return -1;
    }
    if (f == F_ERR_LANDED || f == F_ERR_HALF) {
        g_program_failed = !header;
        return -1;
    }
    return 0;
}

static int sim_read(void *ctx, uint32_t addr, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    if (g_dead) {
        return -1;
    }
    if (addr + len > g_data_end) {
        g_outside++;
        return -1;
    }
    bool fault = false;
    if (g_faults_on) {
        /* The read-back after a failed program fails more often than other
         * reads, or the slot left unread would hardly ever be reached. */
        if (g_program_failed && g_readback_pm != 0u) {
            fault = rnd() % 1000u < g_readback_pm;
        }
        if (!fault) {
            fault = rnd() % 1000u < g_fault_pm;
        }
    }
    g_program_failed = false;
    if (fault) {
        g_read_failed = true;
        return -1;
    }
    memcpy(buf, &g_mem[addr], len);
    return 0;
}

static const flash_ring_io_t g_io = {
    .erase_sector = sim_erase, .write = sim_write, .read = sim_read, .ctx = NULL,
};

/* ---------- geometry, as the README states it ---------- */

static bool last_in_sector(uint32_t slot)
{
    return slot + 1u == g_total || (slot + 1u) % g_sps == 0u;
}

static uint32_t sector_of(uint32_t slot)
{
    return slot / g_sps;
}

/* Sector after the one holding slot, wrapping to the first. */
static uint32_t next_sector(uint32_t slot)
{
    uint32_t first = (slot / g_sps + 1u) * g_sps;
    return first < g_total ? first / g_sps : 0u;
}

/* ---------- the checks ---------- */

/* Classify the live range and compare it with what was live before. */
static void check_live(const char *what)
{
    uint32_t count = flash_ring_count(&g_ring);
    if (count >= g_total || g_ring.write_idx >= g_total || g_ring.read_idx >= g_total) {
        BAD("%s: count %u, write_idx %u, read_idx %u", what, (unsigned)count,
            (unsigned)g_ring.write_idx, (unsigned)g_ring.read_idx);
        return;
    }

    uint32_t now_seq[MAX_SLOTS], now_slot[MAX_SLOTS], now_n = 0;
    uint32_t newest = 0;
    uint8_t want[MAX_SLOT], got[MAX_SLOT];

    for (uint32_t i = 0; i < count; i++) {
        uint32_t s = (g_ring.read_idx + i) % g_total;
        const uint8_t *p = &g_mem[g_data + s * g_slot];
        if (flash_ring_peek(&g_ring, s, got, (uint16_t)g_slot) != FLASH_RING_OK ||
            memcmp(got, p, g_slot) != 0) {
            BAD("%s: peek of live slot %u", what, (unsigned)s);
            return;
        }
        uint32_t seq = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
                       ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);
        bool whole = false;
        if (p[0] == 0xA5 && seq > 0u && seq < g_next_seq) {
            frame_of(seq, want);
            whole = memcmp(want, p, g_slot) == 0;
        }
        if (whole) {
            if (seq <= newest) {
                BAD("%s: slot %u holds frame %u after frame %u", what, (unsigned)s,
                    (unsigned)seq, (unsigned)newest);
                return;
            }
            newest = seq;
            if (g_acked[seq] != 0u) {
                now_seq[now_n] = seq;
                now_slot[now_n] = s;
                now_n++;
            }
            continue;
        }

        /* Not a whole frame: only the remains of the push that touched the slot. */
        uint32_t t = g_touched[s];
        bool blank = true;
        bool remains = t != 0u;
        if (t != 0u) {
            frame_of(t, want);
        }
        for (uint32_t b = 0; b < g_slot; b++) {
            if (p[b] != 0xFF) {
                blank = false;
                if (t != 0u && p[b] != want[b]) {
                    remains = false;
                }
            }
        }
        if (blank) {
            BAD("%s: live slot %u is blank", what, (unsigned)s);
            return;
        }
        if (!remains) {
            BAD("%s: live slot %u holds foreign data", what, (unsigned)s);
            return;
        }
        if (g_acked[t] != 0u) {
            BAD("%s: frame %u in slot %u is damaged", what, (unsigned)t, (unsigned)s);
            return;
        }
    }

    /* Nothing acknowledged went missing without a rule. */
    for (uint32_t i = 0; i < g_live_n; i++) {
        bool found = false;
        for (uint32_t j = 0; j < now_n && !found; j++) {
            found = now_seq[j] == g_live_seq[i];
        }
        if (!found && !g_allow_all && g_live_slot[i] != g_allow_slot &&
            !g_allow_sector[sector_of(g_live_slot[i])]) {
            BAD("%s: frame %u in slot %u is lost", what, (unsigned)g_live_seq[i],
                (unsigned)g_live_slot[i]);
            return;
        }
    }
    memcpy(g_live_seq, now_seq, now_n * sizeof(now_seq[0]));
    memcpy(g_live_slot, now_slot, now_n * sizeof(now_slot[0]));
    g_live_n = now_n;
    g_allow_all = false;
    g_allow_slot = UINT32_MAX;
    memset(g_allow_sector, 0, sizeof(g_allow_sector));

    /* A slot outside the live range is refused. */
    uint32_t s = rnd() % g_total;
    uint32_t from = s >= g_ring.read_idx ? s - g_ring.read_idx
                                         : g_total - g_ring.read_idx + s;
    flash_ring_err_t e = flash_ring_peek(&g_ring, s, got, (uint16_t)g_slot);
    if (count == 0u ? e != FLASH_RING_ERR_EMPTY
                    : (from >= count && e != FLASH_RING_ERR_PARAM)) {
        BAD("%s: peek of slot %u outside the live range returned %d", what,
            (unsigned)s, (int)e);
        return;
    }
    if (g_overprogram != 0u) {
        BAD("%s: programmed over cells that were not blank", what);
        return;
    }
    if (g_outside != 0u) {
        BAD("%s: access outside the ring", what);
    }
}

static void observe(const char *what)
{
    bool faults = g_faults_on;
    g_faults_on = false;
    check_live(what);
    g_faults_on = faults;
}

/* Power on: init until it succeeds, then compare the ring with the flash. */
static void boot(void)
{
    bool header_lost = false;
    uint32_t tries = 0;

    g_program_failed = false;
    for (;;) {
        uint32_t magic;
        memcpy(&magic, g_mem, sizeof(magic));
        if (magic != FLASH_RING_MAGIC) {
            header_lost = true;
        }
        if (last_in_sector(g_true_w)) {
            g_allow_sector[next_sector(g_true_w)] = true;   /* the reset rule */
        }
        g_dead = false;
        /* A long walk at a high fault rate may never get through: after 50
         * failed boots in a row one runs without faults. */
        g_faults_on = tries < 50u;
        flash_ring_err_t e = flash_ring_init(&g_ring, &g_io, &g_cfg);
        if (e == FLASH_RING_OK && !g_dead) {
            break;
        }
        if (e != FLASH_RING_OK && e != FLASH_RING_ERR_IO) {
            BAD("init returned %d", (int)e);
            break;
        }
        if (++tries > 60u) {
            BAD("init never succeeds");
            break;
        }
    }
    g_faults_on = true;
    g_dead = false;
    if (g_bad) {
        return;
    }

    if (header_lost) {
        g_lost_headers++;
        g_allow_all = true;
        g_true_w = g_ring.write_idx;    /* a fresh ring starts over */
    } else if (g_ring.write_idx != g_true_w) {
        BAD("after a reset write_idx is %u and the last program ended at %u",
            (unsigned)g_ring.write_idx, (unsigned)g_true_w);
        return;
    }
    observe("reset");
}

/* ---------- the operations ---------- */

static void do_push(void)
{
    uint8_t frame[MAX_SLOT];
    uint32_t w0 = g_ring.write_idx;
    uint32_t seq = g_next_seq++;

    frame_of(seq, frame);
    g_in_flight = seq;
    g_read_failed = false;
    g_program_failed = false;
    /* Where a program that works will land. After a failed program it could
     * not read back, the ring is one slot behind the flash. */
    uint32_t land = g_true_w;
    if (last_in_sector(w0)) {
        g_allow_sector[next_sector(w0)] = true;             /* erased ahead */
    }
    if (land != w0 && last_in_sector(land)) {
        g_allow_sector[next_sector(land)] = true;
    }

    flash_ring_err_t e = flash_ring_push(&g_ring, frame, (uint16_t)g_slot);
    if (g_dead) {
        return;                         /* the caller never saw the result */
    }
    if (e == FLASH_RING_OK) {
        g_acked[seq] = 1u;
        if (g_ring.write_idx != (land + 1u) % g_total ||
            memcmp(&g_mem[g_data + land * g_slot], frame, g_slot) != 0) {
            BAD("push returned OK, but slot %u does not hold the frame", (unsigned)land);
            return;
        }
    } else if (e != FLASH_RING_ERR_IO) {
        BAD("push returned %d", (int)e);
        return;
    }
    if (g_ring.write_idx != g_true_w) {
        /* A failed program whose slot could not be read back: the ring stays
         * on that slot until the next push has read it. */
        if (e != FLASH_RING_ERR_IO || !g_read_failed ||
            g_true_w != (g_ring.write_idx + 1u) % g_total) {
            BAD("after a push write_idx is %u and the last program ended at %u",
                (unsigned)g_ring.write_idx, (unsigned)g_true_w);
            return;
        }
        g_unread++;
    }
    observe("push");
}

static void do_pop(void)
{
    uint32_t before = flash_ring_count(&g_ring);

    g_allow_slot = g_ring.read_idx;
    flash_ring_err_t e = flash_ring_pop(&g_ring);
    if (g_dead) {
        return;
    }
    if (before == 0u ? e != FLASH_RING_ERR_EMPTY
                     : (e != FLASH_RING_OK || flash_ring_count(&g_ring) != before - 1u)) {
        BAD("pop with %u frames returned %d", (unsigned)before, (int)e);
        return;
    }
    observe("pop");
}

static void do_discard(void)
{
    g_allow_all = true;
    flash_ring_err_t e = flash_ring_discard_all(&g_ring);
    if (g_dead) {
        return;
    }
    if ((e != FLASH_RING_OK && e != FLASH_RING_ERR_IO) || flash_ring_count(&g_ring) != 0u) {
        BAD("discard returned %d and left %u frames", (int)e,
            (unsigned)flash_ring_count(&g_ring));
        return;
    }
    observe("discard");
}

static void do_flush(void)
{
    flash_ring_err_t e = flash_ring_flush_header(&g_ring);
    if (g_dead) {
        return;
    }
    if (e != FLASH_RING_OK && e != FLASH_RING_ERR_IO) {
        BAD("flush returned %d", (int)e);
        return;
    }
    observe("flush");
}

/* ---------- one run ---------- */

static void one_run(uint64_t seed, geom_t geom, uint32_t readback_pm)
{
    static const uint16_t intervals[] = { 0, 1, 2, 3, 5, 0, 0, 0, 0xFFFF };
    static const uint32_t rates[] = { 0, 5, 20, 60, 150 };

    g_rng = seed * 0x9E3779B97F4A7C15ull + 0x1234567ull;
    for (int i = 0; i < 8; i++) {
        (void)rnd();
    }

    g_slot = (rnd() & 1u) != 0u ? 8u : 16u;
    g_sps = 1u << (1u + rnd() % 3u);
    if (geom == GEOM_ONE_SLOT_SECTORS) {
        g_slot = 16u;                   /* the header takes 16 bytes of its sector */
        g_sps = 1u;
    }
    g_sect = g_slot * g_sps;
    uint32_t sectors = 2u + rnd() % (geom == GEOM_LARGER ? 7u : 4u);
    g_total = sectors * g_sps;
    if (rnd() % 3u == 0u) {
        g_total -= rnd() % g_sps;       /* a partial last sector */
    }
    if (g_total <= g_sps) {
        g_total = g_sps + 1u;
    }
    sectors = (g_total + g_sps - 1u) / g_sps;
    g_data = g_sect;
    g_data_end = g_data + sectors * g_sect;

    uint16_t interval = intervals[rnd() % 9u];
    uint32_t pick = rnd() % 4u;
    if (interval == 0u && pick == 1u) {
        interval = (uint16_t)g_sps;
    } else if (interval == 0u && pick == 2u) {
        interval = (uint16_t)(g_total - g_sps);
    } else if (interval == 0u && pick == 3u) {
        interval = (uint16_t)g_total;
    }
    g_fault_pm = rates[rnd() % 5u];
    g_readback_pm = readback_pm;

    g_cfg.slot_size = (uint16_t)g_slot;
    g_cfg.total_slots = g_total;
    g_cfg.sector_size = g_sect;
    g_cfg.header_addr = 0u;
    g_cfg.data_start_addr = g_data;
    g_cfg.persist_interval = interval;

    /* Blank flash, or flash full of something else with no header on it. */
    bool dirty = (rnd() & 1u) != 0u;
    for (uint32_t i = 0; i < g_data_end; i++) {
        g_mem[i] = 0xFF;
        if (dirty) {
            g_mem[i] = (uint8_t)rnd();
        }
    }
    memset(g_mem, 0xFF, 4);

    memset(g_touched, 0, sizeof(g_touched));
    memset(g_acked, 0, sizeof(g_acked));
    memset(g_allow_sector, 0, sizeof(g_allow_sector));
    g_next_seq = 1u;
    g_in_flight = 0u;
    g_live_n = 0u;
    g_allow_all = true;
    g_allow_slot = UINT32_MAX;
    g_true_w = 0u;
    g_overprogram = 0u;
    g_outside = 0u;
    g_read_failed = false;
    g_runs++;

    boot();
    uint32_t ops = 200u + rnd() % 1500u;
    for (uint32_t op = 0; op < ops && !g_bad; op++) {
        uint32_t r = rnd() % 100u;
        if (r < 58u) {
            do_push();
        } else if (r < 82u) {
            do_pop();
        } else if (r < 85u) {
            do_discard();
        } else if (r < 89u) {
            do_flush();
        } else {
            boot();                     /* a reset with the power still on */
        }
        if (g_dead && !g_bad) {
            g_cuts++;
            boot();
        }
    }
}

static void run_seeds(geom_t geom, uint32_t readback_pm)
{
    for (uint32_t seed = 1; seed <= SEEDS && !g_bad; seed++) {
        one_run(seed, geom, readback_pm);
        if (g_bad) {
            printf("  seed %u, geometry %d, read-back faults %u per mille: %s\n",
                   (unsigned)seed, (int)geom, (unsigned)readback_pm, g_why);
        }
    }
}

static void run_geometry(geom_t geom)
{
    g_bad = false;
    run_seeds(geom, 0u);
    run_seeds(geom, 300u);
    run_seeds(geom, 800u);
}

TEST(test_faults_rings_of_two_to_five_sectors)
{
    run_geometry(GEOM_SMALL);
    ASSERT_FALSE(g_bad);
}

TEST(test_faults_one_slot_a_sector)
{
    run_geometry(GEOM_ONE_SLOT_SECTORS);
    ASSERT_FALSE(g_bad);
}

TEST(test_faults_rings_of_up_to_eight_sectors)
{
    run_geometry(GEOM_LARGER);
    ASSERT_FALSE(g_bad);
}

/* The runs must have cut the power, lost headers and left slots unread. */
TEST(test_faults_ran)
{
    ASSERT_EQ(9u * SEEDS, g_runs);
    ASSERT_GT(g_cuts, 20000u);
    ASSERT_GT(g_lost_headers, 6000u);
    ASSERT_GT(g_unread, 2000u);
}

TEST_MAIN("flash_ring faults")
    RUN(test_faults_rings_of_two_to_five_sectors);
    RUN(test_faults_one_slot_a_sector);
    RUN(test_faults_rings_of_up_to_eight_sectors);
    RUN(test_faults_ran);
TEST_MAIN_END()
