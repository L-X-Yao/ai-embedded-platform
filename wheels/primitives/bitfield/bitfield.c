// SPDX-License-Identifier: MIT
/*
 * bitfield — bit-level pack/unpack from byte arrays
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "bitfield.h"
#include <stddef.h>
#include "contract.h"

int bitfield_get(const uint8_t *data, size_t data_len,
                 size_t bit_offset, size_t bit_count,
                 uint32_t *out)
{
    if (!data || !out)
        return BITFIELD_ERR_NULL;
    if (bit_count == 0 || bit_count > 32)
        return BITFIELD_ERR_WIDTH;
    if (bit_offset > data_len * 8 || bit_count > data_len * 8 - bit_offset)
        return BITFIELD_ERR_RANGE;

    uint32_t result = 0;
    for (size_t i = 0; i < bit_count; i++) {
        size_t pos = bit_offset + i;
        size_t byte_idx = pos / 8;
        size_t bit_idx  = 7 - (pos % 8);
        if (data[byte_idx] & (UINT32_C(1) << bit_idx))
            result |= (UINT32_C(1) << (bit_count - 1 - i));
    }
    *out = result;
    return BITFIELD_OK;
}

int bitfield_set(uint8_t *data, size_t data_len,
                 size_t bit_offset, size_t bit_count,
                 uint32_t value)
{
    if (!data)
        return BITFIELD_ERR_NULL;
    if (bit_count == 0 || bit_count > 32)
        return BITFIELD_ERR_WIDTH;
    if (bit_offset > data_len * 8 || bit_count > data_len * 8 - bit_offset)
        return BITFIELD_ERR_RANGE;

    for (size_t i = 0; i < bit_count; i++) {
        size_t pos = bit_offset + i;
        size_t byte_idx = pos / 8;
        size_t bit_idx  = 7 - (pos % 8);
        if (value & (UINT32_C(1) << (bit_count - 1 - i)))
            data[byte_idx] |= (uint8_t)(UINT32_C(1) << bit_idx);
        else
            data[byte_idx] &= (uint8_t)~(UINT32_C(1) << bit_idx);
    }
    return BITFIELD_OK;
}

bool bitfield_test_bit(const uint8_t *data, size_t data_len, size_t bit_index)
{
    if (!data || bit_index / 8 >= data_len)
        return false;
    size_t byte_idx = bit_index / 8;
    size_t bit_idx  = 7 - (bit_index % 8);
    return (data[byte_idx] & (UINT32_C(1) << bit_idx)) != 0;
}

void bitfield_set_bit(uint8_t *data, size_t data_len, size_t bit_index)
{
    if (!data || bit_index / 8 >= data_len)
        return;
    size_t byte_idx = bit_index / 8;
    size_t bit_idx  = 7 - (bit_index % 8);
    data[byte_idx] |= (uint8_t)(UINT32_C(1) << bit_idx);
}

void bitfield_clear_bit(uint8_t *data, size_t data_len, size_t bit_index)
{
    if (!data || bit_index / 8 >= data_len)
        return;
    size_t byte_idx = bit_index / 8;
    size_t bit_idx  = 7 - (bit_index % 8);
    data[byte_idx] &= (uint8_t)~(UINT32_C(1) << bit_idx);
}
