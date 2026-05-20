// SPDX-License-Identifier: MIT
/*
 * prop_gnss_filter.c — property-based tests for gnss_filter
 *
 * Build: add to test/CMakeLists.txt or compile standalone:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_gnss_filter.c ../gnss_filter.c -o prop_gnss_filter -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../gnss_filter.h"
#include <math.h>

static const gnss_filter_cfg_t g_cfg = {
    .min_satellites = 6, .max_hdop = 2.0f, .max_vdop = 4.0f,
    .stable_ms = 5000, .static_alpha = 0.2f, .strong_alpha = 0.05f,
    .static_speed_cms = 100, .zupt_motion_mg = 50,
};

/* Suppress unused warning for ARBI_FILL in property_test.h */
static void prop__dummy_ref(void) { (void)prop__arbi_fill; }

static gnss_filter_input_t random_good_fix(uint32_t *seed) {
    (void)prop__dummy_ref;
    gnss_filter_input_t in = {0};
    in.latitude    = (float)ARBI_RANGE(*seed, -90, 90);
    in.longitude   = (float)ARBI_RANGE(*seed, -180, 180);
    in.altitude    = (float)ARBI_RANGE(*seed, 0, 10000);
    in.speed_cms   = 0;
    in.satellites  = (uint8_t)ARBI_RANGE(*seed, 8, 24);
    in.fix_mode    = 2;
    in.fix_quality = 1;
    in.hdop        = 0.5f + (float)(ARBI_UINT8(*seed) % 10) * 0.1f;
    in.vdop        = 0.5f + (float)(ARBI_UINT8(*seed) % 20) * 0.1f;
    in.fix_valid   = true;
    return in;
}

/* Property: filter output is always within input domain bounds */
PROPERTY(output_bounded, 5000) {
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_cfg);

    gnss_filter_input_t in = random_good_fix(&seed);
    for (uint32_t t = 0; t <= 6000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }

    if (gnss_filter_is_ready(&flt)) {
        float lat = gnss_filter_lat(&flt);
        float lon = gnss_filter_lon(&flt);
        float alt = gnss_filter_alt(&flt);
        PROP_ASSERT(lat >= -90.0f && lat <= 90.0f);
        PROP_ASSERT(lon >= -180.0f && lon <= 180.0f);
        PROP_ASSERT(alt >= -1000.0f && alt <= 100000.0f);
    }
}

/* Property: EMA converges — after many identical inputs, output ≈ input */
PROPERTY(ema_converges, 5000) {
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_cfg);

    gnss_filter_input_t in = random_good_fix(&seed);
    in.speed_cms = 0;

    for (uint32_t t = 0; t <= 30000; t += 200) {
        gnss_filter_update(&flt, &in, 10, t);
    }

    if (gnss_filter_is_ready(&flt)) {
        PROP_ASSERT(fabsf(gnss_filter_lat(&flt) - in.latitude) < 0.1f);
        PROP_ASSERT(fabsf(gnss_filter_lon(&flt) - in.longitude) < 0.1f);
        PROP_ASSERT(fabsf(gnss_filter_alt(&flt) - in.altitude) < 1.0f);
    }
}

/* Property: reset always returns to not-ready state */
PROPERTY(reset_clears, 5000) {
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_cfg);

    gnss_filter_input_t in = random_good_fix(&seed);
    int updates = ARBI_RANGE(seed, 1, 50);
    for (int i = 0; i < updates; i++) {
        gnss_filter_update(&flt, &in, 0, (uint32_t)(i * 200));
    }

    gnss_filter_reset(&flt);
    PROP_ASSERT(!gnss_filter_is_ready(&flt));
    PROP_ASSERT(fabsf(gnss_filter_lat(&flt)) < 0.001f);
}

/* Property: bad fix never makes filter ready */
PROPERTY(bad_fix_blocks, 10000) {
    gnss_filter_t flt;
    gnss_filter_init(&flt, &g_cfg);

    gnss_filter_input_t in = random_good_fix(&seed);
    /* Randomly degrade one quality field */
    int degrade = ARBI_RANGE(seed, 0, 4);
    switch (degrade) {
        case 0: in.fix_valid = false; break;
        case 1: in.fix_mode = 0; break;
        case 2: in.fix_quality = 0; break;
        case 3: in.satellites = 2; break;
        case 4: in.hdop = 10.0f; break;
        default: break;
    }

    for (uint32_t t = 0; t <= 20000; t += 200) {
        gnss_filter_update(&flt, &in, 0, t);
    }
    PROP_ASSERT(!gnss_filter_is_ready(&flt));
}

/* Property: gnss_datetime_to_unix is monotonic with increasing dates */
PROPERTY(datetime_monotonic, 10000) {
    uint16_t y1 = (uint16_t)ARBI_RANGE(seed, 1970, 2099);
    uint8_t m1 = (uint8_t)ARBI_RANGE(seed, 1, 12);
    uint8_t d1 = (uint8_t)ARBI_RANGE(seed, 1, 28);
    uint8_t h1 = (uint8_t)ARBI_RANGE(seed, 0, 23);

    int64_t ts1 = gnss_datetime_to_unix(y1, m1, d1, h1, 0, 0);
    int64_t ts2 = gnss_datetime_to_unix(y1, m1, d1, h1, 0, 1);
    PROP_ASSERT(ts2 == ts1 + 1);

    int64_t ts3 = gnss_datetime_to_unix(y1, m1, d1, h1 < 23 ? h1 + 1 : h1, 0, 0);
    if (h1 < 23) {
        PROP_ASSERT(ts3 == ts1 + 3600);
    }
}

TEST_MAIN("gnss_filter properties")
    RUN(output_bounded);
    RUN(ema_converges);
    RUN(reset_clears);
    RUN(bad_fix_blocks);
    RUN(datetime_monotonic);
TEST_MAIN_END()
