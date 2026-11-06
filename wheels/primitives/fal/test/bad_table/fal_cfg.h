// SPDX-License-Identifier: MIT
/*
 * Partition tables for test_table_fal.c, one per TABLE_* build. The device
 * is 64 KB, and "good" fits in every table so only "bad" can be the reason.
 */
#ifndef FAL_CFG_H
#define FAL_CFG_H

extern const struct fal_flash_dev mock_flash_dev;

#define FAL_FLASH_DEV_TABLE \
{ \
    &mock_flash_dev, \
}

#if defined(TABLE_PAST_END)
/* Starts on the device, ends 0x1000 past it. */
#define BAD_PART {"bad", "mock_flash", 0xF000, 0x2000}
#elif defined(TABLE_OFFSET_PAST_END)
#define BAD_PART {"bad", "mock_flash", 0x10001, 0x10}
#elif defined(TABLE_NEGATIVE_OFFSET)
#define BAD_PART {"bad", "mock_flash", -1, 0x10}
#elif defined(TABLE_WRAPPING_LEN)
/* offset + len wraps size_t round to 0xFFF, which looks like it fits. */
#define BAD_PART {"bad", "mock_flash", 0x1000, (size_t)-1}
#elif defined(TABLE_UNKNOWN_DEVICE)
#define BAD_PART {"bad", "no_such_flash", 0, 0x10}
#else
#error "build with one TABLE_* defined"
#endif

#define FAL_PART_TABLE \
{ \
    {"good", "mock_flash", 0, 0x1000}, \
    BAD_PART, \
}

#endif /* FAL_CFG_H */
