#include <stdio.h>
#include <string.h>
#include "../cli_parser.h"

static int tests_run = 0;
static int tests_fail = 0;

#define ASSERT(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s line %d: %s\n", __func__, __LINE__, #cond); \
        tests_fail++; \
    } \
} while (0)

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

static void test_init(void)
{
    printf("%s\n", __func__);
    setup("> ");
    ASSERT(parser.line_len == 0);
    ASSERT(strstr(out_buf, "> ") != NULL);
}

static void test_run_known(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping");
    ASSERT(err == CLI_OK);
    ASSERT(last_argc == 1);
    ASSERT(strcmp(last_argv0, "ping") == 0);
}

static void test_run_unknown(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "nosuchcmd");
    ASSERT(err == CLI_ERR_NOT_FOUND);
    ASSERT(strstr(out_buf, "unknown command") != NULL);
}

static void test_run_empty(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    cli_err_t err = cli_parser_run(&parser, "");
    ASSERT(err == CLI_OK);
    err = cli_parser_run(&parser, "   ");
    ASSERT(err == CLI_OK);
}

static void test_run_args(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping world");
    ASSERT(err == CLI_OK);
    ASSERT(last_argc == 2);
    ASSERT(strcmp(last_argv0, "ping") == 0);
    ASSERT(strcmp(last_argv1, "world") == 0);
}

static void test_run_extra_whitespace(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "  ping   hello  ");
    ASSERT(err == CLI_OK);
    ASSERT(last_argc == 2);
    ASSERT(strcmp(last_argv1, "hello") == 0);
}

static void test_help(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "help");
    ASSERT(err == CLI_OK);
    ASSERT(strstr(out_buf, "ping") != NULL);
    ASSERT(strstr(out_buf, "test command") != NULL);
    ASSERT(strstr(out_buf, "help") != NULL);
}

static void test_handler_error(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    cli_err_t err = cli_parser_run(&parser, "fail");
    ASSERT(err == CLI_ERR_ARGS);
}

static void test_overflow(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    char long_line[CLI_LINE_BUFSIZE + 10];
    memset(long_line, 'a', sizeof(long_line) - 1);
    long_line[sizeof(long_line) - 1] = '\0';
    cli_err_t err = cli_parser_run(&parser, long_line);
    ASSERT(err == CLI_ERR_OVERFLOW);
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    cli_parser_init(NULL, NULL);
    cli_parser_feed(NULL, 'x');
    cli_err_t err = cli_parser_run(NULL, "ping");
    ASSERT(err == CLI_ERR_ARGS);
    setup(NULL);
    err = cli_parser_run(&parser, NULL);
    ASSERT(err == CLI_ERR_ARGS);
}

static void test_feed_and_enter(void)
{
    printf("%s\n", __func__);
    setup("> ");
    capture_reset();
    const char *input = "ping";
    for (size_t i = 0; i < strlen(input); i++) {
        cli_parser_feed(&parser, (uint8_t)input[i]);
    }
    cli_parser_feed(&parser, '\r');
    ASSERT(last_argc == 1);
    ASSERT(strcmp(last_argv0, "ping") == 0);
    ASSERT(strstr(out_buf, "> ") != NULL);
}

static void test_feed_backspace(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_parser_feed(&parser, 'x');
    cli_parser_feed(&parser, '\b');
    cli_parser_feed(&parser, '\r');
    ASSERT(last_argc == 0);
}

static void test_feed_overflow(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    for (int i = 0; i < CLI_LINE_BUFSIZE + 10; i++) {
        cli_parser_feed(&parser, 'a');
    }
    ASSERT(parser.line_len == CLI_LINE_BUFSIZE - 1);
}

static void test_feed_del(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_parser_feed(&parser, 'a');
    cli_parser_feed(&parser, 'b');
    cli_parser_feed(&parser, 0x7F);
    ASSERT(parser.line_len == 1);
}

static void test_feed_backspace_empty(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_parser_feed(&parser, '\b');
    ASSERT(parser.line_len == 0);
    ASSERT(out_len == 0);
}

static void test_run_max_args(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping a b c d e f g h i");
    ASSERT(err == CLI_OK);
    ASSERT(last_argc == CLI_MAX_ARGS);
}

static void test_exact_buffer_boundary(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    char exact[CLI_LINE_BUFSIZE];
    memset(exact, 'a', CLI_LINE_BUFSIZE - 1);
    exact[CLI_LINE_BUFSIZE - 1] = '\0';
    memcpy(exact, "ping ", 5);
    cli_err_t err = cli_parser_run(&parser, exact);
    ASSERT(err == CLI_OK);
    ASSERT(last_argc == 2);
}

static void test_exact_buffer_plus_one(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    char over[CLI_LINE_BUFSIZE + 1];
    memset(over, 'a', CLI_LINE_BUFSIZE);
    over[CLI_LINE_BUFSIZE] = '\0';
    cli_err_t err = cli_parser_run(&parser, over);
    ASSERT(err == CLI_ERR_OVERFLOW);
}

static void test_feed_exact_then_enter(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    for (int i = 0; i < CLI_LINE_BUFSIZE - 1; i++) {
        cli_parser_feed(&parser, 'b');
    }
    ASSERT(parser.line_len == CLI_LINE_BUFSIZE - 1);
    cli_parser_feed(&parser, 'c');
    ASSERT(parser.line_len == CLI_LINE_BUFSIZE - 1);
}

static void test_null_commands(void)
{
    printf("%s\n", __func__);
    capture_reset();
    cli_cfg_t cfg = {
        .commands = NULL,
        .num_commands = 5,
        .write_fn = capture_write,
        .user_ctx = NULL,
        .prompt = NULL,
    };
    cli_parser_init(&parser, &cfg);
    ASSERT(parser.cfg.num_commands == 0);
    cli_err_t err = cli_parser_run(&parser, "ping");
    ASSERT(err == CLI_ERR_NOT_FOUND);
}

static void test_tab_separated_args(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping\targ1");
    ASSERT(err == CLI_OK);
    ASSERT(last_argc == 2);
    ASSERT(strcmp(last_argv0, "ping") == 0);
    ASSERT(strcmp(last_argv1, "arg1") == 0);
}

static void test_mixed_spaces_tabs(void)
{
    printf("%s\n", __func__);
    setup(NULL);
    capture_reset();
    cli_err_t err = cli_parser_run(&parser, "ping \t arg1");
    ASSERT(err == CLI_OK);
    ASSERT(last_argc == 2);
    ASSERT(strcmp(last_argv1, "arg1") == 0);
}

int main(void)
{
    printf("test_cli_parser\n");
    test_init();
    test_run_known();
    test_run_unknown();
    test_run_empty();
    test_run_args();
    test_run_extra_whitespace();
    test_help();
    test_handler_error();
    test_overflow();
    test_null_safety();
    test_feed_and_enter();
    test_feed_backspace();
    test_feed_overflow();
    test_feed_del();
    test_feed_backspace_empty();
    test_run_max_args();
    test_exact_buffer_boundary();
    test_exact_buffer_plus_one();
    test_feed_exact_then_enter();
    test_null_commands();
    test_tab_separated_args();
    test_mixed_spaces_tabs();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
