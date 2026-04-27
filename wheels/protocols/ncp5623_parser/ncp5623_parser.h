#ifndef NCP5623_PARSER_H
#define NCP5623_PARSER_H

#include <stdint.h>

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

/* ---------- API ---------- */

void               ncp5623_parser_init(ncp5623_parser_t *p);
void               ncp5623_parser_start(ncp5623_parser_t *p);
ncp5623_evt_type_t ncp5623_parser_feed(ncp5623_parser_t *p,
                                        uint8_t byte,
                                        ncp5623_evt_t *evt);
void               ncp5623_parser_stop(ncp5623_parser_t *p);

#endif /* NCP5623_PARSER_H */
