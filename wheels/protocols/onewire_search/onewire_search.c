// SPDX-License-Identifier: MIT
/*
 * onewire_search -- Dallas/Maxim 1-Wire ROM search algorithm (AN187)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none -- HAL injected via function pointers)
 * Instances: multi
 */

#include "onewire_search.h"
#include <string.h>

/* ---------- CRC8 ---------- */

uint8_t onewire_search_crc8(const uint8_t *data, uint8_t len)
{
    if (!data) { return 0; }

    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            uint8_t mix = (uint8_t)((crc ^ byte) & 0x01u);
            crc >>= 1;
            if (mix) {
                crc ^= 0x8Cu;  /* reflected polynomial */
            }
            byte >>= 1;
        }
    }
    return crc;
}

/* ---------- init ---------- */

onewire_search_err_t onewire_search_init(onewire_search_state_t *state,
                                          const onewire_search_bus_t *bus)
{
    if (!state || !bus) { return ONEWIRE_SEARCH_ERR_PARAM; }
    if (!bus->reset || !bus->write_bit || !bus->read_bit) {
        return ONEWIRE_SEARCH_ERR_PARAM;
    }

    memset(state, 0, sizeof(*state));
    state->bus = bus;
    state->last_discrepancy = 0;
    state->last_device = false;
    return ONEWIRE_SEARCH_OK;
}

/* ---------- search_next (core AN187 algorithm) ---------- */

onewire_search_err_t onewire_search_next(onewire_search_state_t *state,
                                          onewire_search_rom_t *rom)
{
    if (!state || !rom) { return ONEWIRE_SEARCH_ERR_PARAM; }
    if (!state->bus) { return ONEWIRE_SEARCH_ERR_PARAM; }

    /* If we already found the last device, signal done */
    if (state->last_device) {
        return ONEWIRE_SEARCH_ERR_NONE_FOUND;
    }

    /* Reset bus -- check for presence */
    if (!state->bus->reset(state->bus->ctx)) {
        /* No presence detected -- no devices on bus */
        state->last_device = true;
        memset(state->rom, 0, sizeof(state->rom));
        return ONEWIRE_SEARCH_ERR_BUS;
    }

    /* Send SEARCH ROM command (0xF0) -- bit by bit, LSB first */
    {
        uint8_t cmd = ONEWIRE_SEARCH_CMD_SEARCH;
        for (uint8_t b = 0; b < 8u; b++) {
            state->bus->write_bit(state->bus->ctx, (uint8_t)(cmd & 0x01u));
            cmd >>= 1;
        }
    }

    int16_t last_zero = 0;  /* bit position of last discrepancy where we chose 0 */

    /* Walk into a copy: a retry follows the bits of the last good walk, so
     * state may only move on once this ROM proves good. */
    uint8_t walk[ONEWIRE_SEARCH_ROM_LEN];
    memcpy(walk, state->rom, sizeof(walk));

    /* Walk all 64 bits of the ROM code */
    for (uint8_t bit_pos = 1; bit_pos <= 64u; bit_pos++) {
        /* Read bit and its complement from the bus */
        uint8_t id_bit  = state->bus->read_bit(state->bus->ctx) & 0x01u;
        uint8_t cmp_bit = state->bus->read_bit(state->bus->ctx) & 0x01u;

        uint8_t direction;

        if (id_bit == 1u && cmp_bit == 1u) {
            /* No devices responding -- bus error or all devices gone */
            state->last_device = true;
            memset(state->rom, 0, sizeof(state->rom));
            return ONEWIRE_SEARCH_ERR_NONE_FOUND;
        }

        if (id_bit != cmp_bit) {
            /* All devices have the same bit value here -- no discrepancy */
            direction = id_bit;
        } else {
            /* Discrepancy: both 0 and 1 present on the bus */
            if ((int16_t)bit_pos == state->last_discrepancy) {
                /* We chose 0 last time at this position, now choose 1 */
                direction = 1u;
            } else if ((int16_t)bit_pos > state->last_discrepancy) {
                /* New discrepancy beyond previous -- choose 0 (explore left) */
                direction = 0u;
            } else {
                /* Before last_discrepancy -- repeat previous choice */
                uint8_t byte_idx = (uint8_t)((bit_pos - 1u) / 8u);
                uint8_t bit_idx  = (uint8_t)((bit_pos - 1u) % 8u);
                direction = (uint8_t)((walk[byte_idx] >> bit_idx) & 0x01u);
            }

            /* Track the last position where we chose 0 */
            if (direction == 0u) {
                last_zero = (int16_t)bit_pos;
            }
        }

        /* Store the chosen direction bit into rom[] */
        {
            uint8_t byte_idx = (uint8_t)((bit_pos - 1u) / 8u);
            uint8_t bit_idx  = (uint8_t)((bit_pos - 1u) % 8u);
            if (direction != 0u) {
                walk[byte_idx] |= (uint8_t)(1u << bit_idx);
            } else {
                walk[byte_idx] &= (uint8_t)(~(1u << bit_idx));
            }
        }

        /* Write the chosen direction back to the bus to select devices */
        state->bus->write_bit(state->bus->ctx, direction);
    }

    /* Verify CRC8: CRC of all 8 bytes should be 0 */
    if (onewire_search_crc8(walk, 8u) != 0u) {
        return ONEWIRE_SEARCH_ERR_CRC;
    }

    /* A bus stuck low reads 0,0 on every bit, and the CRC of all zeros is 0.
     * AN187 refuses family code 0 for the same reason. */
    if (walk[0] == 0u) {
        return ONEWIRE_SEARCH_ERR_BUS;
    }

    /* Update state for next call */
    memcpy(state->rom, walk, ONEWIRE_SEARCH_ROM_LEN);
    state->last_discrepancy = last_zero;
    if (last_zero == 0) {
        state->last_device = true;  /* no discrepancies left -- last device */
    }

    /* Copy result */
    memcpy(rom->rom, walk, ONEWIRE_SEARCH_ROM_LEN);

    return ONEWIRE_SEARCH_OK;
}

/* ---------- search_first (convenience) ---------- */

onewire_search_err_t onewire_search_first(onewire_search_state_t *state,
                                           onewire_search_rom_t *rom)
{
    if (!state || !rom) { return ONEWIRE_SEARCH_ERR_PARAM; }
    if (!state->bus) { return ONEWIRE_SEARCH_ERR_PARAM; }

    /* Re-init keeping the same bus pointer */
    const onewire_search_bus_t *bus = state->bus;
    onewire_search_err_t err = onewire_search_init(state, bus);
    if (err != ONEWIRE_SEARCH_OK) { return err; }

    return onewire_search_next(state, rom);
}

/* ---------- done check ---------- */

bool onewire_search_done(const onewire_search_state_t *state)
{
    if (!state) { return true; }
    return state->last_device;
}
