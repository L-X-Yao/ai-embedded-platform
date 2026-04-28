/*
 * ncp5623_parser — NCP5623 I2C LED driver command parser
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#include "ncp5623_parser.h"
#include <stddef.h>

#define REG_COLOR   0x01U
#define REG_ENABLE  0x04U
#define ENABLE_VAL  0x03U

void ncp5623_parser_init(ncp5623_parser_t *p)
{
    if (p == NULL) {
        return;
    }
    p->reg_addr = 0U;
    p->buf[0]   = 0U;
    p->buf[1]   = 0U;
    p->buf[2]   = 0U;
    p->idx      = 0U;
    p->enabled  = 0U;
}

void ncp5623_parser_start(ncp5623_parser_t *p)
{
    if (p == NULL) {
        return;
    }
    p->reg_addr = 0U;
    p->buf[0]   = 0U;
    p->buf[1]   = 0U;
    p->buf[2]   = 0U;
    p->idx      = 0U;
}

ncp5623_evt_type_t ncp5623_parser_feed(ncp5623_parser_t *p,
                                        uint8_t byte,
                                        ncp5623_evt_t *evt)
{
    if (p == NULL || evt == NULL) {
        return NCP5623_EVT_NONE;
    }

    evt->type = NCP5623_EVT_NONE;

    if (p->idx == 0U) {
        p->reg_addr = byte;
        p->idx = 1U;
        return NCP5623_EVT_NONE;
    }

    if (p->idx <= 3U) {
        p->buf[p->idx - 1U] = byte;
        p->idx++;
    }

    if (p->reg_addr == REG_ENABLE && p->idx == 2U) {
        p->enabled = (p->buf[0] == ENABLE_VAL) ? 1U : 0U;
        evt->type = NCP5623_EVT_ENABLE;
        evt->data.enabled = p->enabled;
        return NCP5623_EVT_ENABLE;
    }

    if (p->reg_addr == REG_COLOR && p->idx == 4U && p->enabled != 0U) {
        evt->type = NCP5623_EVT_COLOR;
        evt->data.color.b = p->buf[0] & 0x0FU;
        evt->data.color.g = p->buf[1] & 0x0FU;
        evt->data.color.r = p->buf[2] & 0x0FU;
        return NCP5623_EVT_COLOR;
    }

    return NCP5623_EVT_NONE;
}

void ncp5623_parser_stop(ncp5623_parser_t *p)
{
    if (p == NULL) {
        return;
    }
    p->reg_addr = 0U;
    p->buf[0]   = 0U;
    p->buf[1]   = 0U;
    p->buf[2]   = 0U;
    p->idx      = 0U;
}
