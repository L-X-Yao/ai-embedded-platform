// SPDX-License-Identifier: MIT
/*
 * command_queue_demo -- integration recipe: ASCII command dispatcher
 *
 * Demonstrates the pattern for receiving commands over UART, parsing
 * them as ASCII text, queueing by priority, and dispatching from the
 * main loop.  Combines four wheels:
 *
 *   L0  ring_buffer   -- byte-level RX buffer between UART ISR and parser
 *   L0  cli_parser    -- tokenize and dispatch the parsed line
 *   L0  string_utils  -- numeric conversion for arg parsing
 *   L3  command_queue -- prioritized work queue consumed in main loop
 *
 * Flow:
 *   simulated UART bytes -> ring_buffer -> line extractor -> cli_parser
 *     -> command_queue_push (priority assigned by command)
 *     -> command_queue_pop in dispatch loop -> action handlers
 *
 * Runs entirely on host. No real UART, no threads, no heap.
 * Build:  cmake -S . -B build && cmake --build build && ./build/command_queue_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ring_buffer.h"
#include "cli_parser.h"
#include "string_utils.h"
#include "command_queue.h"

/* ======================================================================
 * Command catalog
 *
 * Commands carry an ID + priority + small data payload.  The dispatch
 * loop pops in priority order (lower numeric value runs first) and
 * dispatches to a handler.
 * ====================================================================== */

typedef enum {
    CMD_NONE      = 0,
    CMD_REBOOT    = 1,
    CMD_LED_SET   = 2,
    CMD_LOG_DUMP  = 3,
    CMD_PING      = 4,
} cmd_id_t;

/* Priority convention: critical=0, normal=5, background=9.
 * "reboot" runs before everything else; "ping" runs last. */

/* ======================================================================
 * Application context
 * ====================================================================== */

#define RB_CAPACITY     256
#define LINE_BUFSIZE    64
#define QUEUE_CAPACITY  16

typedef struct {
    /* UART RX buffer */
    ring_buffer_t rx_rb;
    uint8_t       rx_buf[RB_CAPACITY];

    /* Pending command queue */
    command_queue_t      queue;
    command_queue_item_t queue_buf[QUEUE_CAPACITY];

    /* Stats */
    uint32_t lines_parsed;
    uint32_t cmds_dispatched;
    uint32_t cmds_unknown;
    uint32_t led_state;
    bool     reboot_requested;
} app_ctx_t;

static app_ctx_t g_app;

/* ======================================================================
 * cli_parser handlers -- push the parsed command into command_queue
 *
 * The parser only handles tokenization and routing; the actual work
 * (LED toggle, log dump, etc.) is performed later by the dispatch loop.
 * This separation is what command_queue is for: parse-fast, execute-when-ready.
 * ====================================================================== */

static cli_err_t handle_reboot(int argc, const char *argv[], void *user_ctx)
{
    (void)argc; (void)argv;
    app_ctx_t *app = (app_ctx_t *)user_ctx;
    printf("    [parser] queued: reboot (priority=0)\n");
    command_queue_push(&app->queue, CMD_REBOOT, 0, NULL, 0);
    return CLI_OK;
}

static cli_err_t handle_led(int argc, const char *argv[], void *user_ctx)
{
    app_ctx_t *app = (app_ctx_t *)user_ctx;
    if (argc < 2) {
        printf("    [parser] usage: led <0|1>\n");
        return CLI_ERR_ARGS;
    }
    /* string_utils handles the numeric conversion */
    int32_t v = 0;
    if (str_to_int(argv[1], &v) != STRING_UTILS_OK) {
        printf("    [parser] led: bad numeric arg '%s'\n", argv[1]);
        return CLI_ERR_ARGS;
    }
    uint8_t payload = (v != 0) ? 1u : 0u;
    printf("    [parser] queued: led %u (priority=5)\n", (unsigned)payload);
    command_queue_push(&app->queue, CMD_LED_SET, 5, &payload, 1);
    return CLI_OK;
}

static cli_err_t handle_log(int argc, const char *argv[], void *user_ctx)
{
    (void)argc; (void)argv;
    app_ctx_t *app = (app_ctx_t *)user_ctx;
    printf("    [parser] queued: log (priority=7)\n");
    command_queue_push(&app->queue, CMD_LOG_DUMP, 7, NULL, 0);
    return CLI_OK;
}

static cli_err_t handle_ping(int argc, const char *argv[], void *user_ctx)
{
    (void)argc; (void)argv;
    app_ctx_t *app = (app_ctx_t *)user_ctx;
    printf("    [parser] queued: ping (priority=9)\n");
    command_queue_push(&app->queue, CMD_PING, 9, NULL, 0);
    return CLI_OK;
}

static const cli_cmd_t CLI_COMMANDS[] = {
    { "reboot", handle_reboot, "schedule a reboot" },
    { "led",    handle_led,    "led <0|1>" },
    { "log",    handle_log,    "dump pending log" },
    { "ping",   handle_ping,   "echo" },
};

/* cli_parser writes prompt/echo bytes via this callback.  We swallow
 * them for the demo so output stays readable. */
static void cli_write_sink(const char *buf, size_t len, void *user_ctx)
{
    (void)buf; (void)len; (void)user_ctx;
    /* discard interactive echo; only handler-side printf is shown */
}

/* ======================================================================
 * Action handlers -- invoked by the dispatch loop when an item pops
 * ====================================================================== */

static const char *cmd_id_name(cmd_id_t id)
{
    switch (id) {
    case CMD_REBOOT:   return "reboot";
    case CMD_LED_SET:  return "led";
    case CMD_LOG_DUMP: return "log";
    case CMD_PING:     return "ping";
    case CMD_NONE:
    default:           return "?";
    }
}

static void action_reboot(app_ctx_t *app, const command_queue_item_t *item)
{
    (void)item;
    printf("    [action] REBOOT scheduled (highest priority)\n");
    app->reboot_requested = true;
}

static void action_led(app_ctx_t *app, const command_queue_item_t *item)
{
    if (item->len < 1) {
        printf("    [action] LED: malformed payload\n");
        return;
    }
    app->led_state = item->data[0];
    printf("    [action] LED -> %s\n", app->led_state ? "ON" : "OFF");
}

static void action_log(app_ctx_t *app, const command_queue_item_t *item)
{
    (void)item;
    printf("    [action] LOG DUMP: parsed=%lu dispatched=%lu unknown=%lu led=%u\n",
           (unsigned long)app->lines_parsed,
           (unsigned long)app->cmds_dispatched,
           (unsigned long)app->cmds_unknown,
           (unsigned)app->led_state);
}

static void action_ping(app_ctx_t *app, const command_queue_item_t *item)
{
    (void)app; (void)item;
    printf("    [action] PING: pong\n");
}

/* ======================================================================
 * UART simulation -- feed an entire ASCII script into ring_buffer
 *
 * In a real device, the UART RX ISR would call ring_buffer_write() one
 * byte at a time.  Here we cram a whole script in at once.
 * ====================================================================== */

static void uart_feed(app_ctx_t *app, const char *bytes)
{
    uint16_t n = (uint16_t)strlen(bytes);
    uint16_t wrote = ring_buffer_write(&app->rx_rb, (const uint8_t *)bytes, n);
    if (wrote != n) {
        printf("    [warn] uart_feed dropped %u bytes (ring_buffer full)\n",
               (unsigned)(n - wrote));
    }
}

/* Feed exactly one byte (simulates a UART RX ISR firing for each char). */
static void uart_feed_byte(app_ctx_t *app, uint8_t byte)
{
    (void)ring_buffer_write(&app->rx_rb, &byte, 1);
}

/* ======================================================================
 * Line extractor -- pull bytes from ring_buffer until '\n', hand line
 * off to cli_parser_run().  Returns the number of complete lines processed.
 * ====================================================================== */

static uint32_t extract_and_parse_lines(app_ctx_t *app, cli_parser_t *parser)
{
    uint32_t parsed = 0;
    char line[LINE_BUFSIZE];
    size_t len = 0;

    while (!ring_buffer_is_empty(&app->rx_rb)) {
        uint8_t byte;
        if (ring_buffer_read(&app->rx_rb, &byte, 1) != 1) break;

        if (byte == '\r') continue;
        if (byte == '\n') {
            if (len == 0) continue;        /* skip blank lines */
            line[len] = '\0';
            printf("  [line] %s\n", line);
            cli_err_t rc = cli_parser_run(parser, line);
            if (rc == CLI_ERR_NOT_FOUND) {
                app->cmds_unknown++;
            }
            parsed++;
            len = 0;
            continue;
        }
        if (len < sizeof(line) - 1) {
            line[len++] = (char)byte;
        }
    }
    app->lines_parsed += parsed;
    return parsed;
}

/* ======================================================================
 * Dispatch loop -- drain command_queue in priority order
 * ====================================================================== */

static void dispatch_pending(app_ctx_t *app)
{
    while (!command_queue_is_empty(&app->queue)) {
        command_queue_item_t item;
        if (command_queue_pop(&app->queue, &item) != COMMAND_QUEUE_OK) break;

        printf("  [dispatch] popped id=%u (%s) prio=%u len=%u\n",
               (unsigned)item.cmd_id, cmd_id_name((cmd_id_t)item.cmd_id),
               (unsigned)item.priority, (unsigned)item.len);

        switch ((cmd_id_t)item.cmd_id) {
        case CMD_REBOOT:   action_reboot(app, &item); break;
        case CMD_LED_SET:  action_led(app, &item);    break;
        case CMD_LOG_DUMP: action_log(app, &item);    break;
        case CMD_PING:     action_ping(app, &item);   break;
        case CMD_NONE:
        default: printf("    [action] unknown id\n"); break;
        }
        app->cmds_dispatched++;
    }
}

/* ======================================================================
 * Initialization
 * ====================================================================== */

static void app_init(app_ctx_t *app)
{
    memset(app, 0, sizeof(*app));
    ring_buffer_init(&app->rx_rb, app->rx_buf, RB_CAPACITY);
    command_queue_init(&app->queue, app->queue_buf, QUEUE_CAPACITY);
}

/* ======================================================================
 * main -- run a few scripted UART scenes
 * ====================================================================== */

int main(void)
{
    printf("=== command_queue_demo -- ASCII command dispatcher ===\n");
    printf("Wheels: ring_buffer + cli_parser + string_utils + command_queue\n");
    printf("========================================================\n");

    app_init(&g_app);

    /* Build the CLI parser. */
    cli_parser_t parser;
    cli_cfg_t parser_cfg = {
        .commands     = CLI_COMMANDS,
        .num_commands = sizeof(CLI_COMMANDS) / sizeof(CLI_COMMANDS[0]),
        .write_fn     = cli_write_sink,
        .user_ctx     = &g_app,
        .prompt       = NULL,
    };
    cli_parser_init(&parser, &parser_cfg);

    /* --- Scene 1: queue arrives out of priority order, dispatch in order. */
    printf("\n--- Scene 1: priority reorders execution ---\n");
    uart_feed(&g_app,
              "ping\n"          /* prio 9 */
              "led 1\n"         /* prio 5 */
              "reboot\n"        /* prio 0 -- should fire first */
              "log\n"           /* prio 7 */
              );
    printf("  --> parse phase\n");
    extract_and_parse_lines(&g_app, &parser);
    printf("  --> dispatch phase (high prio first)\n");
    dispatch_pending(&g_app);

    /* --- Scene 2: parser errors + unknown commands do not crash. */
    printf("\n--- Scene 2: parser error tolerance ---\n");
    uart_feed(&g_app,
              "led not_a_number\n"   /* bad arg, rejected by handler */
              "fly\n"                /* unknown command */
              "led 0\n"              /* good */
              );
    extract_and_parse_lines(&g_app, &parser);
    dispatch_pending(&g_app);

    /* --- Scene 3: byte-by-byte arrival, line is split across reads. */
    printf("\n--- Scene 3: byte-by-byte UART arrival ---\n");
    const char *script = "led 1\nping\nlog\n";
    for (size_t i = 0; script[i] != '\0'; i++) {
        uart_feed_byte(&g_app, (uint8_t)script[i]);
        /* Drain only after seeing a newline -- simulates a polling
         * loop that does a "read all available, then dispatch" pass. */
        if (script[i] == '\n') {
            extract_and_parse_lines(&g_app, &parser);
        }
    }
    dispatch_pending(&g_app);

    /* --- Scene 4: queue full handling. */
    printf("\n--- Scene 4: queue-full handling ---\n");
    /* Push QUEUE_CAPACITY + extras pings without dispatching. */
    int over = 0;
    for (int i = 0; i < QUEUE_CAPACITY + 4; i++) {
        command_queue_err_t rc = command_queue_push(&g_app.queue,
                                                    CMD_PING, 9, NULL, 0);
        if (rc == COMMAND_QUEUE_ERR_FULL) over++;
    }
    printf("  Pushed %d items, %d rejected with FULL (capacity=%d)\n",
           QUEUE_CAPACITY, over, QUEUE_CAPACITY);
    printf("  queue.count = %u\n", (unsigned)command_queue_count(&g_app.queue));
    /* Drain so subsequent scenes start clean. */
    command_queue_clear(&g_app.queue);

    /* --- Scene 5: final stats. */
    printf("\n--- Scene 5: final stats ---\n");
    printf("  lines_parsed    = %lu\n", (unsigned long)g_app.lines_parsed);
    printf("  cmds_dispatched = %lu\n", (unsigned long)g_app.cmds_dispatched);
    printf("  cmds_unknown    = %lu\n", (unsigned long)g_app.cmds_unknown);
    printf("  reboot_pending  = %s\n",  g_app.reboot_requested ? "YES" : "no");

    /* Self-check:
     *   scene 1 -> 4 dispatched (reboot/led/log/ping)
     *   scene 2 -> 1 dispatched (led 0) + 1 unknown ("fly")
     *   scene 3 -> 3 dispatched (led/ping/log)
     * Expected total: dispatched=8, unknown>=1, reboot_requested=true */
    int rc = 0;
    if (g_app.cmds_dispatched != 8) {
        printf("\n[fail] expected 8 dispatched, got %lu\n",
               (unsigned long)g_app.cmds_dispatched);
        rc = 1;
    }
    if (g_app.cmds_unknown < 1) {
        printf("\n[fail] expected >=1 unknown, got %lu\n",
               (unsigned long)g_app.cmds_unknown);
        rc = 1;
    }
    if (!g_app.reboot_requested) {
        printf("\n[fail] reboot was not requested\n");
        rc = 1;
    }
    if (rc != 0) {
        printf("\n[fail] command_queue_demo did not meet expectations\n");
        return rc;
    }

    printf("\n=== command_queue_demo complete ===\n");
    return 0;
}
