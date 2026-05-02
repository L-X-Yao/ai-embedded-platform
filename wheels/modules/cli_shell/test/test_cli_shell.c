// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "cli_shell.h"
#include <string.h>

/* ---- Output capture ---- */

static char out_buf[4096];
static size_t out_len;

static void capture_write(void *ctx, const char *data, uint16_t len)
{
    (void)ctx;
    if (out_len + len < sizeof(out_buf)) {
        memcpy(out_buf + out_len, data, len);
        out_len += len;
    }
    out_buf[out_len] = '\0';
}

static void capture_reset(void)
{
    out_len = 0;
    out_buf[0] = '\0';
}

/* ---- Test command state ---- */

static int handler_called;
static int handler_argc;
static char handler_argv[CLI_SHELL_MAX_ARGS][64];

static void cmd_test(void *ctx, int argc, const char **argv,
                     const cli_shell_io_t *io)
{
    (void)ctx;
    (void)io;
    handler_called = 1;
    handler_argc = argc;
    for (int i = 0; i < argc && i < CLI_SHELL_MAX_ARGS; i++) {
        strncpy(handler_argv[i], argv[i], sizeof(handler_argv[i]) - 1);
        handler_argv[i][sizeof(handler_argv[i]) - 1] = '\0';
    }
}

static void cmd_echo(void *ctx, int argc, const char **argv,
                     const cli_shell_io_t *io)
{
    (void)ctx;
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            io->write(io->ctx, " ", 1);
        }
        io->write(io->ctx, argv[i], (uint16_t)strlen(argv[i]));
    }
    io->write(io->ctx, "\r\n", 2);
}

/* ---- Helpers ---- */

static cli_shell_t shell;
static const cli_shell_io_t test_io = { capture_write, NULL };

static void feed_str(const char *s)
{
    for (size_t i = 0; i < strlen(s); i++) {
        cli_shell_feed(&shell, (uint8_t)s[i]);
    }
}

static void reset_handler(void)
{
    handler_called = 0;
    handler_argc   = 0;
    memset(handler_argv, 0, sizeof(handler_argv));
}

static void setup(const char *prompt)
{
    capture_reset();
    reset_handler();
    cli_shell_init(&shell, &test_io, prompt);
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    cli_shell_err_t err = cli_shell_init(&shell, &test_io, "> ");
    ASSERT_EQ(err, CLI_SHELL_OK);
    ASSERT_EQ(shell.cmd_count, 1);  /* built-in help */
    ASSERT_EQ(shell.line_pos, 0);
}

TEST(test_init_null_shell)
{
    cli_shell_err_t err = cli_shell_init(NULL, &test_io, "> ");
    ASSERT_EQ(err, CLI_SHELL_ERR_PARAM);
}

TEST(test_init_prints_prompt)
{
    capture_reset();
    cli_shell_init(&shell, &test_io, "$ ");
    ASSERT_TRUE(strstr(out_buf, "$ ") != NULL);
}

TEST(test_register_and_call)
{
    setup(NULL);
    cli_shell_register(&shell, "test", "a test cmd", cmd_test, NULL);
    capture_reset();
    reset_handler();
    feed_str("test\r");
    ASSERT_TRUE(handler_called);
    ASSERT_EQ(handler_argc, 1);
    ASSERT_TRUE(strcmp(handler_argv[0], "test") == 0);
}

TEST(test_handler_receives_correct_argc_argv)
{
    setup(NULL);
    cli_shell_register(&shell, "led", "control led", cmd_test, NULL);
    reset_handler();
    feed_str("led on 3\r");
    ASSERT_EQ(handler_argc, 3);
    ASSERT_TRUE(strcmp(handler_argv[0], "led") == 0);
    ASSERT_TRUE(strcmp(handler_argv[1], "on") == 0);
    ASSERT_TRUE(strcmp(handler_argv[2], "3") == 0);
}

TEST(test_builtin_help)
{
    setup(NULL);
    cli_shell_register(&shell, "ping", "test connectivity", cmd_test, NULL);
    cli_shell_register(&shell, "reboot", "restart device", cmd_test, NULL);
    capture_reset();
    feed_str("help\r");
    /* help output should list all commands including help itself */
    ASSERT_TRUE(strstr(out_buf, "help") != NULL);
    ASSERT_TRUE(strstr(out_buf, "ping") != NULL);
    ASSERT_TRUE(strstr(out_buf, "test connectivity") != NULL);
    ASSERT_TRUE(strstr(out_buf, "reboot") != NULL);
    ASSERT_TRUE(strstr(out_buf, "restart device") != NULL);
}

TEST(test_unknown_command)
{
    setup(NULL);
    capture_reset();
    feed_str("nosuch\r");
    ASSERT_TRUE(strstr(out_buf, "unknown") != NULL);
    ASSERT_TRUE(strstr(out_buf, "nosuch") != NULL);
    ASSERT_TRUE(strstr(out_buf, "help") != NULL);
}

TEST(test_empty_line)
{
    setup("> ");
    capture_reset();
    reset_handler();
    feed_str("\r");
    /* empty line: just prompt again, no command dispatched */
    ASSERT_FALSE(handler_called);
    ASSERT_TRUE(strstr(out_buf, "> ") != NULL);
}

TEST(test_backspace_handling)
{
    setup(NULL);
    cli_shell_register(&shell, "test", NULL, cmd_test, NULL);
    reset_handler();
    capture_reset();
    /* type "tesx" then backspace + "t" to correct to "test" */
    feed_str("tesx\bt\r");
    ASSERT_TRUE(handler_called);
    ASSERT_TRUE(strcmp(handler_argv[0], "test") == 0);
}

TEST(test_line_overflow)
{
    setup(NULL);
    cli_shell_register(&shell, "test", NULL, cmd_test, NULL);
    reset_handler();
    /* feed more chars than CLI_SHELL_LINE_SIZE */
    for (int i = 0; i < CLI_SHELL_LINE_SIZE + 20; i++) {
        cli_shell_feed(&shell, 'a');
    }
    /* line_pos should be capped at max */
    ASSERT_EQ(shell.line_pos, CLI_SHELL_LINE_SIZE - 1);
}

TEST(test_multiple_commands_in_sequence)
{
    setup(NULL);
    cli_shell_register(&shell, "test", NULL, cmd_test, NULL);

    /* first command */
    reset_handler();
    feed_str("test arg1\r");
    ASSERT_TRUE(handler_called);
    ASSERT_EQ(handler_argc, 2);
    ASSERT_TRUE(strcmp(handler_argv[1], "arg1") == 0);

    /* second command */
    reset_handler();
    feed_str("test arg2\r");
    ASSERT_TRUE(handler_called);
    ASSERT_EQ(handler_argc, 2);
    ASSERT_TRUE(strcmp(handler_argv[1], "arg2") == 0);
}

TEST(test_register_max_commands)
{
    setup(NULL);
    /* help is already registered (1 slot used) */
    cli_shell_err_t err;
    char names[CLI_SHELL_MAX_CMDS][8];
    for (int i = 1; i < CLI_SHELL_MAX_CMDS; i++) {
        snprintf(names[i], sizeof(names[i]), "cmd%d", i);
        err = cli_shell_register(&shell, names[i], NULL, cmd_test, NULL);
        ASSERT_EQ(err, CLI_SHELL_OK);
    }
    /* now full -- next register should fail */
    err = cli_shell_register(&shell, "overflow", NULL, cmd_test, NULL);
    ASSERT_EQ(err, CLI_SHELL_ERR_FULL);
}

TEST(test_echo_input)
{
    setup(NULL);
    capture_reset();
    cli_shell_feed(&shell, 'a');
    cli_shell_feed(&shell, 'b');
    cli_shell_feed(&shell, 'c');
    /* output should contain the echoed characters */
    ASSERT_TRUE(out_buf[0] == 'a');
    ASSERT_TRUE(out_buf[1] == 'b');
    ASSERT_TRUE(out_buf[2] == 'c');
}

TEST(test_quoted_argument)
{
    setup(NULL);
    cli_shell_register(&shell, "log", "log message", cmd_test, NULL);
    reset_handler();
    feed_str("log \"hello world\"\r");
    ASSERT_EQ(handler_argc, 2);
    ASSERT_TRUE(strcmp(handler_argv[0], "log") == 0);
    ASSERT_TRUE(strcmp(handler_argv[1], "hello world") == 0);
}

TEST(test_crlf_handling)
{
    setup(NULL);
    cli_shell_register(&shell, "test", NULL, cmd_test, NULL);

    /* CRLF should be treated as single newline */
    reset_handler();
    feed_str("test\r\n");
    ASSERT_TRUE(handler_called);
    ASSERT_EQ(handler_argc, 1);

    /* verify no double execution */
    reset_handler();
    feed_str("test\r\n");
    ASSERT_TRUE(handler_called);
    ASSERT_EQ(handler_argc, 1);
}

TEST(test_reinit)
{
    setup("> ");
    cli_shell_register(&shell, "test", NULL, cmd_test, NULL);
    ASSERT_EQ(shell.cmd_count, 2);

    /* re-init should reset everything */
    cli_shell_err_t err = cli_shell_init(&shell, &test_io, "$ ");
    ASSERT_EQ(err, CLI_SHELL_OK);
    ASSERT_EQ(shell.cmd_count, 1);  /* only built-in help */
    ASSERT_EQ(shell.line_pos, 0);
}

TEST(test_register_null_params)
{
    setup(NULL);
    cli_shell_err_t err;
    err = cli_shell_register(NULL, "x", NULL, cmd_test, NULL);
    ASSERT_EQ(err, CLI_SHELL_ERR_PARAM);
    err = cli_shell_register(&shell, NULL, NULL, cmd_test, NULL);
    ASSERT_EQ(err, CLI_SHELL_ERR_PARAM);
    err = cli_shell_register(&shell, "x", NULL, NULL, NULL);
    ASSERT_EQ(err, CLI_SHELL_ERR_PARAM);
}

TEST(test_feed_null_shell)
{
    cli_shell_feed(NULL, 'x');
    /* should not crash */
    ASSERT_TRUE(1);
}

TEST(test_control_chars_ignored)
{
    setup(NULL);
    capture_reset();
    cli_shell_feed(&shell, 0x01);
    cli_shell_feed(&shell, 0x02);
    cli_shell_feed(&shell, 0x1B);  /* ESC */
    ASSERT_EQ(shell.line_pos, 0);
}

TEST(test_backspace_on_empty)
{
    setup(NULL);
    capture_reset();
    cli_shell_feed(&shell, '\b');
    ASSERT_EQ(shell.line_pos, 0);
    ASSERT_EQ(out_len, 0);  /* no echo for backspace on empty */
}

TEST(test_del_key)
{
    setup(NULL);
    capture_reset();
    cli_shell_feed(&shell, 'a');
    cli_shell_feed(&shell, 'b');
    cli_shell_feed(&shell, 0x7F);  /* DEL */
    ASSERT_EQ(shell.line_pos, 1);
}

TEST(test_handler_uses_io_write)
{
    setup(NULL);
    cli_shell_register(&shell, "echo", "echo args", cmd_echo, NULL);
    capture_reset();
    feed_str("echo hello\r");
    ASSERT_TRUE(strstr(out_buf, "hello") != NULL);
}

TEST(test_whitespace_only_line)
{
    setup(NULL);
    reset_handler();
    feed_str("   \r");
    ASSERT_FALSE(handler_called);
}

TEST(test_handler_ctx_passed)
{
    setup(NULL);
    static int ctx_val = 42;
    static int received_ctx;
    /* handler that checks ctx */
    static void (*orig)(void*, int, const char**, const cli_shell_io_t*);
    (void)orig;

    /* use a simple inline-ish approach: register with ctx=&ctx_val */
    cli_shell_register(&shell, "ctxtest", NULL, cmd_test, &ctx_val);

    /* the cmd_test handler ignores ctx, but we can verify the registration */
    ASSERT_TRUE(shell.cmds[shell.cmd_count - 1].ctx == &ctx_val);
    (void)received_ctx;
}

TEST(test_lf_only)
{
    setup(NULL);
    cli_shell_register(&shell, "test", NULL, cmd_test, NULL);
    reset_handler();
    feed_str("test\n");
    ASSERT_TRUE(handler_called);
    ASSERT_EQ(handler_argc, 1);
}

TEST(test_multiple_quoted_args)
{
    setup(NULL);
    cli_shell_register(&shell, "set", "set key value", cmd_test, NULL);
    reset_handler();
    feed_str("set \"key one\" \"val two\"\r");
    ASSERT_EQ(handler_argc, 3);
    ASSERT_TRUE(strcmp(handler_argv[1], "key one") == 0);
    ASSERT_TRUE(strcmp(handler_argv[2], "val two") == 0);
}

/* ================================================================ */

TEST_MAIN("cli_shell")
    RUN(test_init_valid);
    RUN(test_init_null_shell);
    RUN(test_init_prints_prompt);
    RUN(test_register_and_call);
    RUN(test_handler_receives_correct_argc_argv);
    RUN(test_builtin_help);
    RUN(test_unknown_command);
    RUN(test_empty_line);
    RUN(test_backspace_handling);
    RUN(test_line_overflow);
    RUN(test_multiple_commands_in_sequence);
    RUN(test_register_max_commands);
    RUN(test_echo_input);
    RUN(test_quoted_argument);
    RUN(test_crlf_handling);
    RUN(test_reinit);
    RUN(test_register_null_params);
    RUN(test_feed_null_shell);
    RUN(test_control_chars_ignored);
    RUN(test_backspace_on_empty);
    RUN(test_del_key);
    RUN(test_handler_uses_io_write);
    RUN(test_whitespace_only_line);
    RUN(test_handler_ctx_passed);
    RUN(test_lf_only);
    RUN(test_multiple_quoted_args);
TEST_MAIN_END()
