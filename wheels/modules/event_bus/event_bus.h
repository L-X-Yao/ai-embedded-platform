/*
 * event_bus -- publish/subscribe event dispatcher
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 *
 * Passive pub/sub: application calls event_bus_publish(), all matching
 * subscribers are called synchronously in registration order.
 * No heap, no OS, no timer HW. Pure C99.
 * Thread safety: caller owns the event_bus_t struct.
 */

#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- compile-time config ---------- */

#ifndef EVENT_BUS_MAX_EVENTS
#define EVENT_BUS_MAX_EVENTS 16
#endif

#ifndef EVENT_BUS_MAX_SUBS_PER_EVENT
#define EVENT_BUS_MAX_SUBS_PER_EVENT 8
#endif

/* ---------- error codes ---------- */

typedef enum {
    EVENT_BUS_OK         =  0,
    EVENT_BUS_ERR_PARAM  = -1,
    EVENT_BUS_ERR_FULL   = -2,
} event_bus_err_t;

/* ---------- callback ---------- */

typedef void (*event_bus_cb_t)(uint16_t event_id, const void *data,
                               size_t data_len, void *ctx);

/* ---------- internal types ---------- */

typedef struct {
    event_bus_cb_t cb;
    void          *ctx;
} event_bus_sub_t;

typedef struct {
    uint16_t         event_id;
    uint8_t          count;
    event_bus_sub_t  subs[EVENT_BUS_MAX_SUBS_PER_EVENT];
} event_bus_slot_t;

typedef struct {
    event_bus_slot_t slots[EVENT_BUS_MAX_EVENTS];
    uint8_t          slot_count;
} event_bus_t;

/* ---------- API ---------- */

/** Initialize event bus. @thread_safe: no */
event_bus_err_t event_bus_init(event_bus_t *bus);

/** Subscribe to an event. Creates the event slot if needed. @thread_safe: no */
event_bus_err_t event_bus_subscribe(event_bus_t *bus, uint16_t event_id,
                                    event_bus_cb_t cb, void *ctx);

/** Unsubscribe a callback from an event. Matches both cb and ctx. @thread_safe: no */
event_bus_err_t event_bus_unsubscribe(event_bus_t *bus, uint16_t event_id,
                                      event_bus_cb_t cb, void *ctx);

/** Publish event data to all subscribers synchronously. @thread_safe: no */
event_bus_err_t event_bus_publish(event_bus_t *bus, uint16_t event_id,
                                  const void *data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BUS_H */
