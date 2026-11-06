// SPDX-License-Identifier: MIT
/*
 * hw_model_pac1952.h -- PAC1952 bus model from datasheet
 *
 * Models Microchip PAC1952 from DS20006539E (PDF page numbers) at bus
 * level, so tests see the exact bytes:
 * - Client address 0x10, the ADDRSEL-to-GND case (Table 6-1, p37)
 * - 0x00 REFRESH and 0x1F REFRESH_V are commands: "No Data in this
 *   command, Send Byte only" (p47, p56)
 * - A REFRESH latches the readable registers and puts CTRL into effect
 *   (p26), and for 1 ms after it "new commands ... will be ignored and
 *   NACKed" (p26)
 * - 0x01 CTRL (R/W, 16-bit): POR 0730h on a PAC1952 (p49), SAMPLE_MODE
 *   at [15:12] (p48). Taken by a Block Write (p41), reads back at once
 *   (p47)
 * - 0x21 CTRL_ACT: the CTRL value in effect (p58)
 * - 0x07 VBUS1, 0x08 VBUS2 (read-only, 16-bit unsigned, FSR = 32V)
 * - 0x0B VSENSE1, 0x0C VSENSE2 (read-only, 16-bit unsigned at POR,
 *   FSR = 100mV)
 * - 0xFD PRODUCT_ID (read-only, 1 byte, 0x79 on a PAC1952-1)
 *
 * The manual does not say what the chip does with a data byte after a
 * command, or with a CTRL write of the wrong length. The model refuses
 * both, which keeps such a driver bug visible. A read inside the 1 ms
 * window fails too: its register pointer write is a new command.
 *
 * Not modelled: conversion cycles (a rate change can take effect later
 * than the REFRESH, p58), accumulators, auto-increment, channel skip.
 */

#ifndef HW_MODEL_PAC1952_H
#define HW_MODEL_PAC1952_H

#include "hw_model.h"

#define PAC_MODEL_ADDR       0x10u
#define PAC_MODEL_CTRL_POR   0x0730u
#define PAC_MODEL_REFRESH_MS 1u

/* --- registers and commands (Table 7-1, p44-p46) --- */
#define PAC_REG_REFRESH     0x00u
#define PAC_REG_CTRL        0x01u
#define PAC_REG_VBUS1       0x07u
#define PAC_REG_VBUS2       0x08u
#define PAC_REG_VSENSE1     0x0Bu
#define PAC_REG_VSENSE2     0x0Cu
#define PAC_REG_REFRESH_V   0x1Fu
#define PAC_REG_CTRL_ACT    0x21u
#define PAC_REG_PRODUCT_ID  0xFDu

#define PAC_MODEL_LOG_MAX   4
#define PAC_MODEL_FRAME_MAX 4

/* One write on the bus: the bytes after the address */
typedef struct {
    uint16_t len;
    uint8_t  data[PAC_MODEL_FRAME_MAX];
} pac1952_model_frame_t;

/* --- Simulated chip and physical environment --- */
typedef struct {
    hw_reg_model_t hw;              /* failure injection only */
    uint16_t ctrl;                  /* 0x01, as written */
    uint16_t ctrl_act;              /* 0x21, in effect */
    uint16_t true_vbus[2];          /* raw 16-bit unsigned, measured now */
    uint16_t true_vsense[2];        /* raw 16-bit unsigned, measured now */
    uint16_t vbus[2];               /* as the registers show it */
    uint16_t vsense[2];             /* as the registers show it */
    uint32_t busy_ms;               /* what is left of the 1 ms window */
    uint32_t delay_total_ms;
    int      refresh_count;
    int      refresh_v_count;
    int      write_count;           /* writes on the bus, refused or not */
    pac1952_model_frame_t writes[PAC_MODEL_LOG_MAX];
} pac1952_model_t;

/* p26: a REFRESH latches the results and activates CTRL */
static void pac1952_model__refresh(pac1952_model_t *m)
{
    m->ctrl_act = m->ctrl;
    for (int ch = 0; ch < 2; ch++) {
        m->vbus[ch]   = m->true_vbus[ch];
        m->vsense[ch] = m->true_vsense[ch];
    }
    m->busy_ms = PAC_MODEL_REFRESH_MS;
}

/* START, address with R/W=0, data bytes, STOP */
__attribute__((unused))
static bool pac1952_model_write_raw(void *ctx, uint8_t dev_addr,
                                    const uint8_t *data, uint16_t len)
{
    pac1952_model_t *m = (pac1952_model_t *)ctx;

    if (hw_model__should_fail(&m->hw)) return false;
    if (dev_addr != PAC_MODEL_ADDR) return false;

    if (m->write_count < PAC_MODEL_LOG_MAX) {
        pac1952_model_frame_t *f = &m->writes[m->write_count];
        f->len = len;
        for (uint16_t i = 0; i < len && i < PAC_MODEL_FRAME_MAX; i++)
            f->data[i] = data[i];
    }
    m->write_count++;

    if (m->busy_ms != 0 || len == 0) return false;

    switch (data[0]) {
    case PAC_REG_REFRESH:
        if (len != 1) return false;
        m->refresh_count++;
        pac1952_model__refresh(m);
        return true;
    case PAC_REG_REFRESH_V:
        if (len != 1) return false;
        m->refresh_v_count++;
        pac1952_model__refresh(m);
        return true;
    case PAC_REG_CTRL:
        if (len == 1) return true;   /* Send Byte: sets the pointer only */
        if (len != 3) return false;
        m->ctrl = (uint16_t)((uint16_t)data[1] << 8 | data[2]);
        return true;
    default:
        return len == 1;             /* the others here are read-only */
    }
}

/* Block Read: register address, repeated START, data bytes (p42) */
__attribute__((unused))
static bool pac1952_model_read_regs(void *ctx, uint8_t dev_addr,
                                    uint8_t reg,
                                    uint8_t *buf, uint16_t len)
{
    pac1952_model_t *m = (pac1952_model_t *)ctx;
    uint16_t val;

    if (hw_model__should_fail(&m->hw)) return false;
    if (dev_addr != PAC_MODEL_ADDR) return false;
    if (m->busy_ms != 0) return false;

    switch (reg) {
    case PAC_REG_PRODUCT_ID:
        if (len != 1) return false;
        buf[0] = 0x79;
        return true;
    case PAC_REG_CTRL:     val = m->ctrl;      break;
    case PAC_REG_CTRL_ACT: val = m->ctrl_act;  break;
    case PAC_REG_VBUS1:    val = m->vbus[0];   break;
    case PAC_REG_VBUS2:    val = m->vbus[1];   break;
    case PAC_REG_VSENSE1:  val = m->vsense[0]; break;
    case PAC_REG_VSENSE2:  val = m->vsense[1]; break;
    default:               return false;
    }

    if (len != 2) return false;
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFFu);
    return true;
}

/* Time passing: this is what ends the 1 ms window */
__attribute__((unused))
static void pac1952_model_delay_ms(void *ctx, uint32_t ms)
{
    pac1952_model_t *m = (pac1952_model_t *)ctx;
    m->delay_total_ms += ms;
    m->busy_ms = (ms >= m->busy_ms) ? 0u : m->busy_ms - ms;
}

static void pac1952_model_init(pac1952_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);

    m->ctrl     = PAC_MODEL_CTRL_POR;
    m->ctrl_act = PAC_MODEL_CTRL_POR;

    /* Default physical: 12V bus ch0, 5V bus ch1, 50mV sense both */
    /* 12V: raw = 12 / (32/65536) = 24576 = 0x6000 */
    m->true_vbus[0] = 0x6000;
    /* 5V: raw = 5 / (32/65536) = 10240 = 0x2800 */
    m->true_vbus[1] = 0x2800;
    /* 50mV: raw = 50 / (100/65536) = 32768 = 0x8000 */
    m->true_vsense[0] = 0x8000;
    m->true_vsense[1] = 0x8000;
}

/* Helper: set simulated VBUS from voltage (V) */
__attribute__((unused))
static void pac1952_model_set_vbus_v(pac1952_model_t *m, uint8_t ch,
                                     float volts)
{
    m->true_vbus[ch] = (uint16_t)(volts / (32.0f / 65536.0f));
}

/* Helper: set simulated VSENSE from millivolts, unipolar as at POR */
__attribute__((unused))
static void pac1952_model_set_vsense_mv(pac1952_model_t *m, uint8_t ch,
                                        float mv)
{
    m->true_vsense[ch] = (uint16_t)(mv / (100.0f / 65536.0f));
}

#endif /* HW_MODEL_PAC1952_H */
