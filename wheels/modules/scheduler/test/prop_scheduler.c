/*
 * prop_scheduler.c — property-based tests for scheduler
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_scheduler.c ../scheduler.c -o prop_scheduler
 */

#include "test_harness.h"
#include "property_test.h"
#include "../scheduler.h"
#include <string.h>

static int task_run_count;
static void mock_task(void *ctx) {
    (void)ctx;
    task_run_count++;
}

/* Property: random add/tick/run/enable/disable sequences never crash */
PROPERTY(random_ops_no_crash, 10000) {
    scheduler_t s;
    scheduler_init(&s);
    task_run_count = 0;

    static const char *names[] = {"a", "b", "c", "d"};
    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 1, 40);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 6;
        const char *name = names[ARBI_UINT8(seed) % 4];
        if (op == 0) {
            scheduler_add(&s, name, mock_task, NULL,
                          (uint32_t)ARBI_RANGE(seed, 0, 100));
        } else if (op == 1) {
            scheduler_tick(&s);
        } else if (op == 2) {
            scheduler_run(&s);
        } else if (op == 3) {
            scheduler_enable(&s, name);
        } else if (op == 4) {
            scheduler_disable(&s, name);
        } else {
            scheduler_remove(&s, name);
        }
    }

    uint8_t count = scheduler_count(&s);
    PROP_ASSERT(count <= SCHED_MAX_TASKS);
}

/* Property: init twice == init once */
PROPERTY(init_idempotent, 5000) {
    scheduler_t s1, s2;
    scheduler_init(&s1);
    scheduler_init(&s2);
    scheduler_init(&s2);

    PROP_ASSERT_EQ(scheduler_count(&s1), scheduler_count(&s2));
}

/* Property: add task increases count, remove decreases it */
PROPERTY(add_remove_count, 10000) {
    scheduler_t s;
    scheduler_init(&s);

    uint8_t before = scheduler_count(&s);
    PROP_ASSERT_EQ(before, 0);

    scheduler_add(&s, "t1", mock_task, NULL, 10);
    PROP_ASSERT_EQ(scheduler_count(&s), 1);

    scheduler_add(&s, "t2", mock_task, NULL, 20);
    PROP_ASSERT_EQ(scheduler_count(&s), 2);

    scheduler_remove(&s, "t1");
    PROP_ASSERT_EQ(scheduler_count(&s), 1);

    scheduler_remove(&s, "t2");
    PROP_ASSERT_EQ(scheduler_count(&s), 0);
}

TEST_MAIN("scheduler properties") {
    RUN(random_ops_no_crash);
    RUN(init_idempotent);
    RUN(add_remove_count);
}
TEST_MAIN_END()
