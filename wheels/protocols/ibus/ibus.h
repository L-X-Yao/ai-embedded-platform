// SPDX-License-Identifier: MIT
/*
 * ibus — FlySky iBUS serial protocol for RC receivers
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * 32-byte frame: length(0x20) + type(0x40) + 14 channels (16-bit LE) + checksum.
 * Channel range: 1000-2000 (center 1500, like standard RC PWM).
 * Pure C99, no heap, no OS.
 */

#ifndef IBUS_H
#define IBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IBUS_FRAME_LEN    32
#define IBUS_NUM_CHANNELS 14

typedef enum {
    IBUS_OK            =  0,
    IBUS_ERR_PARAM     = -1,
    IBUS_ERR_SHORT     = -2,
    IBUS_ERR_HEADER    = -3,
    IBUS_ERR_CHECKSUM  = -4,
} ibus_err_t;

typedef struct {
    uint16_t channels[IBUS_NUM_CHANNELS];
} ibus_frame_t;

/** Decode a 32-byte iBUS frame. @thread_safe: yes @isr_safe: yes */
ibus_err_t ibus_decode(const uint8_t *data, size_t len, ibus_frame_t *out);

/** Encode channel data into a 32-byte iBUS frame. @thread_safe: yes @isr_safe: yes */
ibus_err_t ibus_encode(const ibus_frame_t *frame, uint8_t *buf, size_t cap);

/** Calculate iBUS checksum over len bytes. @thread_safe: yes @isr_safe: yes */
uint16_t ibus_checksum(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* IBUS_H */
