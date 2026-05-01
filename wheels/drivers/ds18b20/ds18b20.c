/*
 * ds18b20 — Dallas/Maxim DS18B20 1-Wire temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    onewire_hal (injected)
 */
#include "ds18b20.h"
#include <string.h>

#define CMD_SKIP_ROM         0xCC
#define CMD_MATCH_ROM        0x55
#define CMD_READ_ROM         0x33
#define CMD_CONVERT_T        0x44
#define CMD_READ_SCRATCHPAD  0xBE
#define CMD_WRITE_SCRATCHPAD 0x4E

static const uint8_t crc8_table[256] = {
    0,  94, 188, 226,  97,  63, 221, 131, 194, 156, 126,  32, 163, 253,  31,  65,
  157, 195,  33, 127, 252, 162,  64,  30,  95,   1, 227, 189,  62,  96, 130, 220,
   35, 125, 159, 193,  66,  28, 164, 254,  29,  67, 255, 165, 124,  34, 192, 158,
  225, 191,  93,   3, 128, 222,  60,  98, 190, 224,   2,  92, 223, 129,  99,  61,
  124,  34, 192, 158,  29,  67, 255, 165,  35, 125, 159, 193,  66,  28, 164, 254,
  225, 191,  93,   3, 128, 222,  60,  98, 190, 224,   2,  92, 223, 129,  99,  61,
    0,  94, 188, 226,  97,  63, 221, 131, 194, 156, 126,  32, 163, 253,  31,  65,
  157, 195,  33, 127, 252, 162,  64,  30,  95,   1, 227, 189,  62,  96, 130, 220,
   90,   6, 232, 178, 185, 227,  11,  89,  84, 250, 172,  38, 243, 161,  73,  23,
  170, 252,  46,  80, 161, 243,  25,  71,   4,  88, 180, 234,  99,  57, 131, 219,
  233, 179,  91,   5, 174,  36,  86, 248,  37, 175, 249,  87, 178, 232,   4,  90,
   81,  47, 253, 171,  70,  24, 242, 160,  57, 101, 219, 133,  88,   2, 234, 182,
  174,  36,  86, 248,  37, 175, 249,  87, 178, 232,   4,  90,  81,  47, 253, 171,
   70,  24, 242, 160,  57, 101, 219, 133,  88,   2, 234, 182, 233, 179,  91,   5,
   90,   6, 232, 178, 185, 227,  11,  89,  84, 250, 172,  38, 243, 161,  73,  23,
  170, 252,  46,  80, 161, 243,  25,  71,   4,  88, 180, 234,  99,  57, 131, 219
};

uint8_t ds18b20_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++)
        crc = crc8_table[crc ^ data[i]];
    return crc;
}

static bool address_device(ds18b20_t *dev)
{
    if (dev->use_rom) {
        if (!dev->ow.write_byte(dev->ow.ctx, CMD_MATCH_ROM))
            return false;
        for (int i = 0; i < 8; i++)
            if (!dev->ow.write_byte(dev->ow.ctx, dev->rom[i]))
                return false;
    } else {
        if (!dev->ow.write_byte(dev->ow.ctx, CMD_SKIP_ROM))
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
    dev->ow = *ow;
    memset(dev->rom, 0, 8);
    dev->use_rom = false;
    return DS18B20_OK;
}

ds18b20_err_t ds18b20_read_rom(ds18b20_t *dev)
{
    if (!dev)
        return DS18B20_ERR_PARAM;
    if (!dev->ow.reset(dev->ow.ctx))
        return DS18B20_ERR_NO_DEVICE;
    if (!dev->ow.write_byte(dev->ow.ctx, CMD_READ_ROM))
        return DS18B20_ERR_BUS;
    for (int i = 0; i < 8; i++)
        if (!dev->ow.read_byte(dev->ow.ctx, &dev->rom[i]))
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
    if (!dev->ow.reset(dev->ow.ctx))
        return DS18B20_ERR_NO_DEVICE;
    if (!address_device(dev))
        return DS18B20_ERR_BUS;
    if (!dev->ow.write_byte(dev->ow.ctx, CMD_CONVERT_T))
        return DS18B20_ERR_BUS;
    return DS18B20_OK;
}

ds18b20_err_t ds18b20_read_temperature(ds18b20_t *dev, float *temp_c)
{
    if (!dev || !temp_c)
        return DS18B20_ERR_PARAM;
    if (!dev->ow.reset(dev->ow.ctx))
        return DS18B20_ERR_NO_DEVICE;
    if (!address_device(dev))
        return DS18B20_ERR_BUS;
    if (!dev->ow.write_byte(dev->ow.ctx, CMD_READ_SCRATCHPAD))
        return DS18B20_ERR_BUS;

    uint8_t sp[9];
    for (int i = 0; i < 9; i++)
        if (!dev->ow.read_byte(dev->ow.ctx, &sp[i]))
            return DS18B20_ERR_BUS;

    if (ds18b20_crc8(sp, 8) != sp[8])
        return DS18B20_ERR_CRC;

    int16_t raw = (int16_t)((uint16_t)sp[1] << 8 | sp[0]);
    *temp_c = (float)raw / 16.0f;
    return DS18B20_OK;
}

ds18b20_err_t ds18b20_set_resolution(ds18b20_t *dev, ds18b20_resolution_t res)
{
    if (!dev)
        return DS18B20_ERR_PARAM;
    if (!dev->ow.reset(dev->ow.ctx))
        return DS18B20_ERR_NO_DEVICE;
    if (!address_device(dev))
        return DS18B20_ERR_BUS;
    if (!dev->ow.write_byte(dev->ow.ctx, CMD_WRITE_SCRATCHPAD))
        return DS18B20_ERR_BUS;
    if (!dev->ow.write_byte(dev->ow.ctx, 0x00))
        return DS18B20_ERR_BUS;
    if (!dev->ow.write_byte(dev->ow.ctx, 0x00))
        return DS18B20_ERR_BUS;
    if (!dev->ow.write_byte(dev->ow.ctx, (uint8_t)res))
        return DS18B20_ERR_BUS;
    return DS18B20_OK;
}
