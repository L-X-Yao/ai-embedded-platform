// SPDX-License-Identifier: MIT
/*
 * hw_model_at24cxx.h -- AT24CXX I2C EEPROM hardware model
 *
 * Models the EEPROM of the AT24C128/256 manual (0670T-SEEPR-3/07):
 * - Byte storage, erased to 0xFF, for the AT24C02 (8-bit word address, 8-byte
 *   page) by default or any other part through at24cxx_hw_model_init_part
 * - A page write wraps inside its page (p.10)
 * - A read rolls over from the last byte to the first (p.11)
 * - Only the device address that compares gets an ACK (p.10)
 * - No transfer gets an ACK during a write cycle, tWR after the STOP (p.10).
 *   Time passes only through the delay callback.
 *
 * Block addressing in the device address of the AT24C04 to C16 is not modelled,
 * and that manual does not cover the AT24C02: its page size is the driver's.
 */

#ifndef HW_MODEL_AT24CXX_H
#define HW_MODEL_AT24CXX_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define AT24CXX_MODEL_SIZE      256u     /* AT24C02: 256 bytes */
#define AT24CXX_MODEL_MAX_SIZE  32768u   /* AT24C256 */
/* p.5: tWR at 1.8 V, the longest. Only parts marked B promise 5 ms. */
#define AT24CXX_MODEL_TWR_MS    20u

typedef struct {
    uint8_t        mem[AT24CXX_MODEL_MAX_SIZE];
    uint32_t       size;
    uint16_t       page;          /* a power of two */
    uint8_t        addr;          /* the 7-bit address the A pins set */
    uint32_t       busy_left_ms;  /* write cycle still running */
    int            write_count;
    int            delay_count;
} at24cxx_hw_model_t;

__attribute__((unused))
static void at24cxx_hw_model_init_part(at24cxx_hw_model_t *m, uint32_t size,
                                       uint16_t page, uint8_t addr)
{
    memset(m, 0, sizeof(*m));
    m->size = size;
    m->page = page;
    m->addr = addr;

    /* EEPROM starts erased (0xFF) */
    memset(m->mem, 0xFF, size);
}

static void at24cxx_hw_model_init(at24cxx_hw_model_t *m)
{
    at24cxx_hw_model_init_part(m, AT24CXX_MODEL_SIZE, 8u, 0x50);
}

/* p.10: a device address that does not compare gets no ACK, and "All inputs are
 * disabled during this write cycle". */
static bool at24cxx_model_responds(const at24cxx_hw_model_t *m, uint8_t dev_addr)
{
    return dev_addr == m->addr && m->busy_left_ms == 0;
}

/* p.10: the lower word address bits increment and "When the word address,
 * internally generated, reaches the page boundary, the following byte is
 * placed at the beginning of the same page." */
static void at24cxx_model_store(at24cxx_hw_model_t *m, uint32_t addr,
                                const uint8_t *data, uint16_t len)
{
    uint32_t mask = (uint32_t)m->page - 1u;
    uint32_t base = addr & ~mask;

    for (uint16_t i = 0; i < len; i++)
        m->mem[(base | ((addr + i) & mask)) % m->size] = data[i];
    m->write_count++;
    m->busy_left_ms = AT24CXX_MODEL_TWR_MS;
}

/* p.11: "The address "roll over" during read is from the last byte of the last
 * memory page, to the first byte of the first page." */
static void at24cxx_model_load(const at24cxx_hw_model_t *m, uint32_t addr,
                               uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
        buf[i] = m->mem[(addr + i) % m->size];
}

/* I2C write_reg callback (8-bit address, single byte). */
__attribute__((unused))
static bool at24cxx_model_i2c_write_reg(void *ctx, uint8_t dev_addr,
                                        uint8_t reg, uint8_t val)
{
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    if (!at24cxx_model_responds(m, dev_addr))
        return false;
    at24cxx_model_store(m, reg, &val, 1);
    return true;
}

/* I2C write_regs8 callback (8-bit address, multi-byte page write). */
__attribute__((unused))
static bool at24cxx_model_i2c_write_regs8(void *ctx, uint8_t dev_addr,
                                          uint8_t reg,
                                          const uint8_t *data, uint16_t len)
{
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    if (!at24cxx_model_responds(m, dev_addr))
        return false;
    at24cxx_model_store(m, reg, data, len);
    return true;
}

/* I2C read_regs callback (8-bit address, sequential read). */
__attribute__((unused))
static bool at24cxx_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                        uint8_t reg,
                                        uint8_t *buf, uint16_t len)
{
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    if (!at24cxx_model_responds(m, dev_addr))
        return false;
    at24cxx_model_load(m, reg, buf, len);
    return true;
}

/* I2C write_regs callback (16-bit address, page write). */
__attribute__((unused))
static bool at24cxx_model_i2c_write_regs(void *ctx, uint8_t dev_addr,
                                         uint16_t reg,
                                         const uint8_t *data, uint16_t len)
{
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    if (!at24cxx_model_responds(m, dev_addr))
        return false;
    at24cxx_model_store(m, reg, data, len);
    return true;
}

/* I2C read_regs16 callback (16-bit address, sequential read). */
__attribute__((unused))
static bool at24cxx_model_i2c_read_regs16(void *ctx, uint8_t dev_addr,
                                          uint16_t reg,
                                          uint8_t *buf, uint16_t len)
{
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    if (!at24cxx_model_responds(m, dev_addr))
        return false;
    at24cxx_model_load(m, reg, buf, len);
    return true;
}

/* Delay callback: the only clock the model has */
__attribute__((unused))
static void at24cxx_model_delay(void *ctx, uint32_t ms)
{
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    m->delay_count++;
    m->busy_left_ms = (ms < m->busy_left_ms) ? m->busy_left_ms - ms : 0u;
}

/* Helper: pre-fill EEPROM contents */
static void at24cxx_model_fill(at24cxx_hw_model_t *m, uint8_t addr,
                                const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len && ((uint16_t)addr + i) < m->size; i++) {
        m->mem[(uint16_t)addr + i] = data[i];
    }
}

#endif /* HW_MODEL_AT24CXX_H */
