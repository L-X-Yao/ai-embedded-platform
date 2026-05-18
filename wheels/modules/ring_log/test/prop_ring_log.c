// SPDX-License-Identifier: MIT
/*
 * prop_ring_log.c — property-based tests for ring_log
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_ring_log.c ../ring_log.c -o prop_ring_log
 */

#include "test_harness.h"
#include "property_test.h"
#include "../ring_log.h"
#include <string.h>

/* Property: random write/read/clear sequences never crash, count bounded */
PROPERTY(random_ops_no_crash, 10000) {
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 50);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 5;
        if (op == 0) {
            ring_log_level_t lvl = (ring_log_level_t)ARBI_RANGE(seed, 0, 3);
            ring_log_write(&rl, lvl, ARBI_UINT32(seed), "msg %d", (int)i);
        } else if (op == 1) {
            ring_log_entry_t entry;
            uint16_t idx = (uint16_t)ARBI_RANGE(seed, 0, RING_LOG_MAX_ENTRIES);
            ring_log_read(&rl, idx, &entry);
        } else if (op == 2) {
            ring_log_clear(&rl);
        } else if (op == 3) {
            ring_log_set_level(&rl, (ring_log_level_t)ARBI_RANGE(seed, 0, 3));
        } else {
            ring_log_entry_t entry;
            ring_log_read_oldest(&rl, 0, &entry);
        }
    }

    uint16_t count = ring_log_count(&rl);
    PROP_ASSERT(count <= RING_LOG_MAX_ENTRIES);
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    ring_log_t rl1, rl2;
    ring_log_init(&rl1, RING_LOG_INFO);
    ring_log_init(&rl2, RING_LOG_INFO);
    ring_log_init(&rl2, RING_LOG_INFO);

    PROP_ASSERT_EQ(ring_log_count(&rl1), ring_log_count(&rl2));
    PROP_ASSERT_EQ(ring_log_total(&rl1), ring_log_total(&rl2));
    PROP_ASSERT_EQ(ring_log_dropped(&rl1), ring_log_dropped(&rl2));
}

/* Property: count <= total (total includes overwrites) */
PROPERTY(count_le_total, 10000) {
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);

    uint8_t writes = (uint8_t)ARBI_RANGE(seed, 0, RING_LOG_MAX_ENTRIES * 2);
    for (uint8_t i = 0; i < writes; i++) {
        ring_log_write(&rl, RING_LOG_INFO, i, "entry %d", (int)i);
    }

    uint16_t count = ring_log_count(&rl);
    uint32_t total = ring_log_total(&rl);
    PROP_ASSERT(count <= RING_LOG_MAX_ENTRIES);
    PROP_ASSERT(total >= count);
}

/* Property: clear resets count to zero */
PROPERTY(clear_resets, 5000) {
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);

    uint8_t writes = (uint8_t)ARBI_RANGE(seed, 1, 20);
    for (uint8_t i = 0; i < writes; i++) {
        ring_log_write(&rl, RING_LOG_INFO, i, "msg");
    }

    ring_log_clear(&rl);
    PROP_ASSERT_EQ(ring_log_count(&rl), 0);
}

TEST_MAIN("ring_log properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(count_le_total);
    RUN(clear_resets);
}
TEST_MAIN_END()
