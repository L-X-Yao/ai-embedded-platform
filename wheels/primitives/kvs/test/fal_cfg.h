#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

extern const struct fal_flash_dev mock_flash_dev;

#define FAL_FLASH_DEV_TABLE \
{ \
    &mock_flash_dev, \
}

#define FAL_PART_HAS_TABLE_CFG

/* 64KB mock flash: 4KB config + 48KB kvdb + 12KB tsdb */
#define FAL_PART_TABLE \
{ \
    {FAL_PART_MAGIC_WORD, "config", "mock_flash",  0,     4096,  0}, \
    {FAL_PART_MAGIC_WORD, "kvdb",   "mock_flash",  4096,  49152, 0}, \
    {FAL_PART_MAGIC_WORD, "tsdb",   "mock_flash",  53248, 12288, 0}, \
}

#endif /* _FAL_CFG_H_ */
