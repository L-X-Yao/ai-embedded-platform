/*
 * led_pattern — LED blink pattern engine
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */
#include "led_pattern.h"

void led_pattern_init(led_pattern_t *p, const led_pattern_cfg_t *cfg)
{
    if (!p || !cfg) return;
    p->cfg = *cfg;
    p->current_step = 0;
    p->tick_in_step = 0;
    p->is_on = (cfg->step_count > 0 && cfg->steps && cfg->steps[0].on_ticks > 0);
    p->finished = (cfg->step_count == 0);
}

static void advance_step(led_pattern_t *p)
{
    p->current_step++;
    p->tick_in_step = 0;
    if (p->current_step >= p->cfg.step_count) {
        if (p->cfg.loop) {
            p->current_step = 0;
            p->is_on = true;
        } else {
            p->finished = true;
            p->is_on = false;
        }
    } else {
        p->is_on = true;
    }
}

bool led_pattern_tick(led_pattern_t *p)
{
    if (!p || p->finished)
        return false;
    if (!p->cfg.steps || p->cfg.step_count == 0) {
        p->finished = true;
        return false;
    }

    bool result = p->is_on;
    const led_step_t *step = &p->cfg.steps[p->current_step];
    p->tick_in_step++;

    if (p->is_on) {
        if (p->tick_in_step >= step->on_ticks) {
            if (step->off_ticks > 0) {
                p->is_on = false;
                p->tick_in_step = 0;
            } else {
                advance_step(p);
            }
        }
    } else {
        if (p->tick_in_step >= step->off_ticks) {
            advance_step(p);
        }
    }

    return result;
}

bool led_pattern_is_on(const led_pattern_t *p)
{
    if (!p) return false;
    return p->is_on;
}

bool led_pattern_finished(const led_pattern_t *p)
{
    if (!p) return true;
    return p->finished;
}

void led_pattern_restart(led_pattern_t *p)
{
    if (!p) return;
    p->current_step = 0;
    p->tick_in_step = 0;
    p->finished = false;
    p->is_on = (p->cfg.step_count > 0 && p->cfg.steps &&
                p->cfg.steps[0].on_ticks > 0);
}
