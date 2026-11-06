// SPDX-License-Identifier: MIT
/*
 * hw_model_pct2075.h -- PCT2075 hardware register model from datasheet
 *
 * Models PCT2075 register behavior, each register in its own storage:
 * - Reg 0x00: Temperature (read-only, 2 bytes, 11-bit two's complement)
 * - Reg 0x01: Configuration (R/W, 1 byte)
 * - Reg 0x02: T_hyst (R/W, 2 bytes, 9-bit set-point)
 * - Reg 0x03: T_os (R/W, 2 bytes, 9-bit set-point)
 * - Reg 0x04: T_idle (R/W, 1 byte)
 *
 * POR state per datasheet Table 7:
 *   Temperature = 0x0000 (0 C)
 *   Config = 0x00
 *   T_hyst = 0x4B00 (75 C)
 *   T_os = 0x5000 (80 C)
 *   T_idle = 0x00
 *
 * Temp changes only when a conversion ends: about 28 ms after power-up or
 * after leaving shutdown (p.6, p.7), and never in shutdown. The time comes
 * from delay_ms. After that first conversion hw_model_inject_temp stands
 * for a finished conversion, so a test need not wait out the 100 ms period.
 */
#ifndef HW_MODEL_PCT2075_H
#define HW_MODEL_PCT2075_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_CONF_SHUTDOWN  0x01u

/* p.6 tconv(T), "about 28 ms" */
#define HW_TCONV_MS       28u

typedef struct {
    uint8_t  temp[2];      /* reg 0x00: result of the last conversion */
    uint8_t  conf;         /* reg 0x01 */
    uint8_t  thyst[2];     /* reg 0x02 */
    uint8_t  tos[2];       /* reg 0x03 */
    uint8_t  tidle;        /* reg 0x04 */
    uint8_t  sensed[2];    /* what the next conversion will store */
    uint32_t running_ms;   /* since power-up or the end of shutdown */
} pct2075_model_t;

static pct2075_model_t pct_mdl;
static int     hw_write_count;
static int     hw_read_count;

static void hw_model_reset(void)
{
    memset(&pct_mdl, 0, sizeof(pct_mdl));
    hw_write_count = 0;
    hw_read_count  = 0;

    /* Table 7 */
    pct_mdl.thyst[0] = 0x4B;
    pct_mdl.thyst[1] = 0x00;
    pct_mdl.tos[0]   = 0x50;
    pct_mdl.tos[1]   = 0x00;
}

/* p.6 "the Temp register is updated at the end of each conversion" */
static void hw_model__convert(void)
{
    if (pct_mdl.conf & HW_CONF_SHUTDOWN)
        return;
    if (pct_mdl.running_ms >= HW_TCONV_MS)
        memcpy(pct_mdl.temp, pct_mdl.sensed, sizeof(pct_mdl.temp));
}

/* Inject a temperature value (11-bit two's complement in bits [15:5]) */
static void hw_model_inject_temp(float temp_c)
{
    /* Convert: temp_c / 0.125 = raw 11-bit value, then shift left 5 */
    int16_t raw11 = (int16_t)(temp_c / 0.125f);
    int16_t raw16 = (int16_t)(uint16_t)((uint16_t)raw11 << 5);
    pct_mdl.sensed[0] = (uint8_t)((uint16_t)raw16 >> 8);
    pct_mdl.sensed[1] = (uint8_t)((uint16_t)raw16 & 0xFFu);
    hw_model__convert();
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;

    switch (reg) {
    case 0x01:
        /* p.6 "The temperature conversion is initiated when the device is
         * powered-up or put back into normal mode from shutdown." */
        if ((pct_mdl.conf & HW_CONF_SHUTDOWN) && !(val & HW_CONF_SHUTDOWN))
            pct_mdl.running_ms = 0;
        pct_mdl.conf = val;
        break;
    case 0x04:
        pct_mdl.tidle = val;
        break;
    default:
        /* Temp is read only, and one byte cannot fill Thyst or Tos. */
        break;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;

    const uint8_t *src = NULL;
    uint16_t width = 0;

    switch (reg) {
    case 0x00: src = pct_mdl.temp;   width = 2; break;
    case 0x01: src = &pct_mdl.conf;  width = 1; break;
    case 0x02: src = pct_mdl.thyst;  width = 2; break;
    case 0x03: src = pct_mdl.tos;    width = 2; break;
    case 0x04: src = &pct_mdl.tidle; width = 1; break;
    default: break;
    }

    /* The manual gives no bytes past a register's width, so a read past it
     * gets an idle bus rather than the next register. */
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = (i < width) ? src[i] : 0xFFu;
    }
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    if (!(pct_mdl.conf & HW_CONF_SHUTDOWN))
        pct_mdl.running_ms += ms;
    hw_model__convert();
}

#endif /* HW_MODEL_PCT2075_H */
