// SPDX-License-Identifier: MIT
#include "peak_detector.h"

peak_detector_err_t peak_detector_init(peak_detector_t *ctx, float decay_rate)
{
    if (!ctx)
        return PEAK_DETECTOR_ERR_PARAM;
    if (!(decay_rate >= 0.0f && decay_rate <= 1.0f))
        return PEAK_DETECTOR_ERR_PARAM;
    ctx->peak = 0.0f;
    ctx->valley = 0.0f;
    ctx->decay_rate = decay_rate;
    ctx->primed = false;
    return PEAK_DETECTOR_OK;
}

peak_detector_err_t peak_detector_feed(peak_detector_t *ctx, float sample)
{
    if (!ctx)
        return PEAK_DETECTOR_ERR_PARAM;
    if (!(sample == sample))
        return PEAK_DETECTOR_ERR_PARAM;

    if (!ctx->primed) {
        ctx->peak = sample;
        ctx->valley = sample;
        ctx->primed = true;
        return PEAK_DETECTOR_OK;
    }

    if (sample > ctx->peak) {
        ctx->peak = sample;
    } else if (ctx->decay_rate > 0.0f) {
        ctx->peak -= (ctx->peak - sample) * ctx->decay_rate;
    }

    if (sample < ctx->valley) {
        ctx->valley = sample;
    } else if (ctx->decay_rate > 0.0f) {
        ctx->valley += (sample - ctx->valley) * ctx->decay_rate;
    }

    return PEAK_DETECTOR_OK;
}

float peak_detector_get_peak(const peak_detector_t *ctx)
{
    if (!ctx || !ctx->primed)
        return 0.0f;
    return ctx->peak;
}

float peak_detector_get_valley(const peak_detector_t *ctx)
{
    if (!ctx || !ctx->primed)
        return 0.0f;
    return ctx->valley;
}

float peak_detector_get_range(const peak_detector_t *ctx)
{
    if (!ctx || !ctx->primed)
        return 0.0f;
    return ctx->peak - ctx->valley;
}

peak_detector_err_t peak_detector_reset(peak_detector_t *ctx)
{
    if (!ctx)
        return PEAK_DETECTOR_ERR_PARAM;
    ctx->peak = 0.0f;
    ctx->valley = 0.0f;
    ctx->primed = false;
    return PEAK_DETECTOR_OK;
}
