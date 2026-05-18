// SPDX-License-Identifier: MIT
/*
 * prop_event_bus.c — property-based tests for event_bus
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_event_bus.c ../event_bus.c -o prop_event_bus
 */

#include "test_harness.h"
#include "property_test.h"
#include "../event_bus.h"
#include <string.h>

static int cb_count;
static void mock_cb(uint16_t event_id, const void *data,
                     size_t data_len, void *ctx) {
    (void)event_id; (void)data; (void)data_len; (void)ctx;
    cb_count++;
}

static void mock_cb2(uint16_t event_id, const void *data,
                      size_t data_len, void *ctx) {
    (void)event_id; (void)data; (void)data_len; (void)ctx;
}

/* Property: random subscribe/unsubscribe/publish sequences never crash */
PROPERTY(random_ops_no_crash, 10000) {
    event_bus_t bus;
    event_bus_init(&bus);
    cb_count = 0;

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 40);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 4;
        uint16_t eid = (uint16_t)ARBI_RANGE(seed, 1, 8);
        if (op == 0) {
            event_bus_subscribe(&bus, eid, mock_cb, NULL);
        } else if (op == 1) {
            event_bus_unsubscribe(&bus, eid, mock_cb, NULL);
        } else if (op == 2) {
            uint32_t data = ARBI_UINT32(seed);
            event_bus_publish(&bus, eid, &data, sizeof(data));
        } else {
            event_bus_subscribe(&bus, eid, mock_cb2, NULL);
        }
    }
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    event_bus_t bus1, bus2;
    event_bus_init(&bus1);
    event_bus_init(&bus2);
    event_bus_init(&bus2);

    /* Both should have zero slots */
    PROP_ASSERT_EQ(bus1.slot_count, bus2.slot_count);
}

/* Property: publish delivers to all subscribers */
PROPERTY(publish_delivers, 10000) {
    event_bus_t bus;
    event_bus_init(&bus);
    cb_count = 0;

    uint8_t n_subs = (uint8_t)ARBI_RANGE(seed, 1, EVENT_BUS_MAX_SUBS_PER_EVENT);
    /* Subscribe n_subs different callbacks (using different ctx as differentiator) */
    uintptr_t contexts[EVENT_BUS_MAX_SUBS_PER_EVENT];
    for (uint8_t i = 0; i < n_subs; i++) {
        contexts[i] = (uintptr_t)(i + 1);
        event_bus_subscribe(&bus, 42, mock_cb, (void *)contexts[i]);
    }

    cb_count = 0;
    uint32_t data = 0;
    event_bus_publish(&bus, 42, &data, sizeof(data));
    PROP_ASSERT_EQ(cb_count, n_subs);
}

TEST_MAIN("event_bus properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(publish_delivers);
}
TEST_MAIN_END()
