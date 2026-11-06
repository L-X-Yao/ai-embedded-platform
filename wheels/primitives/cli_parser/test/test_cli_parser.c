// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "cli_parser.h"

/* ---- Output capture ---- */

static char out_buf[2048];
static size_t out_len;

static void capture_write(const char *buf, size_t len, void *ctx)
{
    (void)ctx;
    if (out_len + len < sizeof(out_buf)) {
        memcpy(out_buf + out_len, buf, len);
        out_len += len;
    }
    out_buf[out_len] = '\0';
}

static void capture_reset(void)
{
    out_len = 0;
    out_buf[0] = '\0';
}

/* ---- Test commands ---- */

static int last_argc;
static char last_argv0[64];
static char last_argv1[64];

static cli_err_t cmd_ping(int argc, const char *argv[], void *ctx)
{
    (void)ctx;
    last_argc = argc;
    if (argc > 0) strncpy(last_argv0, argv[0], sizeof(last_argv0) - 1);
    if (argc > 1) strncpy(last_argv1, argv[1], sizeof(last_argv1) - 1);
    return CLI_OK;
}

static cli_err_t cmd_fail(int argc, const char *argv[], void *ctx)
{
    (void)argc; (void)argv; (void)ctx;
    return CLI_ERR_ARGS;
}

static const cli_cmd_t commands[] = {
    { "ping", cmd_ping, "test command" },
    { "fail", cmd_fail, "always fails" },
};

static cli_parser_t parser;

static void setup(const char *prompt)
{
    capture_reset();
    last_argc = 0;
    last_argv0[0] = '\0';
    last_argv1[0] = '\0';
    cli_cfg_t cfg = {
        .commands = commands,
        .num_commands = sizeof(commands) / sizeof(commands[0]),
        .write_fn = capture_write,
        .user_ctx = NULL,
        .prompt = prompt,
    };
    cli_parser_init(&parser, &cfg);
}

/* ---- Tests ---- */

TEST(test_init)
{
    setup("> ");
    ASSERT_TRUE(parser.line_len == 0);
    ASSERT_TRUE(strstr(out_buf, "> ") != NULL);
}

TEST(test_run_known)
{
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping");
    ASSERT_TRUE(err == CLI_OK);
    ASSERT_TRUE(last_argc == 1);
    ASSERT_TRUE(strcmp(last_argv0, "ping") == 0);
}

TEST(test_run_unknown)
{
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "nosuchcmd");
    ASSERT_TRUE(err == CLI_ERR_NOT_FOUND);
    ASSERT_TRUE(strstr(out_buf, "unknown command") != NULL);
}

TEST(test_run_empty)
{
    setup(NULL);
    cli_err_t err = cli_parser_run(&parser, "");
    ASSERT_TRUE(err == CLI_OK);
    err = cli_parser_run(&parser, "   ");
    ASSERT_TRUE(err == CLI_OK);
}

TEST(test_run_args)
{
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping world");
    ASSERT_TRUE(err == CLI_OK);
    ASSERT_TRUE(last_argc == 2);
    ASSERT_TRUE(strcmp(last_argv0, "ping") == 0);
    ASSERT_TRUE(strcmp(last_argv1, "world") == 0);
}

TEST(test_run_extra_whitespace)
{
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "  ping   hello  ");
    ASSERT_TRUE(err == CLI_OK);
    ASSERT_TRUE(last_argc == 2);
    ASSERT_TRUE(strcmp(last_argv1, "hello") == 0);
}

TEST(test_help)
{
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "help");
    ASSERT_TRUE(err == CLI_OK);
    ASSERT_TRUE(strstr(out_buf, "ping") != NULL);
    ASSERT_TRUE(strstr(out_buf, "test command") != NULL);
    ASSERT_TRUE(strstr(out_buf, "help") != NULL);
}

TEST(test_handler_error)
{
    setup(NULL);
    cli_err_t err = cli_parser_run(&parser, "fail");
    ASSERT_TRUE(err == CLI_ERR_ARGS);
}

TEST(test_overflow)
{
    setup(NULL);
    char long_line[CLI_LINE_BUFSIZE + 10];
    memset(long_line, 'a', sizeof(long_line) - 1);
    long_line[sizeof(long_line) - 1] = '\0';
    cli_err_t err = cli_parser_run(&parser, long_line);
    ASSERT_TRUE(err == CLI_ERR_OVERFLOW);
}

TEST(test_null_safety)
{
    cli_parser_init(NULL, NULL);
    cli_parser_feed(NULL, 'x');
    cli_err_t err = cli_parser_run(NULL, "ping");
    ASSERT_TRUE(err == CLI_ERR_ARGS);
    setup(NULL);
    err = cli_parser_run(&parser, NULL);
    ASSERT_TRUE(err == CLI_ERR_ARGS);
}

TEST(test_feed_and_enter)
{
    setup("> ");
    capture_reset();
    const char *input = "ping";
    for (size_t i = 0; i < strlen(input); i++) {
        cli_parser_feed(&parser, (uint8_t)input[i]);
    }
    cli_parser_feed(&parser, '\r');
    ASSERT_TRUE(last_argc == 1);
    ASSERT_TRUE(strcmp(last_argv0, "ping") == 0);
    ASSERT_TRUE(strstr(out_buf, "> ") != NULL);
}

TEST(test_feed_backspace)
{
    setup(NULL);
    capture_reset();
    cli_parser_feed(&parser, 'x');
    cli_parser_feed(&parser, '\b');
    cli_parser_feed(&parser, '\r');
    ASSERT_TRUE(last_argc == 0);
}

TEST(test_feed_overflow)
{
    setup(NULL);
    capture_reset();
    for (int i = 0; i < CLI_LINE_BUFSIZE + 10; i++) {
        cli_parser_feed(&parser, 'a');
    }
    ASSERT_TRUE(parser.line_len == CLI_LINE_BUFSIZE - 1);
}

TEST(test_feed_del)
{
    setup(NULL);
    capture_reset();
    cli_parser_feed(&parser, 'a');
    cli_parser_feed(&parser, 'b');
    cli_parser_feed(&parser, 0x7F);
    ASSERT_TRUE(parser.line_len == 1);
}

TEST(test_feed_backspace_empty)
{
    setup(NULL);
    capture_reset();
    cli_parser_feed(&parser, '\b');
    ASSERT_TRUE(parser.line_len == 0);
    ASSERT_TRUE(out_len == 0);
}

TEST(test_run_max_args)
{
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping a b c d e f g h i");
    ASSERT_TRUE(err == CLI_OK);
    ASSERT_TRUE(last_argc == CLI_MAX_ARGS);
}

TEST(test_exact_buffer_boundary)
{
    setup(NULL);
    capture_reset();
    char exact[CLI_LINE_BUFSIZE];
    memset(exact, 'a', CLI_LINE_BUFSIZE - 1);
    exact[CLI_LINE_BUFSIZE - 1] = '\0';
    memcpy(exact, "ping ", 5);
    cli_err_t err = cli_parser_run(&parser, exact);
    ASSERT_TRUE(err == CLI_OK);
    ASSERT_TRUE(last_argc == 2);
}

TEST(test_exact_buffer_plus_one)
{
    setup(NULL);
    capture_reset();
    char over[CLI_LINE_BUFSIZE + 1];
    memset(over, 'a', CLI_LINE_BUFSIZE);
    over[CLI_LINE_BUFSIZE] = '\0';
    cli_err_t err = cli_parser_run(&parser, over);
    ASSERT_TRUE(err == CLI_ERR_OVERFLOW);
}

TEST(test_feed_exact_then_enter)
{
    setup(NULL);
    capture_reset();
    for (int i = 0; i < CLI_LINE_BUFSIZE - 1; i++) {
        cli_parser_feed(&parser, 'b');
    }
    ASSERT_TRUE(parser.line_len == CLI_LINE_BUFSIZE - 1);
    cli_parser_feed(&parser, 'c');
    ASSERT_TRUE(parser.line_len == CLI_LINE_BUFSIZE - 1);
}

static void feed_text(const char *text)
{
    while (*text != '\0') {
        cli_parser_feed(&parser, (uint8_t)*text++);
    }
}

/* "ping " then filler until the line is `total` characters long. */
static void feed_long_ping(int total)
{
    feed_text("ping ");
    for (int i = 5; i < total; i++) {
        cli_parser_feed(&parser, '7');
    }
}

/* cli_parser_run refuses this line, so feed must not run a cut copy of it. */
TEST(test_feed_refuses_line_that_overflowed)
{
    setup(NULL);
    capture_reset();
    feed_long_ping(CLI_LINE_BUFSIZE);
    cli_parser_feed(&parser, '\r');
    ASSERT_EQ(last_argc, 0);
    ASSERT_TRUE(strstr(out_buf, "line too long") != NULL);

    /* The refusal covers that line only. */
    feed_text("ping again\r");
    ASSERT_EQ(last_argc, 2);
    ASSERT_TRUE(strcmp(last_argv1, "again") == 0);
}

TEST(test_feed_runs_line_that_exactly_fits)
{
    setup(NULL);
    capture_reset();
    feed_long_ping(CLI_LINE_BUFSIZE - 1);
    cli_parser_feed(&parser, '\r');
    ASSERT_EQ(last_argc, 2);
    ASSERT_TRUE(strstr(out_buf, "line too long") == NULL);
}

/* Backspace cannot bring dropped bytes back. */
TEST(test_feed_overflow_survives_backspace)
{
    setup(NULL);
    capture_reset();
    feed_long_ping(CLI_LINE_BUFSIZE + 10);
    cli_parser_feed(&parser, '\b');
    cli_parser_feed(&parser, '\r');
    ASSERT_EQ(last_argc, 0);
    ASSERT_TRUE(strstr(out_buf, "line too long") != NULL);
}

TEST(test_null_commands)
{
    capture_reset();
    cli_cfg_t cfg = {
        .commands = NULL,
        .num_commands = 5,
        .write_fn = capture_write,
        .user_ctx = NULL,
        .prompt = NULL,
    };
    cli_parser_init(&parser, &cfg);
    ASSERT_TRUE(parser.cfg.num_commands == 0);
    cli_err_t err = cli_parser_run(&parser, "ping");
    ASSERT_TRUE(err == CLI_ERR_NOT_FOUND);
}

/* A table with holes: dispatch steps over them and finds what follows. */
static const cli_cmd_t no_handler[] = {
    { "info", NULL,     "no handler" },
    { "ping", cmd_ping, "test command" },
};

static const cli_cmd_t no_name[] = {
    { NULL,   NULL,     NULL },
    { NULL,   cmd_ping, "no name" },
    { "ping", cmd_ping, "test command" },
};

static void setup_table(const cli_cmd_t *table, size_t count)
{
    setup(NULL);
    cli_cfg_t cfg = {
        .commands = table,
        .num_commands = count,
        .write_fn = capture_write,
    };
    cli_parser_init(&parser, &cfg);
}

TEST(test_entry_without_handler)
{
    setup_table(no_handler, sizeof(no_handler) / sizeof(no_handler[0]));
    ASSERT_TRUE(cli_parser_run(&parser, "info") == CLI_ERR_NOT_FOUND);
    ASSERT_TRUE(strstr(out_buf, "unknown command: info") != NULL);
    ASSERT_TRUE(cli_parser_run(&parser, "ping") == CLI_OK);
    ASSERT_TRUE(last_argc == 1);
}

TEST(test_entry_without_name)
{
    setup_table(no_name, sizeof(no_name) / sizeof(no_name[0]));
    ASSERT_TRUE(cli_parser_run(&parser, "ping") == CLI_OK);
    ASSERT_TRUE(last_argc == 1);
    ASSERT_TRUE(cli_parser_run(&parser, "nope") == CLI_ERR_NOT_FOUND);
}

TEST(test_tab_separated_args)
{
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping\targ1");
    ASSERT_TRUE(err == CLI_OK);
    ASSERT_TRUE(last_argc == 2);
    ASSERT_TRUE(strcmp(last_argv0, "ping") == 0);
    ASSERT_TRUE(strcmp(last_argv1, "arg1") == 0);
}

TEST(test_mixed_spaces_tabs)
{
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping \t arg1");
    ASSERT_TRUE(err == CLI_OK);
    ASSERT_TRUE(last_argc == 2);
    ASSERT_TRUE(strcmp(last_argv1, "arg1") == 0);
}

TEST(test_feed_cr_only)
{
    setup("> ");
    capture_reset();
    cli_parser_feed(&parser, 'A');
    cli_parser_feed(&parser, '\r');
    ASSERT_TRUE(last_argc == 0);
    ASSERT_TRUE(parser.line_len == 0);
}

TEST(test_feed_crlf)
{
    setup(NULL);
    capture_reset();
    last_argc = -1;
    cli_parser_feed(&parser, 'p');
    cli_parser_feed(&parser, 'i');
    cli_parser_feed(&parser, 'n');
    cli_parser_feed(&parser, 'g');
    cli_parser_feed(&parser, '\r');
    ASSERT_TRUE(last_argc == 1);
    cli_parser_feed(&parser, '\n');
    ASSERT_TRUE(parser.line_len == 0);
}

TEST(test_feed_control_chars_ignored)
{
    setup(NULL);
    capture_reset();
    cli_parser_feed(&parser, 0x01);
    cli_parser_feed(&parser, 0x02);
    cli_parser_feed(&parser, 0x1B);
    ASSERT_TRUE(parser.line_len == 0);
}

TEST_MAIN("cli_parser")
    RUN(test_init);
    RUN(test_run_known);
    RUN(test_run_unknown);
    RUN(test_run_empty);
    RUN(test_run_args);
    RUN(test_run_extra_whitespace);
    RUN(test_help);
    RUN(test_handler_error);
    RUN(test_overflow);
    RUN(test_null_safety);
    RUN(test_feed_and_enter);
    RUN(test_feed_backspace);
    RUN(test_feed_overflow);
    RUN(test_feed_del);
    RUN(test_feed_backspace_empty);
    RUN(test_run_max_args);
    RUN(test_exact_buffer_boundary);
    RUN(test_exact_buffer_plus_one);
    RUN(test_feed_exact_then_enter);
    RUN(test_feed_refuses_line_that_overflowed);
    RUN(test_feed_runs_line_that_exactly_fits);
    RUN(test_feed_overflow_survives_backspace);
    RUN(test_null_commands);
    RUN(test_entry_without_handler);
    RUN(test_entry_without_name);
    RUN(test_tab_separated_args);
    RUN(test_mixed_spaces_tabs);
    RUN(test_feed_cr_only);
    RUN(test_feed_crlf);
    RUN(test_feed_control_chars_ignored);
TEST_MAIN_END()

