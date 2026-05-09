/*
 * prop_ncp5623_parser.c — property-based tests for ncp5623_parser
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_ncp5623_parser.c ../ncp5623_parser.c -o prop_ncp5623_parser
 */

#include "test_harness.h"
#include "property_test.h"
#include "../ncp5623_parser.h"
#include <string.h>

/* Property: feed random bytes never crashes */
PROPERTY(feed_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    ncp5623_parser_t p;
    ncp5623_parser_init(&p);

    uint8_t num_transactions = (uint8_t)ARBI_RANGE(seed, 1, 10);
    for (uint8_t t = 0; t < num_transactions; t++) {
        ncp5623_parser_start(&p);
        uint8_t num_bytes = (uint8_t)ARBI_RANGE(seed, 1, 20);
        for (uint8_t i = 0; i < num_bytes; i++) {
            ncp5623_evt_t evt;
            memset(&evt, 0, sizeof(evt));
            ncp5623_parser_feed(&p, ARBI_UINT8(seed), &evt);
        }
        ncp5623_parser_stop(&p);
    }
}

/* Property: init is idempotent */
PROPERTY(init_idempotent, 5000) {
    ncp5623_parser_t p1, p2;
    ncp5623_parser_init(&p1);
    ncp5623_parser_init(&p2);
    ncp5623_parser_init(&p2); /* second init */

    /* Both should be in the same state */
    PROP_ASSERT_EQ(p1.idx, p2.idx);
    PROP_ASSERT_EQ(p1.enabled, p2.enabled);
}

/* Property: event type is always a valid enum value */
PROPERTY(event_type_valid, 10000) {
    ncp5623_parser_t p;
    ncp5623_parser_init(&p);
    ncp5623_parser_start(&p);

    uint8_t num_bytes = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < num_bytes; i++) {
        ncp5623_evt_t evt;
        memset(&evt, 0, sizeof(evt));
        ncp5623_evt_type_t etype = ncp5623_parser_feed(&p, ARBI_UINT8(seed), &evt);
        PROP_ASSERT(etype == NCP5623_EVT_NONE ||
                    etype == NCP5623_EVT_ENABLE ||
                    etype == NCP5623_EVT_COLOR);
    }
    ncp5623_parser_stop(&p);
}

TEST_MAIN("ncp5623_parser properties") {
    RUN(feed_no_crash);
    RUN(init_idempotent);
    RUN(event_type_valid);
}
TEST_MAIN_END()
