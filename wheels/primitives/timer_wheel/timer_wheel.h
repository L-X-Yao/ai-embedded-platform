// SPDX-License-Identifier: MIT
/*
 * timer_wheel — hashed timing wheel for efficient timeout management
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Classic hashed timing wheel: timers are placed into slots based on
 * (current_pos + ticks) % num_slots. Each tick advances the cursor and
 * fires expired entries. add/cancel are O(1); tick is O(max_timers)
 * due to linear scan. Passive design:
 * caller drives via timer_wheel_tick(). No heap, no OS dependency.
 *
 * Thread safety: tick runs in one context, usually a periodic ISR. add and
 * cancel run in that same context, for example from a callback, or in another
 * context with the tick's interrupt masked around the call. Unmasked, add can
 * read the cursor just before a tick moves it, and the timer then fires a
 * whole revolution late; the two sides can also lose a change to count.
 * init must finish before tick starts.
 */

#ifndef TIMER_WHEEL_H
#define TIMER_WHEEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Keeps a timer's fields on the right side of its active flag. A compiler
 * barrier is enough on a single core, because a core sees its own accesses in
 * program order and an interrupt does not change that. If tick runs on another
 * core, define this as a real fence, such as __DMB(), before including this
 * header or on the command line.
 */
#ifndef TIMER_WHEEL_BARRIER
#if defined(__GNUC__)
#define TIMER_WHEEL_BARRIER() __asm__ volatile("" ::: "memory")
#else
#error "define TIMER_WHEEL_BARRIER() as this compiler's memory barrier"
#endif
#endif

/* ---- error codes --------------------------------------------------- */

typedef enum {
    TIMER_WHEEL_OK        =  0,
    TIMER_WHEEL_ERR_PARAM = -1,
    TIMER_WHEEL_ERR_FULL  = -2,
} timer_wheel_err_t;

/* ---- callback ------------------------------------------------------ */

/**
 * Timer expiry callback.
 *
 * @param ctx   User context passed to timer_wheel_add().
 *
 * Called from timer_wheel_tick() context. Keep short, do not call
 * timer_wheel_tick() from within the callback (re-entrancy is not supported).
 * Calling timer_wheel_add() / timer_wheel_cancel() from inside the callback
 * is allowed. A timer added there waits its full delay from this tick,
 * whatever its id. Cancelling a timer due on this same tick stops it.
 */
typedef void (*timer_wheel_cb_t)(void *ctx);

/* ---- internal types (do not access directly) ----------------------- */

/* active is uint8_t, not bool, because GCC 13's -fsanitize=bool reads a volatile
 * bool as a plain byte, and a retry of add then never sees the timer fire. */
typedef struct {
    uint32_t          remaining_rounds;  /* rounds left before firing     */
    uint32_t          slot;              /* which slot this entry lives in */
    timer_wheel_cb_t  callback;          /* NULL means slot is free       */
    void             *ctx;
    volatile uint8_t  active;            /* set last, cleared first */
    bool              due;               /* expires in the tick under way  */
} timer_wheel_entry_t;

typedef struct {
    uint32_t  head;  /* index of first entry in this slot's chain, or UINT32_MAX */
    uint32_t  tail;  /* index of last entry (for O(1) append) */
} timer_wheel_slot_t;

typedef struct {
    timer_wheel_slot_t  *slots;       /* caller-provided array [num_slots]   */
    timer_wheel_entry_t *timers;      /* caller-provided array [max_timers]  */
    uint32_t             num_slots;
    uint32_t             max_timers;
    volatile uint32_t    current_pos; /* cursor into slots[]                 */
    volatile uint32_t    count;       /* number of active timers             */
} timer_wheel_t;

/* ---- public API ---------------------------------------------------- */

/**
 * Initialise a timing wheel.
 *
 * @param tw          Wheel context (caller-allocated).
 * @param slots       Slot storage (caller-allocated, num_slots elements).
 * @param num_slots   Number of wheel slots (must be >= 1).
 * @param timers      Timer entry storage (caller-allocated, max_timers elements).
 * @param max_timers  Maximum concurrent timers (must be >= 1).
 * @return TIMER_WHEEL_OK on success, TIMER_WHEEL_ERR_PARAM if any pointer
 *         is NULL or sizes are zero.
 *
 * @thread_safe: no @isr_safe: no
 */
timer_wheel_err_t timer_wheel_init(timer_wheel_t *tw,
                                   timer_wheel_slot_t *slots,
                                   uint32_t num_slots,
                                   timer_wheel_entry_t *timers,
                                   uint32_t max_timers);

/**
 * Add a timer that fires after @p ticks ticks.
 *
 * @param tw        Initialised wheel.
 * @param id        Timer ID (0 .. max_timers-1). Caller chooses. Must not
 *                  already be active.
 * @param ticks     Delay in ticks (must be >= 1).
 * @param callback  Function to call on expiry (must not be NULL).
 * @param ctx       Opaque pointer forwarded to callback (may be NULL).
 * @return TIMER_WHEEL_OK on success, TIMER_WHEEL_ERR_PARAM on bad args,
 *         TIMER_WHEEL_ERR_FULL if id is already active.
 *
 * In the tick's context, or with its interrupt masked: see the top of this
 * file. A retry on TIMER_WHEEL_ERR_FULL needs the mask too, around each try,
 * because any try can succeed.
 *
 * @thread_safe: no @isr_safe: yes
 */
timer_wheel_err_t timer_wheel_add(timer_wheel_t *tw,
                                  uint32_t id,
                                  uint32_t ticks,
                                  timer_wheel_cb_t callback,
                                  void *ctx);

/**
 * Cancel a pending timer.
 *
 * @param tw   Initialised wheel.
 * @param id   Timer ID to cancel.
 * @return TIMER_WHEEL_OK on success, TIMER_WHEEL_ERR_PARAM if id is out
 *         of range or not active.
 *
 * In the tick's context, or with its interrupt masked: see the top of this
 * file.
 *
 * @thread_safe: no @isr_safe: yes
 */
timer_wheel_err_t timer_wheel_cancel(timer_wheel_t *tw, uint32_t id);

/**
 * Advance the wheel by one tick and fire all expired timers.
 *
 * Must be called periodically (e.g. from SysTick). Expired timer callbacks
 * are invoked synchronously in this call's context.
 *
 * @param tw   Initialised wheel (silently returns if NULL).
 *
 * @thread_safe: no @isr_safe: yes
 */
void timer_wheel_tick(timer_wheel_t *tw);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_WHEEL_H */
