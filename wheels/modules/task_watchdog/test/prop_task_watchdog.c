// SPDX-License-Identifier: MIT
/*
 * prop_task_watchdog.c — property-based tests for task_watchdog
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_task_watchdog.c ../task_watchdog.c -o prop_task_watchdog
 */

#include "test_harness.h"
#include "property_test.h"
#include "../task_watchdog.h"
#include <string.h>

static int expired_count;
static void on_expired(void *ctx, const char *name, uint32_t last_feed) {
    (void)ctx; (void)name; (void)last_feed;
    expired_count++;
}

/* Property: random register/feed/tick/check sequences never crash */
PROPERTY(random_ops_no_crash, 10000) {
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    expired_count = 0;
    task_watchdog_set_callback(&wd, on_expired, NULL);

    static const char *names[] = {"t1", "t2", "t3", "t4"};
    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 40);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 6;
        const char *name = names[ARBI_UINT8(seed) % 4];
        if (op == 0) {
            task_watchdog_register(&wd, name, (uint32_t)ARBI_RANGE(seed, 1, 100));
        } else if (op == 1) {
            task_watchdog_feed(&wd, name);
        } else if (op == 2) {
            task_watchdog_tick(&wd);
        } else if (op == 3) {
            task_watchdog_check(&wd);
        } else if (op == 4) {
            task_watchdog_enable(&wd, name);
        } else {
            task_watchdog_disable(&wd, name);
        }
    }

    uint8_t count = task_watchdog_count(&wd);
    PROP_ASSERT(count <= TASK_WD_MAX_TASKS);
}

/* Property: init is idempotent */
PROPERTY(init_idempotent, 5000) {
    task_watchdog_t wd1, wd2;
    task_watchdog_init(&wd1);
    task_watchdog_init(&wd2);
    task_watchdog_init(&wd2);

    PROP_ASSERT_EQ(task_watchdog_count(&wd1), task_watchdog_count(&wd2));
}

/* Property: registered task count never exceeds maximum */
PROPERTY(count_bounded, 10000) {
    task_watchdog_t wd;
    task_watchdog_init(&wd);

    char names[TASK_WD_MAX_TASKS + 4][4];
    for (int i = 0; i < TASK_WD_MAX_TASKS + 4; i++) {
        names[i][0] = 't';
        names[i][1] = (char)('0' + (i / 10));
        names[i][2] = (char)('0' + (i % 10));
        names[i][3] = '\0';
        task_watchdog_register(&wd, names[i], 10);
    }

    PROP_ASSERT(task_watchdog_count(&wd) <= TASK_WD_MAX_TASKS);
}

/* Property: status is always a valid enum after random ops */
PROPERTY(status_valid, 5000) {
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, on_expired, NULL);
    task_watchdog_register(&wd, "probe", (uint32_t)ARBI_RANGE(seed, 1, 50));

    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 0, 20);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 4;
        if (op == 0) task_watchdog_tick(&wd);
        else if (op == 1) task_watchdog_check(&wd);
        else if (op == 2) task_watchdog_feed(&wd, "probe");
        else task_watchdog_disable(&wd, "probe");
    }

    task_wd_status_t st = task_watchdog_status(&wd, "probe");
    PROP_ASSERT(st == TASK_WD_STATUS_OK ||
                st == TASK_WD_STATUS_EXPIRED ||
                st == TASK_WD_STATUS_DISABLED);
}

TEST_MAIN("task_watchdog properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(count_bounded);
    RUN(status_valid);
}
TEST_MAIN_END()
