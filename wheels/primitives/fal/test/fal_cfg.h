#ifndef FAL_CFG_H
#define FAL_CFG_H

extern const struct fal_flash_dev mock_flash_dev;

#define FAL_FLASH_DEV_TABLE \
{ \
    &mock_flash_dev, \
}

#define FAL_PART_TABLE \
{ \
    {"config",   "mock_flash",  0,                  4096 }, \
    {"kvdb",     "mock_flash",  4096,               32768}, \
    {"firmware", "mock_flash",  4096 + 32768,       28672}, \
}

#endif /* FAL_CFG_H */
