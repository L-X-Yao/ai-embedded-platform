// SPDX-License-Identifier: MIT
// timer_retry — software timers with retry/backoff strategy
//
// Wheels: soft_timer, retry

#include "soft_timer.h"
#include "retry.h"
#include <stdio.h>
#include <string.h>

static int heartbeat_count;
static int sample_count;

static void on_heartbeat(void *ctx)
{
    (void)ctx;
    heartbeat_count++;
}

static void on_sample(void *ctx)
{
    (void)ctx;
    sample_count++;
}

static int simulate_unreliable_send(int attempt)
{
    return (attempt < 3) ? -1 : 0;
}

int main(void)
{
    printf("=== Timer + Retry Demo ===\n\n");

    /* --- Part 1: Software timers --- */
    soft_timer_mgr_t mgr;
    soft_timer_init(&mgr);

    int hb_id = soft_timer_create(&mgr, 10, SOFT_TIMER_REPEAT,
                                  on_heartbeat, NULL);
    int sp_id = soft_timer_create(&mgr, 25, SOFT_TIMER_REPEAT,
                                  on_sample, NULL);

    soft_timer_start(&mgr, hb_id);
    soft_timer_start(&mgr, sp_id);

    printf("[timers] running 100 ticks (heartbeat@10, sample@25)...\n");
    for (uint32_t t = 0; t < 100; t++) {
        soft_timer_tick(&mgr);
    }

    printf("[timers] heartbeat fired %d times (expect 10)\n", heartbeat_count);
    printf("[timers] sample   fired %d times (expect 4)\n", sample_count);

    if (heartbeat_count != 10 || sample_count != 4) {
        printf("FAIL: unexpected timer counts\n");
        return 1;
    }

    /* Stop and restart one-shot */
    soft_timer_stop(&mgr, hb_id);
    soft_timer_destroy(&mgr, sp_id);

    int oneshot_id = soft_timer_create(&mgr, 5, SOFT_TIMER_ONE_SHOT,
                                       on_sample, NULL);
    soft_timer_start(&mgr, oneshot_id);
    sample_count = 0;
    for (uint32_t t = 0; t < 20; t++) {
        soft_timer_tick(&mgr);
    }
    printf("[timers] one-shot fired %d time (expect 1)\n", sample_count);
    if (sample_count != 1) {
        printf("FAIL: one-shot fired wrong number of times\n");
        return 1;
    }

    /* --- Part 2: Retry with exponential backoff --- */
    printf("\n[retry] simulating unreliable network send...\n");

    retry_config_t cfg = {
        .strategy       = RETRY_EXPONENTIAL,
        .base_delay_ms  = 100,
        .max_delay_ms   = 5000,
        .max_attempts   = 5,
    };
    retry_t retry;
    retry_init(&retry, &cfg);

    int success = 0;
    while (retry_should_retry(&retry)) {
        uint8_t  attempt = retry_get_attempt(&retry);
        uint32_t delay   = retry_get_delay(&retry);
        int      rc      = simulate_unreliable_send(attempt);

        printf("  attempt %d: delay=%ums result=%s\n",
               attempt, (unsigned)delay, rc == 0 ? "OK" : "FAIL");

        if (rc == 0) {
            success = 1;
            break;
        }
    }

    if (!success) {
        printf("FAIL: all retries exhausted\n");
        return 1;
    }
    printf("[retry] succeeded on attempt %d\n", retry_get_attempt(&retry));

    /* Test fixed strategy */
    retry_config_t fixed_cfg = {
        .strategy       = RETRY_FIXED,
        .base_delay_ms  = 500,
        .max_delay_ms   = 500,
        .max_attempts   = 3,
    };
    retry_init(&retry, &fixed_cfg);

    printf("\n[retry] fixed strategy (500ms, 3 attempts):\n");
    while (retry_should_retry(&retry)) {
        printf("  attempt %d: delay=%ums\n",
               retry_get_attempt(&retry), (unsigned)retry_get_delay(&retry));
    }

    printf("\n=== PASSED ===\n");
    return 0;
}
