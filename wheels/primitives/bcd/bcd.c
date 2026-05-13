// SPDX-License-Identifier: MIT
#include "bcd.h"

bool bcd_is_valid(uint8_t bcd)
{
    return ((bcd >> 4) <= 9) && ((bcd & 0x0F) <= 9);
}

uint8_t bcd_to_bin(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

uint8_t bcd_from_bin(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

bcd_err_t bcd_to_bin_safe(uint8_t bcd, uint8_t *out)
{
    if (!out)
        return BCD_ERR_PARAM;
    if (!bcd_is_valid(bcd))
        return BCD_ERR_INVALID;
    *out = bcd_to_bin(bcd);
    return BCD_OK;
}

bcd_err_t bcd_from_bin_safe(uint8_t bin, uint8_t *out)
{
    if (!out)
        return BCD_ERR_PARAM;
    if (bin > 99)
        return BCD_ERR_RANGE;
    *out = bcd_from_bin(bin);
    return BCD_OK;
}

bcd_err_t bcd_buf_to_bin(const uint8_t *bcd, uint8_t *bin, uint16_t len)
{
    if (!bcd || !bin)
        return BCD_ERR_PARAM;
    for (uint16_t i = 0; i < len; i++) {
        if (!bcd_is_valid(bcd[i]))
            return BCD_ERR_INVALID;
        bin[i] = bcd_to_bin(bcd[i]);
    }
    return BCD_OK;
}

bcd_err_t bcd_buf_from_bin(const uint8_t *bin, uint8_t *bcd, uint16_t len)
{
    if (!bin || !bcd)
        return BCD_ERR_PARAM;
    for (uint16_t i = 0; i < len; i++) {
        if (bin[i] > 99)
            return BCD_ERR_RANGE;
        bcd[i] = bcd_from_bin(bin[i]);
    }
    return BCD_OK;
}
