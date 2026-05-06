#ifndef FAL_DEF_H
#define FAL_DEF_H

#include <stdint.h>
#include <stddef.h>

#ifndef FAL_DEV_NAME_MAX
#define FAL_DEV_NAME_MAX 24
#endif

struct fal_flash_dev {
    char name[FAL_DEV_NAME_MAX];
    uint32_t addr;
    size_t len;
    size_t blk_size;
    struct {
        int (*init)(void);
        int (*read)(long offset, uint8_t *buf, size_t size);
        int (*write)(long offset, const uint8_t *buf, size_t size);
        int (*erase)(long offset, size_t size);
    } ops;
    size_t write_gran;
};

struct fal_partition {
    char name[FAL_DEV_NAME_MAX];
    char flash_name[FAL_DEV_NAME_MAX];
    long offset;
    size_t len;
};

#endif /* FAL_DEF_H */
