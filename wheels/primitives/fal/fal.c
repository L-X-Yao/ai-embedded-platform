// SPDX-License-Identifier: MIT
/*
 * fal — Flash Abstraction Layer implementation
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 * Instances: single
 */

#include "fal.h"
#include "contract.h"
#include <string.h>

#ifndef FAL_FLASH_DEV_TABLE
#error "FAL_FLASH_DEV_TABLE must be defined in fal_cfg.h"
#endif

#ifndef FAL_PART_TABLE
#error "FAL_PART_TABLE must be defined in fal_cfg.h"
#endif

static const struct fal_flash_dev *const s_devs[] = FAL_FLASH_DEV_TABLE;
static const size_t s_dev_count = sizeof(s_devs) / sizeof(s_devs[0]);

static const struct fal_partition s_parts[] = FAL_PART_TABLE;
static const size_t s_part_count = sizeof(s_parts) / sizeof(s_parts[0]);

static uint8_t s_init_ok;

int fal_init(void)
{
    if (s_init_ok) {
        return (int)s_part_count;
    }
    for (size_t i = 0; i < s_dev_count; i++) {
        if (s_devs[i]->ops.init) {
            s_devs[i]->ops.init();
        }
    }
    s_init_ok = 1;
    return (int)s_part_count;
}

int fal_init_check(void)
{
    return s_init_ok;
}

const struct fal_flash_dev *fal_flash_device_find(const char *name)
{
    REQUIRE(name != NULL);
    if (!name) {
        return NULL;
    }
    for (size_t i = 0; i < s_dev_count; i++) {
        if (strncmp(name, s_devs[i]->name, FAL_DEV_NAME_MAX) == 0) {
            return s_devs[i];
        }
    }
    return NULL;
}

const struct fal_partition *fal_partition_find(const char *name)
{
    REQUIRE(name != NULL);
    if (!name) {
        return NULL;
    }
    for (size_t i = 0; i < s_part_count; i++) {
        if (strcmp(name, s_parts[i].name) == 0) {
            return &s_parts[i];
        }
    }
    return NULL;
}

const struct fal_partition *fal_get_partition_table(size_t *len)
{
    REQUIRE(len != NULL);
    if (!len) {
        return NULL;
    }
    *len = s_part_count;
    return s_parts;
}

static const struct fal_flash_dev *find_dev_by_part(
    const struct fal_partition *part)
{
    return fal_flash_device_find(part->flash_name);
}

int fal_partition_read(const struct fal_partition *part, uint32_t addr,
                       uint8_t *buf, size_t size)
{
    REQUIRE(part != NULL);
    REQUIRE(buf != NULL);
    if (!part || !buf) {
        return FAL_ERR_PARAM;
    }
    if ((size_t)addr + size > part->len) {
        return FAL_ERR_OVERFLOW;
    }
    const struct fal_flash_dev *dev = find_dev_by_part(part);
    if (!dev || !dev->ops.read) {
        return FAL_ERR_NOT_READY;
    }
    int rc = dev->ops.read(part->offset + (long)addr, buf, size);
    return rc < 0 ? FAL_ERR_HAL : rc;
}

int fal_partition_write(const struct fal_partition *part, uint32_t addr,
                        const uint8_t *buf, size_t size)
{
    REQUIRE(part != NULL);
    REQUIRE(buf != NULL);
    if (!part || !buf) {
        return FAL_ERR_PARAM;
    }
    if ((size_t)addr + size > part->len) {
        return FAL_ERR_OVERFLOW;
    }
    const struct fal_flash_dev *dev = find_dev_by_part(part);
    if (!dev || !dev->ops.write) {
        return FAL_ERR_NOT_READY;
    }
    int rc = dev->ops.write(part->offset + (long)addr, buf, size);
    return rc < 0 ? FAL_ERR_HAL : rc;
}

int fal_partition_erase(const struct fal_partition *part, uint32_t addr,
                        size_t size)
{
    REQUIRE(part != NULL);
    if (!part) {
        return FAL_ERR_PARAM;
    }
    if ((size_t)addr + size > part->len) {
        return FAL_ERR_OVERFLOW;
    }
    const struct fal_flash_dev *dev = find_dev_by_part(part);
    if (!dev || !dev->ops.erase) {
        return FAL_ERR_NOT_READY;
    }
    int rc = dev->ops.erase(part->offset + (long)addr, size);
    return rc < 0 ? FAL_ERR_HAL : rc;
}

int fal_partition_erase_all(const struct fal_partition *part)
{
    REQUIRE(part != NULL);
    if (!part) {
        return FAL_ERR_PARAM;
    }
    return fal_partition_erase(part, 0, part->len);
}

void fal_show_part_table(void)
{
#ifdef FAL_PRINTF
    for (size_t i = 0; i < s_part_count; i++) {
        FAL_PRINTF("| %-16s | %-16s | 0x%08lx | 0x%08lx |\n",
                   s_parts[i].name, s_parts[i].flash_name,
                   (unsigned long)s_parts[i].offset,
                   (unsigned long)s_parts[i].len);
    }
#endif
}
