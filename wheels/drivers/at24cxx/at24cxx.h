/*
 * at24cxx --- I2C EEPROM driver for AT24C01 through AT24C256
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * Supports 8-bit address chips (AT24C01-AT24C16) and 16-bit address
 * chips (AT24C32-AT24C256).  Page-boundary splitting is handled
 * internally by at24cxx_write().  Write cycle delay is done via the
 * caller-provided delay_ms callback.
 *
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef AT24CXX_H
#define AT24CXX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    AT24CXX_OK         =  0,
    AT24CXX_ERR_I2C    = -1,
    AT24CXX_ERR_PARAM  = -2,
    AT24CXX_ERR_ADDR   = -3,
} at24cxx_err_t;

/* ---------- chip models (value = total bytes) ---------- */

typedef enum {
    AT24C01  =    128,   /*  1 Kbit */
    AT24C02  =    256,   /*  2 Kbit */
    AT24C04  =    512,   /*  4 Kbit */
    AT24C08  =   1024,   /*  8 Kbit */
    AT24C16  =   2048,   /* 16 Kbit */
    AT24C32  =   4096,   /* 32 Kbit */
    AT24C64  =   8192,   /* 64 Kbit */
    AT24C128 =  16384,   /* 128 Kbit */
    AT24C256 =  32768,   /* 256 Kbit */
} at24cxx_model_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    /** Write one byte: dev_addr(7-bit), reg(8-bit addr), val. */
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);

    /** Read len bytes starting at reg(8-bit addr). */
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);

    /**
     * Write len bytes: dev_addr, then 2-byte reg, then data.
     * Required for AT24C32 and above (16-bit memory address).
     * May be NULL if only 8-bit address chips are used.
     */
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint16_t reg,
                       const uint8_t *data, uint16_t len);

    /**
     * Read len bytes with 16-bit register address.
     * Required for AT24C32 and above.
     * May be NULL if only 8-bit address chips are used.
     */
    bool (*read_regs16)(void *ctx, uint8_t dev_addr, uint16_t reg,
                        uint8_t *buf, uint16_t len);

    /** Delay in milliseconds (used for write-cycle wait). */
    void (*delay_ms)(void *ctx, uint32_t ms);

    void *ctx;
} at24cxx_i2c_t;

/* ---------- device handle ---------- */

typedef struct {
    const at24cxx_i2c_t *i2c;
    uint8_t              dev_addr;   /* 7-bit base I2C address (0x50) */
    at24cxx_model_t      model;
    uint8_t              page_size;  /* internal -- set by init */
} at24cxx_t;

/* ---------- default I2C address ---------- */

#define AT24CXX_ADDR_DEFAULT  0x50u

/* ---------- write-cycle time (ms) ---------- */

#ifndef AT24CXX_WRITE_CYCLE_MS
#define AT24CXX_WRITE_CYCLE_MS  5u
#endif

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Initialise device handle.  @thread_safe: caller_must_lock */
at24cxx_err_t at24cxx_init(at24cxx_t *dev, const at24cxx_i2c_t *i2c,
                           uint8_t dev_addr, at24cxx_model_t model);

/** Read len bytes from EEPROM address addr.  @thread_safe: caller_must_lock */
at24cxx_err_t at24cxx_read(const at24cxx_t *dev, uint16_t addr,
                           uint8_t *buf, uint16_t len);

/**
 * Write len bytes to EEPROM.  Automatically splits across page
 * boundaries and inserts write-cycle delays.
 * @thread_safe: caller_must_lock
 */
at24cxx_err_t at24cxx_write(const at24cxx_t *dev, uint16_t addr,
                            const uint8_t *data, uint16_t len);

/** Write a single byte.  @thread_safe: caller_must_lock */
at24cxx_err_t at24cxx_write_byte(const at24cxx_t *dev, uint16_t addr,
                                 uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* AT24CXX_H */
