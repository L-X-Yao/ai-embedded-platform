// SPDX-License-Identifier: MIT
/*
 * prop_cli_shell.c — property-based tests for CLI shell module
 *
 * Properties:
 *   1. feeding arbitrary bytes never crashes
 *   2. feeding a registered command name + newline invokes handler
 *   3. arbitrary length input (up to line buffer) never crashes
 *   4. registering max commands then feeding never crashes
 */

#include "test_harness.h"
#include "property_test.h"
#include "cli_shell.h"
#include <string.h>

/* ---------- mock I/O ---------- */

static char  out_buf[1024];
static int   out_pos;
static int   handler_called;
static int   handler_argc;

static void mock_write(void *ctx, const char *data, uint16_t len)
{
    (void)ctx;
    for (uint16_t i = 0; i < len && out_pos < (int)sizeof(out_buf) - 1; i++)
        out_buf[out_pos++] = data[i];
    out_buf[out_pos] = '\0';
}

static const cli_shell_io_t mock_io = {
    .write = mock_write,
    .ctx   = NULL,
};

static void mock_handler(void *ctx, int argc, const char **argv,
                         const cli_shell_io_t *io)
{
    (void)ctx; (void)argv; (void)io;
    handler_called++;
    handler_argc = argc;
}

static void mock_reset(void)
{
    out_pos = 0;
    out_buf[0] = '\0';
    handler_called = 0;
    handler_argc = 0;
}

/* ------------------------------------------------------------------ */

PROPERTY(arbitrary_bytes_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    cli_shell_t sh;
    cli_shell_init(&sh, &mock_io, "> ");

    /* Feed random bytes */
    int n = (int)(ARBI_UINT8(seed) % 200) + 1;
    for (int i = 0; i < n; i++) {
        uint8_t byte = ARBI_UINT8(seed);
        cli_shell_feed(&sh, byte);
    }
    /* Must not crash */
}

PROPERTY(registered_cmd_invoked, 5000)
{
    (void)seed;
    mock_reset();
    cli_shell_t sh;
    cli_shell_init(&sh, &mock_io, "> ");
    cli_shell_register(&sh, "test", "test cmd", mock_handler, NULL);

    /* Feed "test" + newline */
    const char *cmd = "test\n";
    for (int i = 0; cmd[i]; i++)
        cli_shell_feed(&sh, (uint8_t)cmd[i]);

    PROP_ASSERT(handler_called == 1);
    PROP_ASSERT(handler_argc >= 1);
}

PROPERTY(long_input_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    cli_shell_t sh;
    cli_shell_init(&sh, &mock_io, "> ");
    cli_shell_register(&sh, "x", "x cmd", mock_handler, NULL);

    /* Feed a long line (may exceed CLI_SHELL_LINE_SIZE) */
    int len = (int)(ARBI_UINT8(seed) % 255) + CLI_SHELL_LINE_SIZE;
    for (int i = 0; i < len; i++) {
        /* Printable ASCII, avoid newline until the end */
        uint8_t byte = (uint8_t)(0x20 + ARBI_UINT8(seed) % 95);
        cli_shell_feed(&sh, byte);
    }
    /* Terminate with newline */
    cli_shell_feed(&sh, '\n');
    /* Must not crash */
}

PROPERTY(max_commands_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    cli_shell_t sh;
    cli_shell_init(&sh, &mock_io, "> ");

    /* Register up to CLI_SHELL_MAX_CMDS + some extras (should return ERR_FULL) */
    char names[CLI_SHELL_MAX_CMDS + 4][8];
    for (int i = 0; i < CLI_SHELL_MAX_CMDS + 4; i++) {
        int name_len = 1 + (int)(ARBI_UINT8(seed) % 6);
        for (int j = 0; j < name_len; j++)
            names[i][j] = (char)('a' + ARBI_UINT8(seed) % 26);
        names[i][name_len] = '\0';
        cli_shell_register(&sh, names[i], "help", mock_handler, NULL);
    }

    /* Feed a random command and newline */
    int idx = (int)(ARBI_UINT8(seed) % (CLI_SHELL_MAX_CMDS + 4));
    for (int i = 0; names[idx][i]; i++)
        cli_shell_feed(&sh, (uint8_t)names[idx][i]);
    cli_shell_feed(&sh, '\n');
    /* Must not crash */
}

TEST_MAIN("prop_cli_shell")
    RUN(arbitrary_bytes_no_crash);
    RUN(registered_cmd_invoked);
    RUN(long_input_no_crash);
    RUN(max_commands_no_crash);
TEST_MAIN_END()
