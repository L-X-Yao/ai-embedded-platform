// SPDX-License-Identifier: MIT
/*
 * kalman_1d — 1D scalar Kalman filter
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Predict:
 *   x_pred = x_est         (state transition = 1, no control input)
 *   P_pred = P_est + Q
 *
 * Update:
 *   K      = P_pred / (P_pred + R)
 *   x_est  = x_pred + K * (z - x_pred)
 *   P_est  = (1 - K) * P_pred
 *
 * First call bootstraps: x = z, P = 1.0, K = 1.0.
 * Bootstrap is signalled by p == 0.0f after init (p is always > 0 after
 * the first update).
 *
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#include "kalman_1d.h"
#include "contract.h"
#include <string.h>

kalman_1d_err_t kalman_1d_init(kalman_1d_t *kf, const kalman_1d_cfg_t *cfg)
{
    if (!kf || !cfg)
    {
        return KALMAN_1D_ERR_PARAM;
    }
    /* NaN-safe positive guard: rejects 0, negative, NaN, and -0.0f */
    if (!(cfg->q > 0.0f) || !(cfg->r > 0.0f))
    {
        return KALMAN_1D_ERR_PARAM;
    }
    memset(kf, 0, sizeof(*kf));
    kf->q = cfg->q;
    kf->r = cfg->r;
    /* p == 0.0f signals "not yet bootstrapped" */
    ENSURE(kf->p == 0.0f);
    ENSURE(kf->q > 0.0f);
    ENSURE(kf->r > 0.0f);
    return KALMAN_1D_OK;
}

float kalman_1d_update(kalman_1d_t *kf, float z)
{
    if (!kf)
    {
        return 0.0f;
    }
    /* NaN-safe guard: reject NaN measurement (api-conventions §2.5).
     * A NaN z would propagate into kf->x, permanently corrupting state. */
    if (!(z == z))
    {
        return kf->x;
    }

    /* Bootstrap on first call: trust measurement fully */
    if (kf->p == 0.0f)
    {
        kf->x = z;
        kf->p = 1.0f;
        kf->k = 1.0f;
        return kf->x;
    }

    /* Predict */
    float p_pred = kf->p + kf->q;

    /* Update */
    kf->k = p_pred / (p_pred + kf->r);
    kf->x = kf->x + kf->k * (z - kf->x);
    kf->p = (1.0f - kf->k) * p_pred;

    INVARIANT(kf->p > 0.0f);
    INVARIANT(kf->k >= 0.0f && kf->k <= 1.0f);
    return kf->x;
}

float kalman_1d_get(const kalman_1d_t *kf)
{
    return kf ? kf->x : 0.0f;
}

float kalman_1d_get_gain(const kalman_1d_t *kf)
{
    return kf ? kf->k : 0.0f;
}

void kalman_1d_reset(kalman_1d_t *kf)
{
    if (!kf)
    {
        return;
    }
    memset(kf, 0, sizeof(*kf));
}

kalman_1d_err_t kalman_1d_set_noise(kalman_1d_t *kf, float q, float r)
{
    if (!kf)
    {
        return KALMAN_1D_ERR_PARAM;
    }
    if (!(q > 0.0f) || !(r > 0.0f))
    {
        return KALMAN_1D_ERR_PARAM;
    }
    kf->q = q;
    kf->r = r;
    return KALMAN_1D_OK;
}
