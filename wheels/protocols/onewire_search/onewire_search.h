// SPDX-License-Identifier: MIT
/*
 * onewire_search -- Dallas/Maxim 1-Wire ROM search algorithm
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none -- HAL injected via function pointers)
 * Instances: multi
 */

#ifndef ONEWIRE_SEARCH_H
#define ONEWIRE_SEARCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define ONEWIRE_SEARCH_ROM_LEN     8u    /* 64-bit ROM: family(1) + serial(6) + crc(1) */
#define ONEWIRE_SEARCH_CMD_SEARCH  0xF0u /* SEARCH ROM command */

/* ---------- error codes ---------- */

typedef enum {
    ONEWIRE_SEARCH_OK             =  0,
    ONEWIRE_SEARCH_ERR_PARAM      = -1,
    ONEWIRE_SEARCH_ERR_BUS        = -2,  /* no presence pulse or bus error */
    ONEWIRE_SEARCH_ERR_CRC        = -3,  /* CRC8 mismatch on discovered ROM */
    ONEWIRE_SEARCH_ERR_NONE_FOUND = -4   /* no more devices */
} onewire_search_err_t;

/* ---------- HAL (injected) ---------- */

typedef struct {
    void *ctx;
    bool (*reset)(void *ctx);           /* reset pulse, returns true if presence detected */
    void (*write_bit)(void *ctx, uint8_t bit);
    uint8_t (*read_bit)(void *ctx);     /* returns 0 or 1 */
} onewire_search_bus_t;

/* ---------- types ---------- */

typedef struct {
    uint8_t rom[ONEWIRE_SEARCH_ROM_LEN]; /* 64-bit ROM code */
} onewire_search_rom_t;

typedef struct {
    const onewire_search_bus_t *bus;
    int16_t last_discrepancy;
    bool    last_device;
    uint8_t rom[ONEWIRE_SEARCH_ROM_LEN];
} onewire_search_state_t;

/* ---------- API ---------- */

/** Initialize search state. Must be called before first search.
 *  @thread_safe: no  @isr_safe: no (calls HAL) */
onewire_search_err_t onewire_search_init(onewire_search_state_t *state,
                                          const onewire_search_bus_t *bus);

/** Find the next device on the bus. Returns ROM code in *rom.
 *  Returns ONEWIRE_SEARCH_ERR_NONE_FOUND when all devices have been enumerated.
 *  ONEWIRE_SEARCH_ERR_CRC and ONEWIRE_SEARCH_ERR_BUS leave the search where it
 *  was, so calling again retries the same device. ERR_BUS means no presence, no
 *  device answering in mid walk, or a ROM with family code 0, which is what a
 *  bus stuck low reads as. A fault that lasts keeps returning it, so bound any
 *  retries. The one exception: no presence before any device has been found is
 *  an empty bus, which ends the search. onewire_search_first looks again.
 *  @thread_safe: no  @isr_safe: no */
onewire_search_err_t onewire_search_next(onewire_search_state_t *state,
                                          onewire_search_rom_t *rom);

/** Convenience: reset state and find the first device.
 *  Equivalent to init + next.
 *  @thread_safe: no  @isr_safe: no */
onewire_search_err_t onewire_search_first(onewire_search_state_t *state,
                                           onewire_search_rom_t *rom);

/** Check if the last search found the final device, or found the bus empty.
 *  @thread_safe: yes @isr_safe: yes */
bool onewire_search_done(const onewire_search_state_t *state);

/** Compute Dallas/Maxim CRC8 over data.
 *  Polynomial: 0x31 reflected = 0x8C (x^8 + x^5 + x^4 + 1).
 *  CRC of all 8 ROM bytes (including CRC byte) should equal 0.
 *  @thread_safe: yes @isr_safe: yes */
uint8_t onewire_search_crc8(const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* ONEWIRE_SEARCH_H */
