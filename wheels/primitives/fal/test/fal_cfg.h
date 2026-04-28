#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

extern const struct fal_flash_dev mock_flash_dev;

#define FAL_FLASH_DEV_TABLE \
{ \
    &mock_flash_dev, \
}

#define FAL_PART_HAS_TABLE_CFG

#define FAL_PART_TABLE \
{ \
    {FAL_PART_MAGIC_WORD, "config",   "mock_flash",  0,                  4096,  0}, \
    {FAL_PART_MAGIC_WORD, "kvdb",     "mock_flash",  4096,               32768, 0}, \
    {FAL_PART_MAGIC_WORD, "firmware", "mock_flash",  4096 + 32768,       28672, 0}, \
}

#endif /* _FAL_CFG_H_ */
