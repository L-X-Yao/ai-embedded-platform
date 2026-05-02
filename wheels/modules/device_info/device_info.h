/*
 * device_info -- device identity and runtime information module
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none -- self-contained)
 *
 * Passive design: application sets fields, calls tick() once per second
 * to increment uptime. format() serializes all info to key=value text
 * for CLI display or telemetry output.
 */

#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Configuration macros (override before #include) ---------- */

#ifndef DEVICE_INFO_STR_SIZE
#define DEVICE_INFO_STR_SIZE    32
#endif

/* ---------- Error codes ---------- */

typedef enum {
    DEVICE_INFO_OK           =  0,
    DEVICE_INFO_ERR_PARAMS   = -1,
    DEVICE_INFO_ERR_OVERFLOW = -2,
} device_info_err_t;

/* ---------- Data structure ---------- */

typedef struct {
    char     firmware_version[DEVICE_INFO_STR_SIZE];
    char     hardware_version[DEVICE_INFO_STR_SIZE];
    char     serial_number[DEVICE_INFO_STR_SIZE];
    char     build_date[DEVICE_INFO_STR_SIZE];
    char     device_name[DEVICE_INFO_STR_SIZE];
    uint32_t uptime_seconds;
    uint32_t boot_count;
    uint32_t reset_reason;
} device_info_t;

/* ---------- API ---------- */

/**
 * Zero-initialize all fields.
 * @thread_safe: no
 */
device_info_err_t device_info_init(device_info_t *info);

/**
 * Set firmware version string. Truncates if longer than STR_SIZE-1.
 * @thread_safe: no
 */
device_info_err_t device_info_set_firmware_version(device_info_t *info,
                                                   const char *version);

/**
 * Set hardware version string.
 * @thread_safe: no
 */
device_info_err_t device_info_set_hardware_version(device_info_t *info,
                                                   const char *version);

/**
 * Set serial number string.
 * @thread_safe: no
 */
device_info_err_t device_info_set_serial_number(device_info_t *info,
                                                const char *serial);

/**
 * Set build date string.
 * @thread_safe: no
 */
device_info_err_t device_info_set_build_date(device_info_t *info,
                                             const char *date);

/**
 * Set device name string.
 * @thread_safe: no
 */
device_info_err_t device_info_set_device_name(device_info_t *info,
                                              const char *name);

/**
 * Set boot count (caller loads from persistent storage).
 * @thread_safe: no
 */
void device_info_set_boot_count(device_info_t *info, uint32_t count);

/**
 * Set reset reason code (MCU-specific).
 * @thread_safe: no
 */
void device_info_set_reset_reason(device_info_t *info, uint32_t reason);

/**
 * Call once per second to increment uptime counter.
 * @thread_safe: no
 */
void device_info_tick(device_info_t *info);

/**
 * Get current uptime in seconds.
 * @thread_safe: no
 */
uint32_t device_info_get_uptime(const device_info_t *info);

/**
 * Format all info as key=value lines into buf.
 * Returns number of bytes written (excluding NUL), or -1 on error.
 * If buf is too small, returns DEVICE_INFO_ERR_OVERFLOW and buf
 * contains a truncated but NUL-terminated string.
 * @thread_safe: no
 */
int device_info_format(const device_info_t *info, char *buf, uint16_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_INFO_H */
