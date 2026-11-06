// SPDX-License-Identifier: MIT
/*
 * fal — Flash Abstraction Layer
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none — project provides fal_cfg.h with device + partition tables)
 * Instances: single
 */

#ifndef FAL_H
#define FAL_H

#include <stdint.h>
#include <stddef.h>
#include "fal_def.h"
#include <fal_cfg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FAL_OK             =  0,
    FAL_ERR_PARAM      = -1,  /* NULL pointer or invalid argument */
    FAL_ERR_OVERFLOW   = -2,  /* addr + size exceeds partition size */
    FAL_ERR_HAL        = -3,  /* underlying device read/write/erase failure */
    FAL_ERR_NOT_READY  = -4,  /* device not found / not initialized */
    FAL_ERR_PART_TABLE = -5,  /* a partition does not fit inside its device */
} fal_err_t;

/**
 * Initialise FAL: check every partition against its device, then call
 * ops.init on every registered device.
 * Returns the number of partitions on success (>= 0), or a negative
 * fal_err_t value on failure: FAL_ERR_PART_TABLE if a partition does not
 * fit its device, and no device is touched. A partition that names no
 * registered device is not an error here: its calls return
 * FAL_ERR_NOT_READY.
 * Idempotent — subsequent calls return the partition count without
 * re-initialising devices.
 * @thread_safe: no
 * @isr_safe: no
 */
int fal_init(void);

/**
 * Return 1 if fal_init has been called, 0 otherwise.
 * @thread_safe: yes
 * @isr_safe: yes
 */
int fal_init_check(void);

/**
 * Look up a flash device by name.  Returns NULL if not found or name is NULL.
 * @thread_safe: yes
 * @isr_safe: yes
 */
const struct fal_flash_dev *fal_flash_device_find(const char *name);

/**
 * Look up a partition by name.  Returns NULL if not found or name is NULL.
 * @thread_safe: yes
 * @isr_safe: yes
 */
const struct fal_partition *fal_partition_find(const char *name);

/**
 * Return a pointer to the partition table and write the entry count to *len.
 * Returns NULL if len is NULL.
 * @thread_safe: yes
 * @isr_safe: yes
 */
const struct fal_partition *fal_get_partition_table(size_t *len);

/**
 * Read size bytes from a partition starting at addr (partition-relative).
 * Returns the number of bytes read on success (>= 0), or a negative
 * fal_err_t value on error (FAL_ERR_PARAM / FAL_ERR_OVERFLOW /
 * FAL_ERR_NOT_READY / FAL_ERR_HAL).
 * @thread_safe: caller_must_lock
 * @isr_safe: no
 */
int fal_partition_read(const struct fal_partition *part, uint32_t addr,
                       uint8_t *buf, size_t size);

/**
 * Write size bytes to a partition starting at addr (partition-relative).
 * NOR flash semantics apply: can only clear bits; erase before rewriting.
 * Returns the number of bytes written on success (>= 0), or a negative
 * fal_err_t value on error.
 * @thread_safe: caller_must_lock
 * @isr_safe: no
 */
int fal_partition_write(const struct fal_partition *part, uint32_t addr,
                        const uint8_t *buf, size_t size);

/**
 * Erase size bytes in a partition starting at addr (partition-relative).
 * size must be a multiple of the device block size in practice.
 * Returns the number of bytes erased on success (>= 0), or a negative
 * fal_err_t value on error.
 * @thread_safe: caller_must_lock
 * @isr_safe: no
 */
int fal_partition_erase(const struct fal_partition *part, uint32_t addr,
                        size_t size);

/**
 * Erase the entire partition.  Equivalent to fal_partition_erase(part, 0, part->len).
 * Returns the number of bytes erased (>= 0), or a negative fal_err_t.
 * @thread_safe: caller_must_lock
 * @isr_safe: no
 */
int fal_partition_erase_all(const struct fal_partition *part);

/**
 * Print the partition table to FAL_PRINTF (no-op if FAL_PRINTF not defined).
 * @thread_safe: no
 * @isr_safe: no
 */
void fal_show_part_table(void);

#ifdef __cplusplus
}
#endif

#endif /* FAL_H */
