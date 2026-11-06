// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "power_manager.h"
#include <string.h>

/* ─── mock ────────────────────────────────────────────────────── */

static uint32_t g_tick_ms;

static uint32_t mock_tick(void *ctx) { (void)ctx; return g_tick_ms; }

static const power_manager_io_t g_io = {
    .get_tick_ms = mock_tick,
    .ctx         = NULL,
};

static power_manager_t g_pm;

static void reset(void)
{
    g_tick_ms = 0;
    memset(&g_pm, 0, sizeof(g_pm));
}

/* ─── init tests ─────────────────────────────────────────────── */

TEST(test_init_ok)
{
    reset();
    ASSERT_EQ(power_manager_init(&g_pm, &g_io), POWER_MANAGER_OK);
    ASSERT_EQ(power_manager_get_level(&g_pm), POWER_LEVEL_SHUTDOWN);
}

TEST(test_init_null)
{
    ASSERT_EQ(power_manager_init(NULL, &g_io), POWER_MANAGER_ERR_PARAM);
    ASSERT_EQ(power_manager_init(&g_pm, NULL), POWER_MANAGER_ERR_PARAM);
    power_manager_io_t bad = { .get_tick_ms = NULL, .ctx = NULL };
    ASSERT_EQ(power_manager_init(&g_pm, &bad), POWER_MANAGER_ERR_PARAM);
}

/* ─── register tests ─────────────────────────────────────────── */

TEST(test_register_ok)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    ASSERT_EQ(power_manager_register(&g_pm, "uart", POWER_LEVEL_ACTIVE, 1000, &id), POWER_MANAGER_OK);
    ASSERT_EQ(id, 0);
    ASSERT_EQ(power_manager_register(&g_pm, "radio", POWER_LEVEL_IDLE, 0, &id), POWER_MANAGER_OK);
    ASSERT_EQ(id, 1);
}

TEST(test_register_null)
{
    uint8_t id;
    ASSERT_EQ(power_manager_register(NULL, "x", POWER_LEVEL_ACTIVE, 0, &id), POWER_MANAGER_ERR_PARAM);
    reset();
    power_manager_init(&g_pm, &g_io);
    ASSERT_EQ(power_manager_register(&g_pm, "x", POWER_LEVEL_ACTIVE, 0, NULL), POWER_MANAGER_ERR_PARAM);
}

TEST(test_register_invalid_level)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    ASSERT_EQ(power_manager_register(&g_pm, "x", POWER_LEVEL_COUNT, 0, &id), POWER_MANAGER_ERR_PARAM);
    ASSERT_EQ(power_manager_register(&g_pm, "x", (power_level_t)99, 0, &id), POWER_MANAGER_ERR_PARAM);
}

TEST(test_register_overflow)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    for (int i = 0; i < POWER_MANAGER_MAX_VOTERS; i++) {
        ASSERT_EQ(power_manager_register(&g_pm, "v", POWER_LEVEL_IDLE, 0, &id), POWER_MANAGER_OK);
    }
    ASSERT_EQ(power_manager_register(&g_pm, "extra", POWER_LEVEL_IDLE, 0, &id), POWER_MANAGER_ERR_FULL);
}

/* ─── activity / release tests ───────────────────────────────── */

TEST(test_activity_release)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    power_manager_register(&g_pm, "uart", POWER_LEVEL_ACTIVE, 0, &id);

    ASSERT_FALSE(power_manager_is_active(&g_pm, id));
    ASSERT_EQ(power_manager_activity(&g_pm, id), POWER_MANAGER_OK);
    ASSERT_TRUE(power_manager_is_active(&g_pm, id));
    ASSERT_EQ(power_manager_active_count(&g_pm), 1);

    ASSERT_EQ(power_manager_release(&g_pm, id), POWER_MANAGER_OK);
    ASSERT_FALSE(power_manager_is_active(&g_pm, id));
    ASSERT_EQ(power_manager_active_count(&g_pm), 0);
}

TEST(test_activity_bad_id)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    ASSERT_EQ(power_manager_activity(&g_pm, 0), POWER_MANAGER_ERR_ID);
    ASSERT_EQ(power_manager_activity(&g_pm, 255), POWER_MANAGER_ERR_ID);
    ASSERT_EQ(power_manager_release(&g_pm, 0), POWER_MANAGER_ERR_ID);
}

TEST(test_activity_null)
{
    ASSERT_EQ(power_manager_activity(NULL, 0), POWER_MANAGER_ERR_ID);
    ASSERT_EQ(power_manager_release(NULL, 0), POWER_MANAGER_ERR_ID);
}

/* ─── poll / sleep level tests ───────────────────────────────── */

TEST(test_no_voters_deepest_sleep)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_SHUTDOWN);
}

TEST(test_one_active_voter_limits_sleep)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    power_manager_register(&g_pm, "uart", POWER_LEVEL_IDLE, 0, &id);
    power_manager_activity(&g_pm, id);

    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_IDLE);
}

TEST(test_most_restrictive_voter_wins)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id_uart, id_radio, id_sensor;
    power_manager_register(&g_pm, "uart", POWER_LEVEL_ACTIVE, 0, &id_uart);
    power_manager_register(&g_pm, "radio", POWER_LEVEL_LIGHT_SLEEP, 0, &id_radio);
    power_manager_register(&g_pm, "sensor", POWER_LEVEL_DEEP_SLEEP, 0, &id_sensor);

    power_manager_activity(&g_pm, id_radio);
    power_manager_activity(&g_pm, id_sensor);
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_LIGHT_SLEEP);

    power_manager_activity(&g_pm, id_uart);
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_ACTIVE);

    power_manager_release(&g_pm, id_uart);
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_LIGHT_SLEEP);

    power_manager_release(&g_pm, id_radio);
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_DEEP_SLEEP);

    power_manager_release(&g_pm, id_sensor);
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_SHUTDOWN);
}

TEST(test_inactive_voter_does_not_limit)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    power_manager_register(&g_pm, "uart", POWER_LEVEL_ACTIVE, 0, &id);
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_SHUTDOWN);
}

/* ─── timeout tests ──────────────────────────────────────────── */

TEST(test_activity_timeout_auto_release)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    power_manager_register(&g_pm, "uart", POWER_LEVEL_ACTIVE, 500, &id);

    g_tick_ms = 100;
    power_manager_activity(&g_pm, id);
    ASSERT_TRUE(power_manager_is_active(&g_pm, id));

    g_tick_ms = 599;
    power_manager_poll(&g_pm);
    ASSERT_TRUE(power_manager_is_active(&g_pm, id));

    g_tick_ms = 600;
    power_manager_poll(&g_pm);
    ASSERT_FALSE(power_manager_is_active(&g_pm, id));
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_SHUTDOWN);
}

TEST(test_activity_refresh_resets_timeout)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    power_manager_register(&g_pm, "uart", POWER_LEVEL_ACTIVE, 500, &id);

    g_tick_ms = 0;
    power_manager_activity(&g_pm, id);

    g_tick_ms = 400;
    power_manager_activity(&g_pm, id);

    g_tick_ms = 800;
    power_manager_poll(&g_pm);
    ASSERT_TRUE(power_manager_is_active(&g_pm, id));

    g_tick_ms = 900;
    power_manager_poll(&g_pm);
    ASSERT_FALSE(power_manager_is_active(&g_pm, id));
}

TEST(test_zero_timeout_never_expires)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    power_manager_register(&g_pm, "radio", POWER_LEVEL_IDLE, 0, &id);

    power_manager_activity(&g_pm, id);
    g_tick_ms = 999999;
    power_manager_poll(&g_pm);
    ASSERT_TRUE(power_manager_is_active(&g_pm, id));
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_IDLE);
}

/* ─── getter tests ───────────────────────────────────────────── */

TEST(test_get_level_null)
{
    ASSERT_EQ(power_manager_get_level(NULL), POWER_LEVEL_ACTIVE);
}

TEST(test_active_count_null)
{
    ASSERT_EQ(power_manager_active_count(NULL), 0);
}

TEST(test_is_active_null)
{
    ASSERT_FALSE(power_manager_is_active(NULL, 0));
}

TEST(test_voter_name)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    uint8_t id;
    power_manager_register(&g_pm, "uart_rx", POWER_LEVEL_ACTIVE, 0, &id);
    ASSERT_TRUE(strcmp(power_manager_voter_name(&g_pm, id), "uart_rx") == 0);
}

TEST(test_voter_name_bad_id)
{
    reset();
    power_manager_init(&g_pm, &g_io);
    ASSERT_TRUE(power_manager_voter_name(&g_pm, 0) == NULL);
    ASSERT_TRUE(power_manager_voter_name(NULL, 0) == NULL);
}

TEST(test_poll_null)
{
    ASSERT_EQ(power_manager_poll(NULL), POWER_LEVEL_ACTIVE);
}

/* ─── activity against the clock ─────────────────────────────── */

static uint8_t g_late_voter;
static bool    g_late_activity;

/* Activity arriving just after poll has read the clock: by then the tick has
 * moved on, so last_activity_ms is later than the t that poll works with. */
static uint32_t late_activity_tick(void *ctx)
{
    (void)ctx;
    uint32_t t = g_tick_ms;
    if (g_late_activity) {
        g_late_activity = false;
        g_tick_ms = t + 1;
        power_manager_activity(&g_pm, g_late_voter);
    }
    return t;
}

static const power_manager_io_t g_late_io = {
    .get_tick_ms = late_activity_tick,
    .ctx         = NULL,
};

TEST(test_activity_later_than_poll_does_not_expire)
{
    reset();
    power_manager_init(&g_pm, &g_late_io);
    power_manager_register(&g_pm, "uart", POWER_LEVEL_IDLE, 50, &g_late_voter);
    g_tick_ms = 1000;
    power_manager_activity(&g_pm, g_late_voter);

    g_tick_ms = 1010;
    g_late_activity = true;
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_IDLE);
    ASSERT_TRUE(power_manager_is_active(&g_pm, g_late_voter));

    /* and it still runs out, counted from that activity */
    g_tick_ms = 1060;
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_IDLE);
    g_tick_ms = 1061;
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_SHUTDOWN);
}

TEST(test_timeout_runs_across_the_tick_wrap)
{
    uint8_t id;

    reset();
    power_manager_init(&g_pm, &g_io);
    power_manager_register(&g_pm, "uart", POWER_LEVEL_IDLE, 50, &id);
    g_tick_ms = 0xFFFFFFF0u;
    power_manager_activity(&g_pm, id);

    g_tick_ms = 0x21u;  /* 49 ms on */
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_IDLE);
    g_tick_ms = 0x22u;  /* 50 ms on */
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_SHUTDOWN);
}

/* The longest timeout that can be told from an activity in the future. */
TEST(test_longest_timeout)
{
    uint8_t id;

    reset();
    power_manager_init(&g_pm, &g_io);
    ASSERT_EQ(power_manager_register(&g_pm, "a", POWER_LEVEL_IDLE,
                                     0x80000000u, &id),
              POWER_MANAGER_ERR_PARAM);
    ASSERT_EQ(power_manager_register(&g_pm, "a", POWER_LEVEL_IDLE,
                                     0x7FFFFFFFu, &id),
              POWER_MANAGER_OK);
    power_manager_activity(&g_pm, id);

    g_tick_ms = 0x7FFFFFFEu;
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_IDLE);
    g_tick_ms = 0x7FFFFFFFu;
    ASSERT_EQ(power_manager_poll(&g_pm), POWER_LEVEL_SHUTDOWN);
}

TEST_MAIN("power_manager")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_register_ok);
    RUN(test_register_null);
    RUN(test_register_invalid_level);
    RUN(test_register_overflow);
    RUN(test_activity_release);
    RUN(test_activity_bad_id);
    RUN(test_activity_null);
    RUN(test_no_voters_deepest_sleep);
    RUN(test_one_active_voter_limits_sleep);
    RUN(test_most_restrictive_voter_wins);
    RUN(test_inactive_voter_does_not_limit);
    RUN(test_activity_timeout_auto_release);
    RUN(test_activity_refresh_resets_timeout);
    RUN(test_zero_timeout_never_expires);
    RUN(test_get_level_null);
    RUN(test_active_count_null);
    RUN(test_is_active_null);
    RUN(test_voter_name);
    RUN(test_voter_name_bad_id);
    RUN(test_poll_null);
    RUN(test_activity_later_than_poll_does_not_expire);
    RUN(test_timeout_runs_across_the_tick_wrap);
    RUN(test_longest_timeout);
TEST_MAIN_END()
