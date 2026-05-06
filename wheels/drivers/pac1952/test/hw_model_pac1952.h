/*
 * hw_model_pac1952.h -- PAC1952 hardware register model from datasheet
 *
 * Models Microchip PAC1952 register behavior:
 * - 16-bit big-endian registers, mapped to byte pairs in hw_reg_model_t
 * - 0x01 CTRL (R/W), sample rate bits at [11:10]
 * - 0x07 VBUS1 (read-only, 16-bit unsigned, FSR = 32V)
 * - 0x08 VBUS2 (read-only, 16-bit unsigned, FSR = 32V)
 * - 0x0B VSENSE1 (read-only, 16-bit signed, FSR = +/-100mV)
 * - 0x0C VSENSE2 (read-only, 16-bit signed, FSR = +/-100mV)
 * - 0x1F REFRESH (write-only, send-byte command to latch readings)
 * - 0xFD PRODUCT_ID (read-only, 1 byte, default 0x59)
 *
 * Each 16-bit register is stored as two consecutive bytes at reg*2 and
 * reg*2+1 in the model, so the standard hw_model I2C callbacks can
 * serve 2-byte read_regs calls. Single-byte registers (PRODUCT_ID)
 * are stored at reg*2 only.
 */

#ifndef HW_MODEL_PAC1952_H
#define HW_MODEL_PAC1952_H

#include "hw_model.h"

/* --- PAC1952 register byte-pair addresses in model --- */
/* Real reg 0x01 -> model bytes [0x02, 0x03] */
#define PAC_REG_CTRL_H      0x02
#define PAC_REG_CTRL_L       0x03
#define PAC_REG_VBUS1_H     0x0E
#define PAC_REG_VBUS1_L     0x0F
#define PAC_REG_VBUS2_H     0x10
#define PAC_REG_VBUS2_L     0x11
#define PAC_REG_VSENSE1_H   0x16
#define PAC_REG_VSENSE1_L   0x17
#define PAC_REG_VSENSE2_H   0x18
#define PAC_REG_VSENSE2_L   0x19
#define PAC_REG_REFRESH_H   0x3E
#define PAC_REG_PRODUCT_ID  0xFA  /* 0xFD * 2 = 0x1FA, but model is 256 bytes;
                                   * use direct mapping for single-byte regs */

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t hw;
    uint16_t true_vbus1;    /* raw 16-bit unsigned */
    uint16_t true_vbus2;
    int16_t  true_vsense1;  /* raw 16-bit signed */
    int16_t  true_vsense2;
    int      refresh_count;
} pac1952_model_t;

/*
 * Custom I2C callbacks that remap the driver's register address
 * to byte-pair addresses in the model. PAC1952 driver uses
 * write_reg (single byte) and read_regs with register addresses.
 */
__attribute__((unused))
static bool pac1952_model_i2c_write_reg(void *ctx, uint8_t dev_addr,
                                        uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    pac1952_model_t *m = (pac1952_model_t *)ctx;

    if (reg == 0x1F) {
        /* REFRESH command: latch readings */
        m->refresh_count++;
        return true;
    }

    /* Map register to byte-pair high byte */
    uint8_t base = (uint8_t)(reg * 2);
    if (base < HW_MODEL_REG_COUNT) {
        hw_model__write_reg(&m->hw, base, val);
    }
    return true;
}

__attribute__((unused))
static bool pac1952_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                        uint8_t reg,
                                        uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    pac1952_model_t *m = (pac1952_model_t *)ctx;

    /* Single-byte registers (PRODUCT_ID at 0xFD) */
    if (reg == 0xFD) {
        if (len >= 1) buf[0] = 0x59;  /* PAC1952 product ID */
        return true;
    }

    uint8_t base = (uint8_t)(reg * 2);
    for (uint16_t i = 0; i < len && (base + i) < HW_MODEL_REG_COUNT; i++) {
        buf[i] = m->hw.regs[base + i];
    }
    return true;
}

/* Update simulated output registers from true physical values */
static void pac1952_model_update_outputs(pac1952_model_t *m)
{
    m->hw.regs[PAC_REG_VBUS1_H] = (uint8_t)(m->true_vbus1 >> 8);
    m->hw.regs[PAC_REG_VBUS1_L] = (uint8_t)(m->true_vbus1 & 0xFF);
    m->hw.regs[PAC_REG_VBUS2_H] = (uint8_t)(m->true_vbus2 >> 8);
    m->hw.regs[PAC_REG_VBUS2_L] = (uint8_t)(m->true_vbus2 & 0xFF);

    uint16_t vs1 = (uint16_t)m->true_vsense1;
    m->hw.regs[PAC_REG_VSENSE1_H] = (uint8_t)(vs1 >> 8);
    m->hw.regs[PAC_REG_VSENSE1_L] = (uint8_t)(vs1 & 0xFF);

    uint16_t vs2 = (uint16_t)m->true_vsense2;
    m->hw.regs[PAC_REG_VSENSE2_H] = (uint8_t)(vs2 >> 8);
    m->hw.regs[PAC_REG_VSENSE2_L] = (uint8_t)(vs2 & 0xFF);
}

static void pac1952_model_init(pac1952_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* CTRL register: default 0x00 0x00 */
    hw_model_set_reset_value(&m->hw, PAC_REG_CTRL_H, 0x00);
    hw_model_set_reset_value(&m->hw, PAC_REG_CTRL_L, 0x00);

    /* VBUS and VSENSE are read-only */
    hw_model_set_readonly(&m->hw, PAC_REG_VBUS1_H);
    hw_model_set_readonly(&m->hw, PAC_REG_VBUS1_L);
    hw_model_set_readonly(&m->hw, PAC_REG_VBUS2_H);
    hw_model_set_readonly(&m->hw, PAC_REG_VBUS2_L);
    hw_model_set_readonly(&m->hw, PAC_REG_VSENSE1_H);
    hw_model_set_readonly(&m->hw, PAC_REG_VSENSE1_L);
    hw_model_set_readonly(&m->hw, PAC_REG_VSENSE2_H);
    hw_model_set_readonly(&m->hw, PAC_REG_VSENSE2_L);

    /* Default physical: 12V bus ch0, 5V bus ch1, 50mV sense both */
    /* 12V: raw = 12 / (32/65536) = 24576 = 0x6000 */
    m->true_vbus1 = 0x6000;
    /* 5V: raw = 5 / (32/65536) = 10240 = 0x2800 */
    m->true_vbus2 = 0x2800;
    /* 50mV: raw = 50 / (100/32768) = 16384 = 0x4000 */
    m->true_vsense1 = 0x4000;
    m->true_vsense2 = 0x4000;

    hw_model_reset(&m->hw);
    pac1952_model_update_outputs(m);
}

/* Helper: set simulated VBUS (raw 16-bit unsigned) */
__attribute__((unused))
static void pac1952_model_set_vbus(pac1952_model_t *m, uint8_t ch,
                                   uint16_t raw)
{
    if (ch == 0) m->true_vbus1 = raw;
    else         m->true_vbus2 = raw;
    pac1952_model_update_outputs(m);
}

/* Helper: set simulated VSENSE (raw 16-bit signed) */
__attribute__((unused))
static void pac1952_model_set_vsense(pac1952_model_t *m, uint8_t ch,
                                     int16_t raw)
{
    if (ch == 0) m->true_vsense1 = raw;
    else         m->true_vsense2 = raw;
    pac1952_model_update_outputs(m);
}

/* Helper: set simulated VBUS from voltage (V) */
__attribute__((unused))
static void pac1952_model_set_vbus_v(pac1952_model_t *m, uint8_t ch,
                                     float volts)
{
    uint16_t raw = (uint16_t)(volts / (32.0f / 65536.0f));
    pac1952_model_set_vbus(m, ch, raw);
}

/* Helper: set simulated VSENSE from millivolts */
__attribute__((unused))
static void pac1952_model_set_vsense_mv(pac1952_model_t *m, uint8_t ch,
                                        float mv)
{
    int16_t raw = (int16_t)(mv / (100.0f / 32768.0f));
    pac1952_model_set_vsense(m, ch, raw);
}

#endif /* HW_MODEL_PAC1952_H */
