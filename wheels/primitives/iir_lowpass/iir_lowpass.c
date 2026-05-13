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
#include "contract.h"

iir_lowpass_err_t iir_lowpass_init(iir_lowpass_t *f, const iir_lowpass_cfg_t *cfg)
{
    REQUIRE(f != NULL);
    REQUIRE(cfg != NULL);
    if (!f || !cfg)
    {
        return IIR_LOWPASS_ERR_PARAM;
    }
    /* NaN-safe guard: rejects 0, negative, NaN, and values > 1 */
    REQUIRE(cfg->alpha > 0.0f && cfg->alpha <= 1.0f);
    if (!(cfg->alpha > 0.0f && cfg->alpha <= 1.0f))
    {
        return IIR_LOWPASS_ERR_PARAM;
    }
    f->alpha  = cfg->alpha;
    f->output = 0.0f;
    f->primed = 0U;
    ENSURE(f->primed == 0U);
    return IIR_LOWPASS_OK;
}

float iir_lowpass_feed(iir_lowpass_t *f, float x)
{
    REQUIRE(f != NULL);
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
    REQUIRE(f != NULL);
    if (!f)
    {
        return 0.0f;
    }
    return f->output;
}

void iir_lowpass_reset(iir_lowpass_t *f)
{
    REQUIRE(f != NULL);
    if (!f)
    {
        return;
    }
    f->output = 0.0f;
    f->primed = 0U;
    ENSURE(f->primed == 0U);
}

iir_lowpass_err_t iir_lowpass_set_alpha(iir_lowpass_t *f, float alpha)
{
    REQUIRE(f != NULL);
    if (!f)
    {
        return IIR_LOWPASS_ERR_PARAM;
    }
    /* NaN-safe guard */
    REQUIRE(alpha > 0.0f && alpha <= 1.0f);
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
    REQUIRE(dt > 0.0f);
    REQUIRE(rc > 0.0f);
    if (!(dt > 0.0f) || !(rc > 0.0f))
    {
        return 0.0f;
    }
    return dt / (rc + dt);
}
