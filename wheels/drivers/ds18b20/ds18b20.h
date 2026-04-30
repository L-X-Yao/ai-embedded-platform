/*
 * ds18b20 — Dallas/Maxim DS18B20 1-Wire temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    onewire_hal (injected)
 *
 * 1-Wire protocol: reset pulse, ROM commands, function commands.
 * Supports single-device bus (skip ROM) and multi-device (match ROM).
 * 12-bit resolution default (0.0625°C), configurable 9-12 bits.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the ds18b20_t struct.
 */

#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DS18B20_OK            =  0,
    DS18B20_ERR_PARAM     = -1,
    DS18B20_ERR_NO_DEVICE = -2,
    DS18B20_ERR_CRC       = -3,
    DS18B20_ERR_BUS       = -4,
} ds18b20_err_t;

typedef struct {
    bool (*reset)(void *ctx);
    void (*write_byte)(void *ctx, uint8_t byte);
    uint8_t (*read_byte)(void *ctx);
    void *ctx;
} ds18b20_ow_t;

typedef enum {
    DS18B20_RES_9BIT  = 0x1F,
    DS18B20_RES_10BIT = 0x3F,
    DS18B20_RES_11BIT = 0x5F,
    DS18B20_RES_12BIT = 0x7F,
} ds18b20_resolution_t;

typedef struct {
    ds18b20_ow_t ow;
    uint8_t      rom[8];
    bool         use_rom;
} ds18b20_t;

/** @thread_safe: no */
ds18b20_err_t ds18b20_init(ds18b20_t *dev, const ds18b20_ow_t *ow);

ds18b20_err_t ds18b20_read_rom(ds18b20_t *dev);

ds18b20_err_t ds18b20_start_conversion(ds18b20_t *dev);

ds18b20_err_t ds18b20_read_temperature(ds18b20_t *dev, float *temp_c);

ds18b20_err_t ds18b20_set_resolution(ds18b20_t *dev, ds18b20_resolution_t res);

uint8_t ds18b20_crc8(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_H */
