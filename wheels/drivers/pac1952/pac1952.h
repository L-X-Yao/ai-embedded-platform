// SPDX-License-Identifier: MIT
/*
 * pac1952 — Microchip PAC1952 dual-channel I2C power/energy monitor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Supports dual-channel bus voltage, sense voltage, current, and power
 * measurement with configurable sense resistors per channel.
 * VBUS: 16-bit unsigned, FSR = 32V, LSB = 32/65536 V.
 * VSENSE: 16-bit unsigned, FSR = 100mV, LSB = 100/65536 mV (POR default).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef PAC1952_H
#define PAC1952_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    PAC1952_OK        =  0,
    PAC1952_ERR_I2C   = -1,
    PAC1952_ERR_ID    = -2,
    PAC1952_ERR_PARAM = -3,
    PAC1952_ERR_UNSUPPORTED = -4,   /* no call returns this any more */
} pac1952_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    /* Unused: the chip's commands take no data byte, and CTRL takes two. */
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
    /* Puts exactly data[0..len-1] on the bus after the address, then STOP.
     * NULL: pac1952_refresh and pac1952_set_sample_rate return
     * PAC1952_ERR_PARAM and send nothing. */
    bool (*write_raw)(void *ctx, uint8_t dev_addr,
                      const uint8_t *data, uint16_t len);
    /* Waits at least ms milliseconds.  NULL: as for write_raw. */
    void (*delay_ms)(void *ctx, uint32_t ms);
} pac1952_i2c_t;

/* ---------- constants ---------- */

#define PAC1952_DEV_ADDR        0x10u   /* ADDRSEL to GND, and what cfg.addr 0 means */
#define PAC1952_DEV_ADDR_MAX    0x1Fu   /* ADDRSEL tied to VDD */

#define PAC1952_PRODUCT_ID_VAL    0x79u   /* PRODUCT_ID of a PAC1952-1 */
#define PAC1952_PRODUCT_ID_VAL_2  0x7Du   /* PRODUCT_ID of a PAC1952-2 */

/* ---------- sample rate ---------- */

typedef enum {
    PAC1952_RATE_8SPS   = 0,
    PAC1952_RATE_64SPS  = 1,
    PAC1952_RATE_256SPS = 2,
    PAC1952_RATE_1024SPS = 3,
} pac1952_rate_t;

/* ---------- channel data ---------- */

typedef struct {
    float vbus_v;       /* bus voltage in volts       */
    float vsense_mv;    /* sense voltage in millivolts */
    float current_ma;   /* current in milliamps       */
    float power_mw;     /* power in milliwatts        */
} pac1952_channel_t;

/* ---------- configuration ---------- */

typedef struct {
    float sense_resistor_mohm[2]; /* sense resistor per channel (milliohms) */
    uint8_t addr;   /* what the ADDRSEL resistor picks, 0x10 to 0x1F (Table 6-1).
                     * 0 means 0x10, so a config without this field stays as it was */
} pac1952_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const pac1952_i2c_t *i2c;
    pac1952_cfg_t        cfg;
} pac1952_t;

/* ---------- API ---------- */

/** Configure and verify chip PRODUCT_ID (reg 0xFD).  Call it at least
 *  50 ms (tINT_T) after VDD comes up: the driver does not wait for that.
 *  @thread_safe: no @isr_safe: no */
pac1952_err_t pac1952_init(pac1952_t *dev, const pac1952_i2c_t *i2c,
                           const pac1952_cfg_t *cfg);

/** Send REFRESH (command 0x00) to latch new readings, then wait for them
 *  to settle.  It also resets the chip's accumulators, which this driver
 *  does not read.  Must be called before read_channel/read_both for fresh
 *  data.  Needs write_raw and delay_ms.  Calls delay_ms (1 ms).
 *  @thread_safe: no @isr_safe: no */
pac1952_err_t pac1952_refresh(pac1952_t *dev);

/** Read a single channel (0 or 1).
 *  @thread_safe: caller_must_lock @isr_safe: no */
pac1952_err_t pac1952_read_channel(pac1952_t *dev, uint8_t ch,
                                   pac1952_channel_t *out);

/** Read both channels in one call.
 *  @thread_safe: caller_must_lock @isr_safe: no */
pac1952_err_t pac1952_read_both(pac1952_t *dev,
                                pac1952_channel_t *ch1,
                                pac1952_channel_t *ch2);

/** Set SAMPLE_MODE in CTRL, keeping its other bits, then send REFRESH to
 *  activate it.  The rates are the plain modes; the chip starts in the
 *  adaptive 1024 SPS mode.  The new rate starts when the current conversion
 *  cycle ends, which can be after this returns.  Also resets the chip's
 *  accumulators.  Needs write_raw and delay_ms.  Calls delay_ms (1 ms).
 *  @thread_safe: no @isr_safe: no */
pac1952_err_t pac1952_set_sample_rate(pac1952_t *dev, pac1952_rate_t rate);

/** Read PRODUCT_ID register (0xFD).
 *  @thread_safe: caller_must_lock @isr_safe: no */
pac1952_err_t pac1952_get_product_id(pac1952_t *dev, uint8_t *id);

#ifdef __cplusplus
}
#endif

#endif /* PAC1952_H */
