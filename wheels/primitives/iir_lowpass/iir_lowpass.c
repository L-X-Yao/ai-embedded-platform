// SPDX-License-Identifier: MIT
/*
 * iir_lowpass — First-order IIR low-pass filter
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 * Instances: multi
 */

#include "iir_lowpass.h"

int iir_lowpass_init(iir_lowpass_t *f, const iir_lowpass_cfg_t *cfg)
{
    if (!f || !cfg)
    {
        return IIR_LOWPASS_ERR_PARAM;
    }
    /* NaN-safe guard: rejects 0, negative, NaN, and values > 1 */
    if (!(cfg->alpha > 0.0f && cfg->alpha <= 1.0f))
    {
        return IIR_LOWPASS_ERR_PARAM;
    }
    f->alpha  = cfg->alpha;
    f->output = 0.0f;
    f->primed = 0U;
    return IIR_LOWPASS_OK;
}

float iir_lowpass_feed(iir_lowpass_t *f, float x)
{
    if (!f)
    {
        return 0.0f;
    }
    if (!f->primed)
    {
        f->output = x;
        f->primed = 1U;
    }
    else
    {
        f->output = f->alpha * x + (1.0f - f->alpha) * f->output;
    }
    return f->output;
}

float iir_lowpass_get(const iir_lowpass_t *f)
{
    if (!f)
    {
        return 0.0f;
    }
    return f->output;
}

void iir_lowpass_reset(iir_lowpass_t *f)
{
    if (!f)
    {
        return;
    }
    f->output = 0.0f;
    f->primed = 0U;
}

int iir_lowpass_set_alpha(iir_lowpass_t *f, float alpha)
{
    if (!f)
    {
        return IIR_LOWPASS_ERR_PARAM;
    }
    /* NaN-safe guard */
    if (!(alpha > 0.0f && alpha <= 1.0f))
    {
        return IIR_LOWPASS_ERR_PARAM;
    }
    f->alpha = alpha;
    return IIR_LOWPASS_OK;
}

float iir_lowpass_alpha_from_rc(float dt, float rc)
{
    /* Both must be strictly positive; NaN-safe guard */
    if (!(dt > 0.0f) || !(rc > 0.0f))
    {
        return 0.0f;
    }
    return dt / (rc + dt);
}
