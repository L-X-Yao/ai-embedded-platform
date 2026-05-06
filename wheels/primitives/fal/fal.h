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

/**
 * Initialise FAL: call ops.init on every registered device.
 * Returns the number of partitions on success, -1 on failure.
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
 * Returns the number of bytes read on success, -1 on error (NULL pointer,
 * out-of-bounds, or device not found).
 * @thread_safe: caller_must_lock
 * @isr_safe: no
 */
int fal_partition_read(const struct fal_partition *part, uint32_t addr,
                       uint8_t *buf, size_t size);

/**
 * Write size bytes to a partition starting at addr (partition-relative).
 * NOR flash semantics apply: can only clear bits; erase before rewriting.
 * Returns the number of bytes written on success, -1 on error.
 * @thread_safe: caller_must_lock
 * @isr_safe: no
 */
int fal_partition_write(const struct fal_partition *part, uint32_t addr,
                        const uint8_t *buf, size_t size);

/**
 * Erase size bytes in a partition starting at addr (partition-relative).
 * size must be a multiple of the device block size in practice.
 * Returns the number of bytes erased on success, -1 on error.
 * @thread_safe: caller_must_lock
 * @isr_safe: no
 */
int fal_partition_erase(const struct fal_partition *part, uint32_t addr,
                        size_t size);

/**
 * Erase the entire partition.  Equivalent to fal_partition_erase(part, 0, part->len).
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
