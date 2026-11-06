// SPDX-License-Identifier: MIT
/*
 * ncp5623_parser — ToshibaLED (TCA62724) I2C LED command parser
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Parses what ArduPilot's AP_Notify/ToshibaLED_I2C.cpp sends at I2C address
 * 0x55, despite the name. A real NCP5623 master sends single bytes of
 * (reg & 0xE0) | (data & 0x1F), which give no events here. The name stays
 * because renaming the wheel would change its API.
 */

#ifndef NCP5623_PARSER_H
#define NCP5623_PARSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- event types ---------- */

typedef enum {
    NCP5623_EVT_NONE = 0,
    NCP5623_EVT_ENABLE,
    NCP5623_EVT_COLOR,
} ncp5623_evt_type_t;

/* ---------- color (4-bit per channel, raw from protocol) ---------- */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ncp5623_color_t;

/* ---------- event ---------- */

typedef struct {
    ncp5623_evt_type_t type;
    union {
        uint8_t        enabled;
        ncp5623_color_t color;
    } data;
} ncp5623_evt_t;

/* ---------- parser state ---------- */

typedef struct {
    uint8_t reg_addr;
    uint8_t buf[3];
    uint8_t idx;
    uint8_t enabled;
} ncp5623_parser_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Initialize parser state. @thread_safe: no */
void               ncp5623_parser_init(ncp5623_parser_t *p);
/** Begin I2C transaction (address phase). @thread_safe: caller_must_lock */
void               ncp5623_parser_start(ncp5623_parser_t *p);
/** Feed one received byte. @thread_safe: caller_must_lock */
ncp5623_evt_type_t ncp5623_parser_feed(ncp5623_parser_t *p,
                                        uint8_t byte,
                                        ncp5623_evt_t *evt);
/** End I2C transaction. @thread_safe: caller_must_lock */
void               ncp5623_parser_stop(ncp5623_parser_t *p);

#ifdef __cplusplus
}
#endif

#endif /* NCP5623_PARSER_H */
