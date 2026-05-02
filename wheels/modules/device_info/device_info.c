// SPDX-License-Identifier: MIT
/*
 * device_info -- device identity and runtime information module
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "device_info.h"
#include <string.h>
#include <stdio.h>

/* ---------- Internal helpers ---------- */

static device_info_err_t set_str(char *dst, size_t dst_size, const char *src)
{
    size_t len = strlen(src);

    if (len >= dst_size) {
        memcpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return DEVICE_INFO_ERR_OVERFLOW;
    }

    memcpy(dst, src, len + 1);
    return DEVICE_INFO_OK;
}

/* ---------- Public API ---------- */

device_info_err_t device_info_init(device_info_t *info)
{
    if (!info) {
        return DEVICE_INFO_ERR_PARAM;
    }

    memset(info, 0, sizeof(*info));
    return DEVICE_INFO_OK;
}

device_info_err_t device_info_set_firmware_version(device_info_t *info,
                                                   const char *version)
{
    if (!info || !version) {
        return DEVICE_INFO_ERR_PARAM;
    }
    return set_str(info->firmware_version, DEVICE_INFO_STR_SIZE, version);
}

device_info_err_t device_info_set_hardware_version(device_info_t *info,
                                                   const char *version)
{
    if (!info || !version) {
        return DEVICE_INFO_ERR_PARAM;
    }
    return set_str(info->hardware_version, DEVICE_INFO_STR_SIZE, version);
}

device_info_err_t device_info_set_serial_number(device_info_t *info,
                                                const char *serial)
{
    if (!info || !serial) {
        return DEVICE_INFO_ERR_PARAM;
    }
    return set_str(info->serial_number, DEVICE_INFO_STR_SIZE, serial);
}

device_info_err_t device_info_set_build_date(device_info_t *info,
                                             const char *date)
{
    if (!info || !date) {
        return DEVICE_INFO_ERR_PARAM;
    }
    return set_str(info->build_date, DEVICE_INFO_STR_SIZE, date);
}

device_info_err_t device_info_set_device_name(device_info_t *info,
                                              const char *name)
{
    if (!info || !name) {
        return DEVICE_INFO_ERR_PARAM;
    }
    return set_str(info->device_name, DEVICE_INFO_STR_SIZE, name);
}

void device_info_set_boot_count(device_info_t *info, uint32_t count)
{
    if (info) {
        info->boot_count = count;
    }
}

void device_info_set_reset_reason(device_info_t *info, uint32_t reason)
{
    if (info) {
        info->reset_reason = reason;
    }
}

void device_info_tick(device_info_t *info)
{
    if (info) {
        info->uptime_seconds++;
    }
}

uint32_t device_info_get_uptime(const device_info_t *info)
{
    if (!info) {
        return 0;
    }
    return info->uptime_seconds;
}

int device_info_format(const device_info_t *info, char *buf, uint16_t buf_size)
{
    if (!info || !buf || buf_size == 0) {
        return -1;
    }

    int n = snprintf(buf, buf_size,
        "device_name=%s\n"
        "firmware_version=%s\n"
        "hardware_version=%s\n"
        "serial_number=%s\n"
        "build_date=%s\n"
        "uptime_seconds=%lu\n"
        "boot_count=%lu\n"
        "reset_reason=%lu\n",
        info->device_name,
        info->firmware_version,
        info->hardware_version,
        info->serial_number,
        info->build_date,
        (unsigned long)info->uptime_seconds,
        (unsigned long)info->boot_count,
        (unsigned long)info->reset_reason);

    if (n < 0) {
        buf[0] = '\0';
        return -1;
    }

    if (n >= (int)buf_size) {
        /* snprintf truncated; buf is NUL-terminated by snprintf */
        return (int)DEVICE_INFO_ERR_OVERFLOW;
    }

    return n;
}
