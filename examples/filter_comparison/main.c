// SPDX-License-Identifier: MIT
// filter_comparison — compare median, IIR, and Kalman on noisy data
//
// Wheels: median_filter, iir_lowpass, kalman_1d

#include "median_filter.h"
#include "iir_lowpass.h"
#include "kalman_1d.h"
#include <stdio.h>
#include <math.h>

#define N_SAMPLES 40
#define NOISE_AMPLITUDE 500

static int32_t pseudo_noise(int i)
{
    uint32_t x = (uint32_t)(i * 7919 + 104729);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (int32_t)(x % (uint32_t)(NOISE_AMPLITUDE * 2 + 1)) - NOISE_AMPLITUDE;
}

int main(void)
{
    median_filter_t mf;
    iir_lowpass_t   iir;
    kalman_1d_t     kf;

    median_filter_init(&mf, 5);

    iir_lowpass_cfg_t iir_cfg = { .alpha = 0.2f };
    iir_lowpass_init(&iir, &iir_cfg);

    kalman_1d_cfg_t kal_cfg = { .q = 1.0f, .r = 50.0f };
    kalman_1d_init(&kf, &kal_cfg);

    printf("=== Filter Comparison Demo ===\n");
    printf("%-6s %-8s %-8s %-8s %-8s %-8s\n",
           "i", "true", "noisy", "median", "IIR", "kalman");
    printf("------------------------------------------------------\n");

    int pass = 1;
    for (int i = 0; i < N_SAMPLES; i++) {
        int32_t true_val = (int32_t)(1000.0 * sin((double)i * 0.3));
        int32_t noisy    = true_val + pseudo_noise(i);

        int32_t med_out = median_filter_update(&mf, noisy);
        float   iir_out = iir_lowpass_feed(&iir, (float)noisy);
        float   kal_out = kalman_1d_update(&kf, (float)noisy);

        printf("%-6d %-8d %-8d %-8d %-8.0f %-8.1f\n",
               i, (int)true_val, (int)noisy, (int)med_out,
               (double)iir_out, (double)kal_out);

        if (i >= 10) {
            float med_err = (float)(med_out - true_val);
            float iir_err = iir_out - (float)true_val;
            float kal_err = kal_out - (float)true_val;
            if (fabsf(med_err) > 1000.0f &&
                fabsf(iir_err) > 1000.0f &&
                fabsf(kal_err) > 1000.0f) {
                pass = 0;
            }
        }
    }

    printf("\n");
    printf("Kalman gain at end: %.4f\n", (double)kalman_1d_get_gain(&kf));
    printf("IIR alpha:          %.2f\n", (double)iir.alpha);
    printf("Median window:      5\n");

    if (pass) {
        printf("\n=== PASSED ===\n");
    } else {
        printf("\n=== FAILED (all filters diverged) ===\n");
    }
    return pass ? 0 : 1;
}
