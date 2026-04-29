/*
 * checksum — common checksum algorithms collection
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */
#include "checksum.h"

uint8_t checksum_xor(const uint8_t *data, size_t len)
{
    uint8_t cs = 0;
    if (!data) return 0;
    for (size_t i = 0; i < len; i++)
        cs ^= data[i];
    return cs;
}

uint8_t checksum_sum8(const uint8_t *data, size_t len)
{
    uint8_t cs = 0;
    if (!data) return 0;
    for (size_t i = 0; i < len; i++)
        cs = (uint8_t)(cs + data[i]);
    return cs;
}

uint16_t checksum_fletcher16(const uint8_t *data, size_t len)
{
    uint16_t sum1 = 0xFF, sum2 = 0xFF;
    if (!data) return 0;
    while (len > 0) {
        size_t block = len > 20 ? 20 : len;
        len -= block;
        while (block--) {
            sum1 = (uint16_t)(sum1 + *data++);
            sum2 = (uint16_t)(sum2 + sum1);
        }
        sum1 = (uint16_t)((sum1 & 0xFF) + (sum1 >> 8));
        sum2 = (uint16_t)((sum2 & 0xFF) + (sum2 >> 8));
    }
    sum1 = (uint16_t)((sum1 & 0xFF) + (sum1 >> 8));
    sum2 = (uint16_t)((sum2 & 0xFF) + (sum2 >> 8));
    return (uint16_t)((sum2 << 8) | sum1);
}

uint8_t checksum_lrc(const uint8_t *data, size_t len)
{
    uint8_t lrc = 0;
    if (!data) return 0;
    for (size_t i = 0; i < len; i++)
        lrc = (uint8_t)(lrc + data[i]);
    return (uint8_t)(~lrc + 1);
}

uint16_t checksum_internet(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    if (!data) return 0;
    while (len > 1) {
        sum += (uint32_t)((uint16_t)data[0] << 8 | data[1]);
        data += 2;
        len -= 2;
    }
    if (len == 1)
        sum += (uint32_t)((uint16_t)data[0] << 8);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (uint16_t)(~sum & 0xFFFF);
}
