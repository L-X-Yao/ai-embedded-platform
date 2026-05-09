/*
 * prop_power_manager.c -- property-based tests for power_manager
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_power_manager.c ../power_manager.c -o prop_power_manager
 */

#include "test_harness.h"
#include "property_test.h"
#include <string.h>
#include "../power_manager.h"

/* --- mock tick --- */

static uint32_t g_tick_ms;

static uint32_t mock_tick(void *ctx)
{
    (void)ctx;
    return g_tick_ms;
}

static const power_manager_io_t g_io = {
    .get_tick_ms = mock_tick,
    .ctx         = NULL,
};

/* --- init_no_crash: init never crashes --- */
PROPERTY(init_no_crash, 5000) {
    (void)seed;
    power_manager_t pm;
    memset(&pm, 0, sizeof(pm));
    power_manager_err_t err = power_manager_init(&pm, &g_io);
    PROP_ASSERT(err == POWER_MANAGER_OK);
}

/* --- double_init_safe: calling init twice resets cleanly --- */
PROPERTY(double_init_safe, 5000) {
    (void)seed;
    power_manager_t pm;
    memset(&pm, 0, sizeof(pm));
    power_manager_init(&pm, &g_io);

    /* Register some voters */
    uint8_t id;
    power_manager_register(&pm, "v1", POWER_LEVEL_ACTIVE, 0, &id);

    /* Re-init should reset */
    power_manager_init(&pm, &g_io);
    PROP_ASSERT_EQ(power_manager_active_count(&pm), 0);
    PROP_ASSERT(power_manager_poll(&pm) == POWER_LEVEL_SHUTDOWN);
}

/* --- most_restrictive_wins: the shallowest active voter limits sleep --- */
PROPERTY(most_restrictive_wins, 10000) {
    g_tick_ms = 0;
    power_manager_t pm;
    memset(&pm, 0, sizeof(pm));
    power_manager_init(&pm, &g_io);

    /* Register random number of voters with random levels */
    int n = ARBI_RANGE(seed, 1, POWER_MANAGER_MAX_VOTERS);
    uint8_t ids[POWER_MANAGER_MAX_VOTERS];
    power_level_t levels[POWER_MANAGER_MAX_VOTERS];

    for (int i = 0; i < n; i++) {
        levels[i] = (power_level_t)ARBI_RANGE(seed, 0, (int)POWER_LEVEL_SHUTDOWN - 1);
        power_manager_register(&pm, "v", levels[i], 0, &ids[i]);
    }

    /* Activate all voters */
    for (int i = 0; i < n; i++) {
        power_manager_activity(&pm, ids[i]);
    }

    power_level_t result = power_manager_poll(&pm);

    /* Expected: minimum level among all active voters */
    power_level_t expected = POWER_LEVEL_SHUTDOWN;
    for (int i = 0; i < n; i++) {
        if (levels[i] < expected) expected = levels[i];
    }
    PROP_ASSERT_EQ(result, expected);
}

/* --- release_all_yields_deepest: releasing all voters gives SHUTDOWN --- */
PROPERTY(release_all_yields_deepest, 5000) {
    g_tick_ms = 0;
    power_manager_t pm;
    memset(&pm, 0, sizeof(pm));
    power_manager_init(&pm, &g_io);

    int n = ARBI_RANGE(seed, 1, POWER_MANAGER_MAX_VOTERS);
    uint8_t ids[POWER_MANAGER_MAX_VOTERS];

    for (int i = 0; i < n; i++) {
        power_level_t lvl = (power_level_t)ARBI_RANGE(seed, 0, (int)POWER_LEVEL_SHUTDOWN - 1);
        power_manager_register(&pm, "v", lvl, 0, &ids[i]);
        power_manager_activity(&pm, ids[i]);
    }

    /* Release all */
    for (int i = 0; i < n; i++) {
        power_manager_release(&pm, ids[i]);
    }

    PROP_ASSERT(power_manager_poll(&pm) == POWER_LEVEL_SHUTDOWN);
    PROP_ASSERT_EQ(power_manager_active_count(&pm), 0);
}

/* --- timeout_auto_releases: timed-out voters do not block sleep --- */
PROPERTY(timeout_auto_releases, 5000) {
    g_tick_ms = 0;
    power_manager_t pm;
    memset(&pm, 0, sizeof(pm));
    power_manager_init(&pm, &g_io);

    uint32_t timeout = (uint32_t)ARBI_RANGE(seed, 100, 10000);
    uint8_t id;
    power_manager_register(&pm, "timed", POWER_LEVEL_ACTIVE, timeout, &id);
    power_manager_activity(&pm, id);

    /* Before timeout: voter is active */
    g_tick_ms = timeout - 1;
    power_manager_poll(&pm);
    PROP_ASSERT(power_manager_is_active(&pm, id));

    /* After timeout: voter auto-released */
    g_tick_ms = timeout;
    power_manager_poll(&pm);
    PROP_ASSERT(!power_manager_is_active(&pm, id));
}

TEST_MAIN("power_manager properties") {
    RUN(init_no_crash);
    RUN(double_init_safe);
    RUN(most_restrictive_wins);
    RUN(release_all_yields_deepest);
    RUN(timeout_auto_releases);
}
TEST_MAIN_END()
