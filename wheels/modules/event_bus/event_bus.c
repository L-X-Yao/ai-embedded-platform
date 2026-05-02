/*
 * event_bus -- publish/subscribe event dispatcher
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "event_bus.h"
#include <string.h>

/* Find slot for event_id; return NULL if not found. */
static event_bus_slot_t *find_slot(event_bus_t *bus, uint16_t event_id)
{
    for (uint8_t i = 0; i < bus->slot_count; i++) {
        if (bus->slots[i].event_id == event_id) {
            return &bus->slots[i];
        }
    }
    return NULL;
}

event_bus_err_t event_bus_init(event_bus_t *bus)
{
    if (!bus) {
        return EVENT_BUS_ERR_PARAM;
    }
    memset(bus, 0, sizeof(*bus));
    return EVENT_BUS_OK;
}

event_bus_err_t event_bus_subscribe(event_bus_t *bus, uint16_t event_id,
                                    event_bus_cb_t cb, void *ctx)
{
    if (!bus || !cb) {
        return EVENT_BUS_ERR_PARAM;
    }

    event_bus_slot_t *slot = find_slot(bus, event_id);
    if (!slot) {
        if (bus->slot_count >= EVENT_BUS_MAX_EVENTS) {
            return EVENT_BUS_ERR_FULL;
        }
        slot = &bus->slots[bus->slot_count];
        slot->event_id = event_id;
        slot->count = 0;
        bus->slot_count++;
    }

    if (slot->count >= EVENT_BUS_MAX_SUBS_PER_EVENT) {
        return EVENT_BUS_ERR_FULL;
    }

    slot->subs[slot->count].cb = cb;
    slot->subs[slot->count].ctx = ctx;
    slot->count++;

    return EVENT_BUS_OK;
}

event_bus_err_t event_bus_unsubscribe(event_bus_t *bus, uint16_t event_id,
                                      event_bus_cb_t cb, void *ctx)
{
    if (!bus || !cb) {
        return EVENT_BUS_ERR_PARAM;
    }

    event_bus_slot_t *slot = find_slot(bus, event_id);
    if (!slot) {
        return EVENT_BUS_OK;
    }

    for (uint8_t i = 0; i < slot->count; i++) {
        if (slot->subs[i].cb == cb && slot->subs[i].ctx == ctx) {
            /* Shift remaining entries down to keep order stable. */
            for (uint8_t j = i; j + 1 < slot->count; j++) {
                slot->subs[j] = slot->subs[j + 1];
            }
            slot->count--;
            return EVENT_BUS_OK;
        }
    }

    return EVENT_BUS_OK;
}

event_bus_err_t event_bus_publish(event_bus_t *bus, uint16_t event_id,
                                  const void *data, size_t data_len)
{
    if (!bus) {
        return EVENT_BUS_ERR_PARAM;
    }

    event_bus_slot_t *slot = find_slot(bus, event_id);
    if (!slot) {
        return EVENT_BUS_OK;
    }

    for (uint8_t i = 0; i < slot->count; i++) {
        slot->subs[i].cb(event_id, data, data_len, slot->subs[i].ctx);
    }

    return EVENT_BUS_OK;
}
