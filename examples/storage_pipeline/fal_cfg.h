/* SPDX-License-Identifier: MIT */
#ifndef FAL_CFG_H
#define FAL_CFG_H

extern const struct fal_flash_dev mock_flash_dev;

#define FAL_FLASH_DEV_TABLE \
{ \
    &mock_flash_dev, \
}

/* 64KB mock flash: 4KB config + 48KB kvdb + 12KB spare */
#define FAL_PART_TABLE \
{ \
    {"config", "mock_flash",  0,     4096 }, \
    {"kvdb",   "mock_flash",  4096,  49152}, \
    {"spare",  "mock_flash",  53248, 12288}, \
}

#endif /* FAL_CFG_H */
