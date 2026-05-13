// SPDX-License-Identifier: MIT
/*
 * device_info_demo -- integration recipe: device identity banner
 *
 * Standard pattern at the start of every embedded firmware: print a
 * banner with firmware version, hardware revision, serial number,
 * build date, and assigned device UUID, then keep ticking uptime so
 * the same info block can be re-emitted on demand (e.g. shell `info`
 * command).  Combines four wheels:
 *
 *   L0  log          -- structured leveled log with stdout sink
 *   L0  uuid         -- generate a per-boot UUID v4 device ID
 *   L3  version_info -- parse + compare semantic firmware version
 *   L3  device_info  -- hold and format the device-identity block
 *
 * Runs on host. No real flash or RTC needed -- uptime is incremented
 * synchronously and uuid v4 uses a deterministic PRNG so output is
 * reproducible run-to-run.
 *
 * Build:  cmake -S . -B build && cmake --build build && ./build/device_info_demo
 */

#define LOG_MODULE "boot"
#include "log.h"

#include "uuid.h"
#include "version_info.h"
#include "device_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ======================================================================
 * Build-time identity (in real firmware these come from build system)
 * ====================================================================== */

#define FIRMWARE_VERSION   "2.4.1"
#define MIN_REQUIRED_VER   "2.0.0"     /* OTA compatibility floor */
#define HARDWARE_VERSION   "rev-C2"
#define SERIAL_NUMBER      "AEP-2026-00347"
#define DEVICE_NAME        "FlyboxPro"
#define BUILD_DATE         "2026-05-13"

/* ======================================================================
 * Deterministic PRNG injected into uuid wheel
 *
 * On a real MCU, this would pull from a hardware RNG, a TRNG peripheral,
 * or a seeded LFSR.  For the demo we want reproducible output, so we use
 * a tiny xorshift32 seeded with a fixed constant.
 * ====================================================================== */

static uint32_t g_prng_state = 0xDEADBEEF;

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void demo_rng(void *ctx, uint8_t *buf, uint16_t len)
{
    (void)ctx;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(xorshift32(&g_prng_state) & 0xFF);
    }
}

/* ======================================================================
 * Simulated tick source -- 1 Hz uptime counter
 *
 * In a real device, an RTC IRQ or systick would call device_info_tick()
 * once per second.  Here we just call it in a loop and print the banner
 * a couple of times to show uptime advancing.
 * ====================================================================== */

static uint32_t g_ticks_ms = 0;

static uint32_t demo_get_tick_ms(void)
{
    return g_ticks_ms;
}

/* ======================================================================
 * Banner emit -- formats device_info via the wheel and routes each line
 * through the log subsystem, one INFO line per record.
 * ====================================================================== */

static void emit_banner(const device_info_t *info, const char *device_uuid_str)
{
    char buf[512];
    int n = device_info_format(info, buf, sizeof(buf));
    if (n < 0) {
        LOG_ERROR("device_info_format failed: rc=%d", n);
        return;
    }

    LOG_INFO("============= DEVICE BANNER ============");
    /* Walk the key=value lines emitted by device_info_format(), and
     * forward each as its own INFO log line so the sink sees structured
     * fields instead of one giant blob. */
    const char *p = buf;
    const char *end = buf + n;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        size_t line_len = nl ? (size_t)(nl - p) : (size_t)(end - p);
        if (line_len > 0) {
            /* slice into a temporary NUL-terminated string */
            char line[128];
            if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
            memcpy(line, p, line_len);
            line[line_len] = '\0';
            LOG_INFO("  %s", line);
        }
        if (!nl) break;
        p = nl + 1;
    }
    LOG_INFO("  device_uuid=%s", device_uuid_str);
    LOG_INFO("========================================");
}

/* ======================================================================
 * Version compatibility check -- demonstrate version_info
 * ====================================================================== */

static int check_firmware_compatibility(void)
{
    version_t actual, required;
    if (version_parse(FIRMWARE_VERSION, &actual) != VERSION_INFO_OK) {
        LOG_ERROR("could not parse firmware version '%s'", FIRMWARE_VERSION);
        return -1;
    }
    if (version_parse(MIN_REQUIRED_VER, &required) != VERSION_INFO_OK) {
        LOG_ERROR("could not parse required version '%s'", MIN_REQUIRED_VER);
        return -1;
    }

    char actual_str[16];
    char required_str[16];
    version_to_string(&actual,   actual_str,   sizeof(actual_str));
    version_to_string(&required, required_str, sizeof(required_str));

    int cmp = version_compare(&actual, &required);
    bool compat = version_compatible(&required, &actual);

    LOG_INFO("firmware version: %s (required >= %s)", actual_str, required_str);
    LOG_INFO("version_compare = %d  (negative = older, 0 = equal, positive = newer)", cmp);
    LOG_INFO("version_compatible = %s", compat ? "YES" : "NO");

    if (!compat) {
        LOG_ERROR("firmware is BELOW required floor -- refuse to boot");
        return -1;
    }
    return 0;
}

/* ======================================================================
 * main -- run the boot scenario
 * ====================================================================== */

int main(void)
{
    /* 1. Wire the log subsystem to stdout (no timestamps for reproducibility) */
    log_add_stdout_sink();
    log_set_level(LOG_LEVEL_DEBUG);

    printf("=== device_info_demo -- identity banner ===\n");
    printf("Wheels: log + uuid + version_info + device_info\n");
    printf("==========================================\n\n");

    LOG_INFO("system boot");

    /* 2. Firmware version compatibility check */
    if (check_firmware_compatibility() != 0) {
        LOG_ERROR("aborting boot");
        return 1;
    }

    /* 3. Generate a device UUID for this run (per-boot ID) */
    uuid_t dev_uuid;
    if (uuid_v4_generate(&dev_uuid, demo_rng, NULL) != UUID_OK) {
        LOG_ERROR("uuid_v4_generate failed");
        return 1;
    }
    char uuid_str[37];
    if (uuid_to_string(&dev_uuid, uuid_str, sizeof(uuid_str)) != UUID_OK) {
        LOG_ERROR("uuid_to_string failed");
        return 1;
    }

    /* 4. Populate the device_info block */
    device_info_t info;
    if (device_info_init(&info) != DEVICE_INFO_OK) {
        LOG_ERROR("device_info_init failed");
        return 1;
    }
    device_info_set_firmware_version(&info, FIRMWARE_VERSION);
    device_info_set_hardware_version(&info, HARDWARE_VERSION);
    device_info_set_serial_number(&info, SERIAL_NUMBER);
    device_info_set_device_name(&info, DEVICE_NAME);
    device_info_set_build_date(&info, BUILD_DATE);
    /* Simulate values that would normally come from persistent storage */
    device_info_set_boot_count(&info, 42);
    device_info_set_reset_reason(&info, 0x01);  /* POR */

    /* 5. Emit the banner */
    emit_banner(&info, uuid_str);

    /* 6. Tick uptime forward, re-emit banner to show it advancing.
     *    In a real device this would be hours later via shell command. */
    LOG_INFO("simulating 5 seconds of uptime...");
    for (int s = 0; s < 5; s++) {
        device_info_tick(&info);
        g_ticks_ms += 1000;
    }
    log_set_time_fn(demo_get_tick_ms);
    LOG_INFO("uptime now %lu seconds (expected 5)",
             (unsigned long)device_info_get_uptime(&info));

    emit_banner(&info, uuid_str);

    /* 7. Demonstrate truncation handling -- field too long should not crash */
    {
        const char *too_long =
            "this-is-a-very-long-serial-number-that-exceeds-the-32-byte-limit";
        device_info_err_t rc =
            device_info_set_serial_number(&info, too_long);
        if (rc == DEVICE_INFO_ERR_OVERFLOW) {
            LOG_WARN("serial truncated by device_info (expected behaviour)");
        } else {
            LOG_ERROR("expected ERR_OVERFLOW, got rc=%d", rc);
            return 1;
        }
        LOG_INFO("truncated serial: %s", info.serial_number);
    }

    /* 8. Self-check: assert we hit the expected wheel-driven uptime */
    if (device_info_get_uptime(&info) != 5) {
        LOG_ERROR("uptime mismatch: got %lu, expected 5",
                  (unsigned long)device_info_get_uptime(&info));
        return 1;
    }
    if (strlen(uuid_str) != 36) {
        LOG_ERROR("uuid_str wrong length: %zu", strlen(uuid_str));
        return 1;
    }

    LOG_INFO("device_info_demo complete");
    printf("\n=== device_info_demo complete ===\n");
    return 0;
}
