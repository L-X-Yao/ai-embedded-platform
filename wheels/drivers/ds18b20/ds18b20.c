// SPDX-License-Identifier: MIT
/*
 * ds18b20 — Dallas/Maxim DS18B20 1-Wire temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      onewire_hal (injected)
 * Instances: multi
 */
#include "ds18b20.h"
#include "contract.h"
#include <string.h>

#define CMD_SKIP_ROM         0xCC
#define CMD_MATCH_ROM        0x55
#define CMD_READ_ROM         0x33
#define CMD_CONVERT_T        0x44
#define CMD_READ_SCRATCHPAD  0xBE
#define CMD_WRITE_SCRATCHPAD 0x4E

/* Configuration register: R1 R0 in bits 6 and 5, 0 for 9 bits to 3 for 12. */
#define CONFIG_RES_SHIFT     5u
#define CONFIG_RES_MASK      0x03u
#define CONFIG_RES_12BIT     3u

/* CRC = X8 + X5 + X4 + 1, written for a register that shifts right. */
#define CRC8_POLY_REFLECTED  0x8C

uint8_t ds18b20_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            bool carry = (crc & 0x01) != 0;
            crc = (uint8_t)(crc >> 1);
            if (carry)
                crc ^= CRC8_POLY_REFLECTED;
        }
    }
    return crc;
}

static bool address_device(ds18b20_t *dev)
{
    if (dev->use_rom) {
        if (!dev->ow->write_byte(dev->ow->ctx, CMD_MATCH_ROM))
            return false;
        for (int i = 0; i < 8; i++)
            if (!dev->ow->write_byte(dev->ow->ctx, dev->rom[i]))
                return false;
    } else {
        if (!dev->ow->write_byte(dev->ow->ctx, CMD_SKIP_ROM))
            return false;
    }
    return true;
}

ds18b20_err_t ds18b20_init(ds18b20_t *dev, const ds18b20_ow_t *ow)
{
    if (!dev || !ow)
        return DS18B20_ERR_PARAM;
    if (!ow->reset || !ow->write_byte || !ow->read_byte)
        return DS18B20_ERR_PARAM;
    dev->ow = ow;
    memset(dev->rom, 0, 8);
    dev->use_rom = false;
    return DS18B20_OK;
}

ds18b20_err_t ds18b20_read_rom(ds18b20_t *dev)
{
    if (!dev)
        return DS18B20_ERR_PARAM;
    if (!dev->ow->reset(dev->ow->ctx))
        return DS18B20_ERR_NO_DEVICE;
    if (!dev->ow->write_byte(dev->ow->ctx, CMD_READ_ROM))
        return DS18B20_ERR_BUS;
    for (int i = 0; i < 8; i++)
        if (!dev->ow->read_byte(dev->ow->ctx, &dev->rom[i]))
            return DS18B20_ERR_BUS;
    if (ds18b20_crc8(dev->rom, 7) != dev->rom[7])
        return DS18B20_ERR_CRC;
    dev->use_rom = true;
    return DS18B20_OK;
}

ds18b20_err_t ds18b20_start_conversion(ds18b20_t *dev)
{
    if (!dev)
        return DS18B20_ERR_PARAM;
    if (!dev->ow->reset(dev->ow->ctx))
        return DS18B20_ERR_NO_DEVICE;
    if (!address_device(dev))
        return DS18B20_ERR_BUS;
    if (!dev->ow->write_byte(dev->ow->ctx, CMD_CONVERT_T))
        return DS18B20_ERR_BUS;
    return DS18B20_OK;
}

ds18b20_err_t ds18b20_read_temperature(ds18b20_t *dev, float *temp_c)
{
    if (!dev || !temp_c)
        return DS18B20_ERR_PARAM;
    if (!dev->ow->reset(dev->ow->ctx))
        return DS18B20_ERR_NO_DEVICE;
    if (!address_device(dev))
        return DS18B20_ERR_BUS;
    if (!dev->ow->write_byte(dev->ow->ctx, CMD_READ_SCRATCHPAD))
        return DS18B20_ERR_BUS;

    uint8_t sp[9];
    for (int i = 0; i < 9; i++)
        if (!dev->ow->read_byte(dev->ow->ctx, &sp[i]))
            return DS18B20_ERR_BUS;

    if (ds18b20_crc8(sp, 8) != sp[8])
        return DS18B20_ERR_CRC;

    /* Below 12-bit resolution the lowest bits are undefined: one per step. */
    unsigned res_steps = ((unsigned)sp[4] >> CONFIG_RES_SHIFT) & CONFIG_RES_MASK;
    unsigned undefined_bits = CONFIG_RES_12BIT - res_steps;
    uint16_t bits = (uint16_t)((uint16_t)sp[1] << 8 | sp[0]);
    bits = (uint16_t)(bits & (uint16_t)(0xFFFFu << undefined_bits));

    int16_t raw = (int16_t)bits;
    *temp_c = (float)raw / 16.0f;
    ENSURE(*temp_c == *temp_c);  /* not NaN */
    return DS18B20_OK;
}

ds18b20_err_t ds18b20_set_resolution(ds18b20_t *dev, ds18b20_resolution_t res)
{
    if (!dev)
        return DS18B20_ERR_PARAM;
    if (!dev->ow->reset(dev->ow->ctx))
        return DS18B20_ERR_NO_DEVICE;
    if (!address_device(dev))
        return DS18B20_ERR_BUS;
    if (!dev->ow->write_byte(dev->ow->ctx, CMD_WRITE_SCRATCHPAD))
        return DS18B20_ERR_BUS;
    if (!dev->ow->write_byte(dev->ow->ctx, 0x00))
        return DS18B20_ERR_BUS;
    if (!dev->ow->write_byte(dev->ow->ctx, 0x00))
        return DS18B20_ERR_BUS;
    if (!dev->ow->write_byte(dev->ow->ctx, (uint8_t)res))
        return DS18B20_ERR_BUS;
    return DS18B20_OK;
}
