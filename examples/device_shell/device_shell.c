// SPDX-License-Identifier: MIT
/*
 * device_shell -- integration recipe: Device Configuration Shell
 *
 * Combines 5 L3 wheels:
 *   cli_shell    — command parsing and dispatch
 *   config_store — typed configuration with validation and persistence
 *   param_table  — string-based parameter table with CLI get/set/list
 *   version_info — firmware version handling
 *   error_handler— fault logging ring buffer
 *
 * Demonstrates the "configuration terminal" pattern common in embedded
 * devices: a serial shell for reading/writing device parameters,
 * checking firmware version, and inspecting error logs.
 *
 * All I/O is simulated (stdin/stdout via write callback), and
 * config_store persistence uses a RAM buffer as mock flash.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "cli_shell.h"
#include "config_store.h"
#include "param_table.h"
#include "version_info.h"
#include "error_handler.h"

/* ======================================================================
 * Firmware version (compile-time constant)
 * ====================================================================== */

#define FW_VERSION_STR  "1.3.7"

/* ======================================================================
 * Mock flash storage for config_store
 *
 * A simple RAM-backed KV store that simulates flash persistence.
 * Each slot holds a key string and a raw config_value_t blob.
 * ====================================================================== */

#define MOCK_FLASH_SLOTS  16

typedef struct {
    char     key[32];
    uint8_t  data[sizeof(config_value_t)];
    uint16_t len;
    bool     occupied;
} mock_flash_slot_t;

static mock_flash_slot_t g_flash[MOCK_FLASH_SLOTS];

static config_store_err_t mock_flash_save(void *ctx, const char *key,
                                          const uint8_t *data, uint16_t len)
{
    (void)ctx;
    /* Find existing slot or first free slot */
    int free_idx = -1;
    for (int i = 0; i < MOCK_FLASH_SLOTS; i++) {
        if (g_flash[i].occupied && strcmp(g_flash[i].key, key) == 0) {
            /* Overwrite existing */
            if (len > sizeof(g_flash[i].data)) {
                return CONFIG_STORE_ERR_STORAGE;
            }
            memcpy(g_flash[i].data, data, len);
            g_flash[i].len = len;
            return CONFIG_STORE_OK;
        }
        if (!g_flash[i].occupied && free_idx < 0) {
            free_idx = i;
        }
    }
    if (free_idx < 0) {
        return CONFIG_STORE_ERR_STORAGE;  /* no room */
    }
    strncpy(g_flash[free_idx].key, key, sizeof(g_flash[free_idx].key) - 1);
    g_flash[free_idx].key[sizeof(g_flash[free_idx].key) - 1] = '\0';
    if (len > sizeof(g_flash[free_idx].data)) {
        return CONFIG_STORE_ERR_STORAGE;
    }
    memcpy(g_flash[free_idx].data, data, len);
    g_flash[free_idx].len      = len;
    g_flash[free_idx].occupied = true;
    return CONFIG_STORE_OK;
}

static config_store_err_t mock_flash_load(void *ctx, const char *key,
                                          uint8_t *data, uint16_t len)
{
    (void)ctx;
    for (int i = 0; i < MOCK_FLASH_SLOTS; i++) {
        if (g_flash[i].occupied && strcmp(g_flash[i].key, key) == 0) {
            uint16_t copy_len = g_flash[i].len < len ? g_flash[i].len : len;
            memcpy(data, g_flash[i].data, copy_len);
            return CONFIG_STORE_OK;
        }
    }
    return CONFIG_STORE_ERR_STORAGE;  /* not found in flash */
}

/* ======================================================================
 * Shell output capture
 *
 * The CLI shell writes output via a callback.  We print it to stdout
 * so the user can see responses to each command.
 * ====================================================================== */

static void shell_write(void *ctx, const char *data, uint16_t len)
{
    (void)ctx;
    fwrite(data, 1, len, stdout);
}

/* ======================================================================
 * Application context — bundles all wheel instances
 * ====================================================================== */

typedef struct {
    cli_shell_t     shell;
    config_store_t  config;
    param_table_t   params;
    error_handler_t errors;
    version_t       fw_version;
} app_ctx_t;

static app_ctx_t g_app;

/* ======================================================================
 * CLI command handlers
 * ====================================================================== */

/* --- "config get <key>" / "config set <key> <value>" / "config list" --- */

static void cmd_config(void *ctx, int argc, const char **argv,
                       const cli_shell_io_t *io)
{
    app_ctx_t *app = (app_ctx_t *)ctx;
    char buf[128];

    if (argc < 2) {
        io->write(io->ctx, "usage: config get|set|list\r\n", 28);
        return;
    }

    if (strcmp(argv[1], "get") == 0) {
        if (argc < 3) {
            io->write(io->ctx, "usage: config get <key>\r\n", 25);
            return;
        }
        /* Try param_table first (string-based, CLI-friendly) */
        char val[PARAM_VALUE_SIZE];
        param_err_t err = param_table_get(&app->params, argv[2],
                                          val, sizeof(val));
        if (err == PARAM_OK) {
            int n = snprintf(buf, sizeof(buf), "%s = %s\r\n", argv[2], val);
            if (n > 0) {
                io->write(io->ctx, buf, (uint16_t)n);
            }
        } else {
            int n = snprintf(buf, sizeof(buf),
                             "error: key '%s' not found\r\n", argv[2]);
            if (n > 0) {
                io->write(io->ctx, buf, (uint16_t)n);
            }
        }

    } else if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            io->write(io->ctx, "usage: config set <key> <value>\r\n", 33);
            return;
        }
        param_err_t err = param_table_set(&app->params, argv[2], argv[3]);
        if (err == PARAM_OK) {
            /* Sync to config_store for persistence */
            config_store_set_string(&app->config, argv[2], argv[3]);

            int n = snprintf(buf, sizeof(buf), "OK: %s = %s\r\n",
                             argv[2], argv[3]);
            if (n > 0) {
                io->write(io->ctx, buf, (uint16_t)n);
            }
        } else if (err == PARAM_ERR_NOT_FOUND) {
            int n = snprintf(buf, sizeof(buf),
                             "error: key '%s' not found\r\n", argv[2]);
            if (n > 0) {
                io->write(io->ctx, buf, (uint16_t)n);
            }
        } else if (err == PARAM_ERR_READONLY) {
            int n = snprintf(buf, sizeof(buf),
                             "error: '%s' is read-only\r\n", argv[2]);
            if (n > 0) {
                io->write(io->ctx, buf, (uint16_t)n);
            }
        } else {
            io->write(io->ctx, "error: set failed\r\n", 19);
        }

    } else if (strcmp(argv[1], "list") == 0) {
        char list_buf[512];
        int n = param_table_list(&app->params, list_buf, sizeof(list_buf));
        if (n > 0) {
            io->write(io->ctx, list_buf, (uint16_t)n);
        }

    } else {
        io->write(io->ctx, "usage: config get|set|list\r\n", 28);
    }
}

/* --- "status" — show device summary --- */

static void cmd_status(void *ctx, int argc, const char **argv,
                       const cli_shell_io_t *io)
{
    (void)argc;
    (void)argv;
    app_ctx_t *app = (app_ctx_t *)ctx;
    char buf[128];
    int n;

    io->write(io->ctx, "--- Device Status ---\r\n", 23);

    /* Firmware version */
    char ver_str[16];
    version_to_string(&app->fw_version, ver_str, sizeof(ver_str));
    n = snprintf(buf, sizeof(buf), "Firmware : %s\r\n", ver_str);
    if (n > 0) io->write(io->ctx, buf, (uint16_t)n);

    /* Config entries */
    n = snprintf(buf, sizeof(buf), "Config entries: %u\r\n",
                 (unsigned)config_store_count(&app->config));
    if (n > 0) io->write(io->ctx, buf, (uint16_t)n);

    /* Parameters */
    n = snprintf(buf, sizeof(buf), "Parameters    : %u\r\n",
                 (unsigned)param_table_count(&app->params));
    if (n > 0) io->write(io->ctx, buf, (uint16_t)n);

    /* Errors */
    n = snprintf(buf, sizeof(buf),
                 "Error records : %u (total: %lu, fatal: %lu)\r\n",
                 (unsigned)error_handler_get_count(&app->errors),
                 (unsigned long)error_handler_total_errors(&app->errors),
                 (unsigned long)error_handler_total_fatals(&app->errors));
    if (n > 0) io->write(io->ctx, buf, (uint16_t)n);

    /* Show a couple key parameters */
    char val[PARAM_VALUE_SIZE];
    if (param_table_get(&app->params, "device_name", val, sizeof(val))
            == PARAM_OK) {
        n = snprintf(buf, sizeof(buf), "Device name   : %s\r\n", val);
        if (n > 0) io->write(io->ctx, buf, (uint16_t)n);
    }
}

/* --- "errors" — list error log entries --- */

static const char *severity_str(error_severity_t sev)
{
    switch (sev) {
    case ERROR_SEVERITY_INFO:    return "INFO";
    case ERROR_SEVERITY_WARNING: return "WARN";
    case ERROR_SEVERITY_ERROR:   return "ERROR";
    case ERROR_SEVERITY_FATAL:   return "FATAL";
    default:                     return "?";
    }
}

static void cmd_errors(void *ctx, int argc, const char **argv,
                       const cli_shell_io_t *io)
{
    app_ctx_t *app = (app_ctx_t *)ctx;
    char buf[128];
    int n;

    /* "errors clear" subcommand */
    if (argc >= 2 && strcmp(argv[1], "clear") == 0) {
        error_handler_clear(&app->errors);
        io->write(io->ctx, "Error log cleared.\r\n", 20);
        return;
    }

    uint8_t count = error_handler_get_count(&app->errors);
    if (count == 0) {
        io->write(io->ctx, "No errors recorded.\r\n", 21);
        return;
    }

    n = snprintf(buf, sizeof(buf),
                 "--- Error Log (%u records) ---\r\n", (unsigned)count);
    if (n > 0) io->write(io->ctx, buf, (uint16_t)n);

    for (uint8_t i = 0; i < count; i++) {
        error_record_t rec;
        if (error_handler_get(&app->errors, i, &rec) == ERROR_HANDLER_OK) {
            n = snprintf(buf, sizeof(buf),
                         "  [%lu] code=0x%04X sev=%-5s ctx=0x%08lX\r\n",
                         (unsigned long)rec.timestamp,
                         (unsigned)rec.code,
                         severity_str(rec.severity),
                         (unsigned long)rec.context);
            if (n > 0) io->write(io->ctx, buf, (uint16_t)n);
        }
    }
}

/* --- "version" — show firmware version --- */

static void cmd_version(void *ctx, int argc, const char **argv,
                        const cli_shell_io_t *io)
{
    (void)argc;
    (void)argv;
    app_ctx_t *app = (app_ctx_t *)ctx;
    char buf[64];

    char ver_str[16];
    version_to_string(&app->fw_version, ver_str, sizeof(ver_str));
    int n = snprintf(buf, sizeof(buf), "Firmware v%s\r\n", ver_str);
    if (n > 0) io->write(io->ctx, buf, (uint16_t)n);
}

/* ======================================================================
 * Initialization — wire up all wheels
 * ====================================================================== */

static void app_init(app_ctx_t *app)
{
    /* --- Parse firmware version --- */
    version_parse(FW_VERSION_STR, &app->fw_version);

    /* --- Config store with mock flash backend --- */
    static const config_store_storage_t flash_storage = {
        .save = mock_flash_save,
        .load = mock_flash_load,
        .ctx  = NULL,
    };
    config_store_init(&app->config, &flash_storage);

    /* Define typed config entries (used for persistence / validation) */
    config_store_define_uint(&app->config, "sensor_interval",
                             1000, 100, 60000);
    config_store_define_uint(&app->config, "log_level",
                             2, 0, 4);
    config_store_define_string(&app->config, "device_name",
                               "DevShell-01");
    config_store_define_bool(&app->config, "auto_report", true);

    /* --- Param table (string view for CLI get/set/list) --- */
    param_table_init(&app->params);
    param_table_define(&app->params, "sensor_interval", "1000", false);
    param_table_define(&app->params, "log_level",       "2",    false);
    param_table_define(&app->params, "device_name",     "DevShell-01", false);
    param_table_define(&app->params, "auto_report",     "true", false);
    param_table_define(&app->params, "hw_revision",     "B2",   true);

    /* --- Error handler (no persistence for this demo) --- */
    error_handler_init(&app->errors, NULL);

    /* --- CLI shell --- */
    static const cli_shell_io_t shell_io = {
        .write = shell_write,
        .ctx   = NULL,
    };
    cli_shell_init(&app->shell, &shell_io, "device> ");

    /* Register commands */
    cli_shell_register(&app->shell, "config",  "get|set|list parameters",
                       cmd_config, app);
    cli_shell_register(&app->shell, "status",  "show device summary",
                       cmd_status, app);
    cli_shell_register(&app->shell, "errors",  "show/clear error log",
                       cmd_errors, app);
    cli_shell_register(&app->shell, "version", "show firmware version",
                       cmd_version, app);
}

/* ======================================================================
 * Command feeder — simulate user typing commands
 * ====================================================================== */

static void feed_line(cli_shell_t *sh, const char *line)
{
    printf("\n");  /* visual separator between commands */
    while (*line) {
        cli_shell_feed(sh, (uint8_t)*line);
        line++;
    }
    cli_shell_feed(sh, (uint8_t)'\r');  /* press Enter */
}

/* ======================================================================
 * main — run the demo scenario
 * ====================================================================== */

int main(void)
{
    printf("=== Device Configuration Shell — Integration Recipe ===\n");
    printf("Wheels: cli_shell + config_store + param_table "
           "+ version_info + error_handler\n");
    printf("========================================================\n");

    app_init(&g_app);

    /* ---- Scene 1: Check version ---- */
    printf("\n--- Scene 1: Check firmware version ---\n");
    feed_line(&g_app.shell, "version");

    /* ---- Scene 2: List all parameters ---- */
    printf("\n--- Scene 2: List all parameters ---\n");
    feed_line(&g_app.shell, "config list");

    /* ---- Scene 3: Get a specific parameter ---- */
    printf("\n--- Scene 3: Read a parameter ---\n");
    feed_line(&g_app.shell, "config get sensor_interval");

    /* ---- Scene 4: Set a parameter and verify persistence ---- */
    printf("\n--- Scene 4: Set parameter + verify ---\n");
    feed_line(&g_app.shell, "config set sensor_interval 500");
    feed_line(&g_app.shell, "config get sensor_interval");

    /* ---- Scene 5: Try to set a read-only parameter ---- */
    printf("\n--- Scene 5: Read-only parameter protection ---\n");
    feed_line(&g_app.shell, "config set hw_revision C1");

    /* ---- Scene 6: Trigger some errors, then inspect ---- */
    printf("\n--- Scene 6: Error reporting ---\n");
    error_handler_report(&g_app.errors, 0x0010, ERROR_SEVERITY_WARNING,
                         1000, 0x00);
    error_handler_report(&g_app.errors, 0x0020, ERROR_SEVERITY_ERROR,
                         2500, 0xDEAD);
    error_handler_report(&g_app.errors, 0x0030, ERROR_SEVERITY_INFO,
                         3200, 0x42);
    printf("(Injected 3 error records)\n");
    feed_line(&g_app.shell, "errors");

    /* ---- Scene 7: Device status overview ---- */
    printf("\n--- Scene 7: Device status ---\n");
    feed_line(&g_app.shell, "status");

    /* ---- Scene 8: Save config to mock flash, change, reload ---- */
    printf("\n--- Scene 8: Persistence (save → change → load) ---\n");
    config_store_save_all(&g_app.config);
    printf("(Saved config to mock flash)\n");

    /* Change param in-memory */
    param_table_set(&g_app.params, "device_name", "CHANGED");
    config_store_set_string(&g_app.config, "device_name", "CHANGED");
    feed_line(&g_app.shell, "config get device_name");

    /* Reload from flash — should restore original */
    config_store_load_all(&g_app.config);
    /* Sync param_table from config_store */
    char restored[CONFIG_STORE_STRING_SIZE];
    config_store_get_string(&g_app.config, "device_name",
                            restored, sizeof(restored));
    param_table_set(&g_app.params, "device_name", restored);
    printf("(Reloaded config from mock flash)\n");
    feed_line(&g_app.shell, "config get device_name");

    /* ---- Scene 9: Help command ---- */
    printf("\n--- Scene 9: Built-in help ---\n");
    feed_line(&g_app.shell, "help");

    /* ---- Scene 10: Unknown command ---- */
    printf("\n--- Scene 10: Unknown command ---\n");
    feed_line(&g_app.shell, "reboot");

    /* ---- Scene 11: Clear errors ---- */
    printf("\n--- Scene 11: Clear error log ---\n");
    feed_line(&g_app.shell, "errors clear");
    feed_line(&g_app.shell, "errors");

    printf("\n=== Demo complete ===\n");
    return 0;
}
