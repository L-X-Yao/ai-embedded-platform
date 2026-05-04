/*
 * prop_cli_parser.c — property-based tests for cli_parser
 *
 * Requires: property_test.h from tooling/test/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/test prop_cli_parser.c ../cli_parser.c -o prop_cli_parser
 */

#include "test_harness.h"
#include "property_test.h"
#include "../cli_parser.h"
#include <string.h>

/* --- test infrastructure ------------------------------------------------ */

static char g_output[4096];
static size_t g_output_len;

static void mock_write(const char *buf, size_t len, void *ctx)
{
    (void)ctx;
    if (g_output_len + len < sizeof(g_output)) {
        memcpy(g_output + g_output_len, buf, len);
        g_output_len += len;
    }
}

static int g_handler_called;
static int g_handler_argc;

static cli_err_t mock_handler(int argc, const char *argv[], void *ctx)
{
    (void)argv; (void)ctx;
    g_handler_called = 1;
    g_handler_argc = argc;
    return CLI_OK;
}

static const cli_cmd_t test_cmds[] = {
    { "test", mock_handler, "test command" },
    { "ping", mock_handler, "ping command" },
};

static void reset_state(void)
{
    g_output_len = 0;
    memset(g_output, 0, sizeof(g_output));
    g_handler_called = 0;
    g_handler_argc = 0;
}

/* --- properties --------------------------------------------------------- */

/* Property: feeding arbitrary bytes never crashes */
PROPERTY(no_crash_random_feed, 50000) {
    reset_state();
    cli_parser_t parser;
    cli_cfg_t cfg = {
        .commands     = test_cmds,
        .num_commands = 2,
        .write_fn     = mock_write,
        .user_ctx     = NULL,
        .prompt       = "> ",
    };
    cli_parser_init(&parser, &cfg);

    unsigned n = (unsigned)ARBI_RANGE(seed, 1, 200);
    for (unsigned i = 0; i < n; i++) {
        uint8_t byte = ARBI_UINT8(seed);
        cli_parser_feed(&parser, byte);
    }
    PROP_ASSERT(1);  /* survived without crash */
}

/* Property: determinism — same byte sequence produces same output */
PROPERTY(deterministic_output, 5000) {
    uint8_t bytes[100];
    unsigned n = (unsigned)ARBI_RANGE(seed, 1, 100);
    uint32_t fill_seed = seed;
    ARBI_FILL(fill_seed, bytes, n);

    char out1[4096], out2[4096];
    size_t len1, len2;

    for (int pass = 0; pass < 2; pass++) {
        reset_state();
        cli_parser_t parser;
        cli_cfg_t cfg = {
            .commands     = test_cmds,
            .num_commands = 2,
            .write_fn     = mock_write,
            .user_ctx     = NULL,
            .prompt       = "> ",
        };
        cli_parser_init(&parser, &cfg);
        g_output_len = 0;  /* ignore init prompt */

        for (unsigned i = 0; i < n; i++) {
            cli_parser_feed(&parser, bytes[i]);
        }

        if (pass == 0) {
            memcpy(out1, g_output, g_output_len);
            len1 = g_output_len;
        } else {
            memcpy(out2, g_output, g_output_len);
            len2 = g_output_len;
        }
    }

    PROP_ASSERT_EQ(len1, len2);
    if (len1 > 0) {
        PROP_ASSERT_MEM_EQ(out1, out2, len1);
    }
}

/* Property: cli_parser_run returns valid error codes for random strings */
PROPERTY(run_valid_error_codes, 50000) {
    reset_state();
    cli_parser_t parser;
    cli_cfg_t cfg = {
        .commands     = test_cmds,
        .num_commands = 2,
        .write_fn     = mock_write,
        .user_ctx     = NULL,
        .prompt       = NULL,
    };
    cli_parser_init(&parser, &cfg);

    char line[CLI_LINE_BUFSIZE + 10];
    unsigned len = (unsigned)ARBI_RANGE(seed, 0, CLI_LINE_BUFSIZE + 9);
    ARBI_FILL(seed, (uint8_t *)line, len);
    /* Replace NUL bytes so strlen works predictably */
    for (unsigned i = 0; i < len; i++) {
        if (line[i] == '\0') line[i] = 'x';
    }
    line[len] = '\0';

    cli_err_t err = cli_parser_run(&parser, line);
    PROP_ASSERT(err == CLI_OK || err == CLI_ERR_NOT_FOUND ||
                err == CLI_ERR_ARGS || err == CLI_ERR_OVERFLOW);
}

/* Property: line buffer never exceeds CLI_LINE_BUFSIZE - 1 */
PROPERTY(line_length_bounded, 50000) {
    reset_state();
    cli_parser_t parser;
    cli_cfg_t cfg = {
        .commands     = test_cmds,
        .num_commands = 2,
        .write_fn     = mock_write,
        .user_ctx     = NULL,
        .prompt       = NULL,
    };
    cli_parser_init(&parser, &cfg);

    unsigned n = (unsigned)ARBI_RANGE(seed, 1, 300);
    for (unsigned i = 0; i < n; i++) {
        /* Feed only printable non-newline chars to fill buffer */
        uint8_t byte = (uint8_t)(0x20 + (ARBI_UINT8(seed) % 0x5F));
        cli_parser_feed(&parser, byte);
        PROP_ASSERT(parser.line_len < CLI_LINE_BUFSIZE);
    }
}

TEST_MAIN("cli_parser properties") {
    RUN(no_crash_random_feed);
    RUN(deterministic_output);
    RUN(run_valid_error_codes);
    RUN(line_length_bounded);
}
TEST_MAIN_END()
