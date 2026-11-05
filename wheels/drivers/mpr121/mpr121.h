// SPDX-License-Identifier: MIT
/*
 * mpr121 — NXP MPR121 12-channel capacitive touch sensor (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 12 capacitive sensing electrodes with per-channel 10-bit
 * filtered data. Touch/release thresholds configurable per
 * electrode. Soft-reset verification (no chip ID register).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef MPR121_H
#define MPR121_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    MPR121_OK        =  0,
    MPR121_ERR_I2C   = -1,
    MPR121_ERR_PARAM = -2,
    MPR121_ERR_OVERCURRENT = -3,  /* OVCF set: the part has stopped */
} mpr121_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* required for soft reset */
    void *ctx;
} mpr121_i2c_t;

/* ---------- constants ---------- */

#define MPR121_DEV_ADDR       0x5Au
#define MPR121_CHANNEL_COUNT  12u

/* ---------- registers ---------- */

#define MPR121_REG_TOUCH_STATUS_L  0x00u
#define MPR121_REG_TOUCH_STATUS_H  0x01u
#define MPR121_REG_EFD0_LB         0x04u  /* electrode 0 filtered data low  */
#define MPR121_REG_EFD0_HB         0x05u  /* electrode 0 filtered data high */
#define MPR121_REG_MHDR            0x2Bu  /* MHD rising            */
#define MPR121_REG_NHDR            0x2Cu  /* NHD amount rising     */
#define MPR121_REG_NCLR            0x2Du  /* NCL rising            */
#define MPR121_REG_FDLR            0x2Eu  /* FDL rising            */
#define MPR121_REG_MHDF            0x2Fu  /* MHD falling           */
#define MPR121_REG_NHDF            0x30u  /* NHD amount falling    */
#define MPR121_REG_NCLF            0x31u  /* NCL falling           */
#define MPR121_REG_FDLF            0x32u  /* FDL falling           */
#define MPR121_REG_E0TTH           0x41u  /* electrode 0 touch threshold    */
#define MPR121_REG_E0RTH           0x42u  /* electrode 0 release threshold  */
#define MPR121_REG_ECR             0x5Eu  /* electrode configuration        */
#define MPR121_REG_SOFT_RESET      0x80u  /* write 0x63 to soft reset       */

/* ---------- configuration ---------- */

typedef struct {
    uint8_t touch_threshold;    /* per-electrode touch threshold    (default 12) */
    uint8_t release_threshold;  /* per-electrode release threshold  (default 6)  */
    uint8_t ecr;                /* ECR register value (default 0x8F = baseline + 12 electrodes) */
} mpr121_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const mpr121_i2c_t *i2c;
    uint8_t addr;
} mpr121_t;

/* ---------- API ---------- */

/** Soft-reset, configure thresholds + ECR. @thread_safe: no @isr_safe: no */
mpr121_err_t mpr121_init(mpr121_t *dev,
                         const mpr121_i2c_t *i2c,
                         const mpr121_cfg_t *cfg);

/** Read 12-channel touch bitmap (bit N = electrode N, bits 15:12 always 0).
 *  Returns MPR121_ERR_OVERCURRENT if OVCF is set: over current on REXT put the
 *  part in Stop Mode. To restart, write 0x80 to MPR121_REG_TOUCH_STATUS_H
 *  through the HAL to clear OVCF, then call mpr121_init again.
 *  @thread_safe: caller_must_lock @isr_safe: no */
mpr121_err_t mpr121_read_touched(mpr121_t *dev, uint16_t *status);

/** Read 10-bit filtered electrode data for one channel [0..11]. @thread_safe: caller_must_lock @isr_safe: no */
mpr121_err_t mpr121_read_electrode(mpr121_t *dev, uint8_t channel,
                                   uint16_t *value);

#ifdef __cplusplus
}
#endif

#endif /* MPR121_H */
