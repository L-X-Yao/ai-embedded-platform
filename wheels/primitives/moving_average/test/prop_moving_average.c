/*
 * prop_moving_average.c — property-based tests for moving_average
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_moving_average.c ../moving_average.c -o prop_moving_average -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "../moving_average.h"
#include <math.h>

#define MAX_WINDOW 16

/* Property: SMA output always within [min_input, max_input] */
PROPERTY(sma_output_bounded, 10000) {
    float buf[MAX_WINDOW];
    moving_average_sma_t sma;
    size_t window = 1 + ARBI_SIZE(seed, MAX_WINDOW - 1);
    moving_average_sma_init(&sma, buf, window);

    float lo = 1e30f, hi = -1e30f;
    unsigned n_samples = (unsigned)ARBI_RANGE(seed, 1, 50);

    for (unsigned i = 0; i < n_samples; i++) {
        /* Generate values in [-1000, 1000] */
        float sample = (float)ARBI_RANGE(seed, -1000, 1000);
        if (sample < lo) lo = sample;
        if (sample > hi) hi = sample;
        float result = moving_average_sma_feed(&sma, sample);

        /* After feeding, result must be within [lo, hi] */
        PROP_ASSERT(result >= lo - 0.01f);
        PROP_ASSERT(result <= hi + 0.01f);
    }
}

/* Property: EMA output always within [min_input, max_input] */
PROPERTY(ema_output_bounded, 10000) {
    moving_average_ema_t ema;
    float alpha = 0.01f + (float)(ARBI_RANGE(seed, 1, 99)) / 100.0f;
    moving_average_ema_init(&ema, alpha);

    float lo = 1e30f, hi = -1e30f;
    unsigned n_samples = (unsigned)ARBI_RANGE(seed, 1, 50);

    for (unsigned i = 0; i < n_samples; i++) {
        float sample = (float)ARBI_RANGE(seed, -1000, 1000);
        if (sample < lo) lo = sample;
        if (sample > hi) hi = sample;
        float result = moving_average_ema_feed(&ema, sample);

        PROP_ASSERT(result >= lo - 0.01f);
        PROP_ASSERT(result <= hi + 0.01f);
    }
}

/* Property: deterministic — same input sequence produces same output */
PROPERTY(sma_deterministic, 5000) {
    float buf1[MAX_WINDOW], buf2[MAX_WINDOW];
    moving_average_sma_t sma1, sma2;
    size_t window = 1 + ARBI_SIZE(seed, MAX_WINDOW - 1);
    moving_average_sma_init(&sma1, buf1, window);
    moving_average_sma_init(&sma2, buf2, window);

    /* Save seed state to replay */
    uint32_t saved_seed = seed;
    unsigned n_samples = (unsigned)ARBI_RANGE(seed, 1, 30);
    uint32_t seq_seed = seed;

    /* First run */
    float results1[30];
    uint32_t s1 = seq_seed;
    for (unsigned i = 0; i < n_samples; i++) {
        float sample = (float)ARBI_RANGE(s1, -500, 500);
        results1[i] = moving_average_sma_feed(&sma1, sample);
    }

    /* Second run with same seed sequence */
    float results2[30];
    uint32_t s2 = seq_seed;
    for (unsigned i = 0; i < n_samples; i++) {
        float sample = (float)ARBI_RANGE(s2, -500, 500);
        results2[i] = moving_average_sma_feed(&sma2, sample);
    }

    for (unsigned i = 0; i < n_samples; i++) {
        PROP_ASSERT(results1[i] == results2[i]);
    }
    (void)saved_seed;
}

/* Property: SMA of constant value equals that constant */
PROPERTY(sma_constant_input, 5000) {
    float buf[MAX_WINDOW];
    moving_average_sma_t sma;
    size_t window = 1 + ARBI_SIZE(seed, MAX_WINDOW - 1);
    moving_average_sma_init(&sma, buf, window);

    float constant = (float)ARBI_RANGE(seed, -1000, 1000);

    /* Feed the constant enough times to fill the window */
    float result = 0.0f;
    for (size_t i = 0; i < window + 5; i++) {
        result = moving_average_sma_feed(&sma, constant);
    }

    /* After window is full of the same value, average == value */
    float diff = result - constant;
    PROP_ASSERT(diff > -0.1f && diff < 0.1f);
}

TEST_MAIN("moving_average properties") {
    RUN(sma_output_bounded);
    RUN(ema_output_bounded);
    RUN(sma_deterministic);
    RUN(sma_constant_input);
}
TEST_MAIN_END()
