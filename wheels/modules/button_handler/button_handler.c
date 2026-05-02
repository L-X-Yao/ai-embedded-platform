// SPDX-License-Identifier: MIT
/*
 * button_handler -- high-level button event processor
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "button_handler.h"
#include <string.h>

btn_handler_err_t btn_handler_init(btn_handler_t *bh, btn_callback_fn cb, void *ctx,
                                   uint16_t debounce_ticks, uint16_t long_press_ticks,
                                   uint16_t double_click_ticks, uint16_t repeat_ticks)
{
    if (bh == NULL || cb == NULL) {
        return BTN_HANDLER_ERR_PARAM;
    }

    memset(bh, 0, sizeof(*bh));
    bh->callback           = cb;
    bh->callback_ctx       = ctx;
    bh->tick               = 0;
    bh->debounce_ticks     = debounce_ticks;
    bh->long_press_ticks   = long_press_ticks;
    bh->double_click_ticks = double_click_ticks;
    bh->repeat_ticks       = repeat_ticks;
    bh->count              = 0;

    return BTN_HANDLER_OK;
}

btn_handler_err_t btn_handler_register(btn_handler_t *bh, uint8_t id,
                                       btn_read_fn read, void *ctx)
{
    if (bh == NULL || read == NULL) {
        return BTN_HANDLER_ERR_PARAM;
    }
    if (bh->count >= BTN_HANDLER_MAX_BUTTONS) {
        return BTN_HANDLER_ERR_FULL;
    }

    btn_entry_t *e    = &bh->buttons[bh->count];
    e->read           = read;
    e->read_ctx       = ctx;
    e->id             = id;
    e->last_state     = false;
    e->pressed        = false;
    e->press_tick     = 0;
    e->release_tick   = 0;
    e->click_count    = 0;
    e->long_press_fired = false;
    e->repeat_tick    = 0;

    bh->count++;
    return BTN_HANDLER_OK;
}

uint8_t btn_handler_count(const btn_handler_t *bh)
{
    if (bh == NULL) {
        return 0;
    }
    return bh->count;
}

/*
 * Per-button state machine (called from btn_handler_tick):
 *
 *  1. Read raw state via callback.
 *  2. Debounce: ignore transitions shorter than debounce_ticks.
 *  3. On debounced press   -> fire PRESS, record press_tick.
 *  4. On debounced release -> fire RELEASE, increment click_count.
 *  5. While held past long_press_ticks -> fire LONG_PRESS (once),
 *     then fire REPEAT every repeat_ticks (if repeat_ticks > 0).
 *  6. After release, if double_click_ticks expires:
 *     - click_count == 1 -> fire CLICK
 *     - click_count == 2 -> fire DOUBLE_CLICK
 *     - click_count > 2  -> fire DOUBLE_CLICK (consume 2) then CLICK (remainder)
 *  7. Long-press suppresses click on release (click_count reset).
 */
static void process_button(btn_handler_t *bh, btn_entry_t *btn)
{
    bool raw = btn->read(btn->read_ctx);
    uint32_t now = bh->tick;

    /* --- debounced press edge --- */
    if (raw && !btn->pressed && !btn->last_state) {
        /* raw went high; start debounce */
        btn->last_state = true;
        btn->press_tick = now;
    } else if (raw && !btn->pressed && btn->last_state) {
        /* still high, check debounce */
        uint32_t elapsed = now - btn->press_tick;
        if (elapsed >= bh->debounce_ticks) {
            btn->pressed = true;
            btn->long_press_fired = false;
            btn->repeat_tick = now;
            bh->callback(bh->callback_ctx, btn->id, BTN_EVENT_PRESS);
        }
    } else if (!raw && btn->last_state) {
        /* raw went low */
        btn->last_state = false;

        if (btn->pressed) {
            /* debounced release */
            btn->pressed = false;
            bh->callback(bh->callback_ctx, btn->id, BTN_EVENT_RELEASE);

            if (!btn->long_press_fired) {
                /* count this click toward multi-click detection */
                if (btn->click_count == 0) {
                    btn->release_tick = now;
                }
                btn->click_count++;
            } else {
                /* long-press already fired -- suppress click */
                btn->click_count = 0;
            }
        } else {
            /* released before debounce completed -- bounce, ignore */
        }
    } else if (!raw && !btn->last_state) {
        /* still released -- check pending click timeout */
        if (btn->click_count > 0) {
            uint32_t idle = now - btn->release_tick;
            if (idle >= bh->double_click_ticks) {
                /* emit accumulated clicks */
                if (btn->click_count >= 2) {
                    bh->callback(bh->callback_ctx, btn->id, BTN_EVENT_DOUBLE_CLICK);
                    btn->click_count -= 2;
                }
                if (btn->click_count >= 1) {
                    bh->callback(bh->callback_ctx, btn->id, BTN_EVENT_CLICK);
                    btn->click_count = 0;
                }
            }
        }
    }

    /* --- long-press / repeat while held --- */
    if (btn->pressed) {
        uint32_t held = now - btn->press_tick;
        if (!btn->long_press_fired && held >= bh->long_press_ticks) {
            btn->long_press_fired = true;
            btn->click_count = 0;
            btn->repeat_tick = now;
            bh->callback(bh->callback_ctx, btn->id, BTN_EVENT_LONG_PRESS);
        }
        if (btn->long_press_fired && bh->repeat_ticks > 0) {
            uint32_t since_repeat = now - btn->repeat_tick;
            if (since_repeat >= bh->repeat_ticks) {
                btn->repeat_tick = now;
                bh->callback(bh->callback_ctx, btn->id, BTN_EVENT_REPEAT);
            }
        }
    }
}

void btn_handler_tick(btn_handler_t *bh)
{
    if (bh == NULL) {
        return;
    }

    for (uint8_t i = 0; i < bh->count; i++) {
        process_button(bh, &bh->buttons[i]);
    }

    bh->tick++;
}
