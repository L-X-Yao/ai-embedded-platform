// SPDX-License-Identifier: MIT
// LibFuzzer harness for cli_parser -- coverage-guided fuzzing
//
// Types the input into the line editor, then runs it as one command line. Handlers
// check the argument vector, the output callback reads everything it is given.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_cli_parser.c ../cli_parser.c -o fuzz_cli_parser
// Run:     ./fuzz_cli_parser -max_total_time=60 corpus_cli_parser/

#include "../cli_parser.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_output_sum;

static void write_out(const char *buf, size_t len, void *user_ctx) {
    (void)user_ctx;
    for (size_t i = 0; i < len; i++) g_output_sum += (uint8_t)buf[i];
}

static cli_err_t handle(int argc, const char *argv[], void *user_ctx) {
    (void)user_ctx;
    if (argc < 1 || argc > CLI_MAX_ARGS) __builtin_trap();
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (len == 0 || len >= CLI_LINE_BUFSIZE) __builtin_trap();
        if (memchr(argv[i], ' ', len) || memchr(argv[i], '\t', len)) __builtin_trap();
    }
    return argc > 2 ? CLI_ERR_ARGS : CLI_OK;
}

static const cli_cmd_t commands[] = {
    { "get", handle, "read a value" },
    { "set", handle, NULL },
};

static void run_line(cli_parser_t *cli, const uint8_t *data, size_t size) {
    // Exact-size string, so ASan sees any read past the terminator.
    char *line = malloc(size + 1);
    if (!line) return;
    if (size > 0) memcpy(line, data, size);
    line[size] = 0;

    cli_err_t err = cli_parser_run(cli, line);
    if (strlen(line) >= CLI_LINE_BUFSIZE && err != CLI_ERR_OVERFLOW) __builtin_trap();
    free(line);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    cli_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.commands = commands;
    cfg.num_commands = sizeof(commands) / sizeof(commands[0]);
    cfg.write_fn = write_out;
    cfg.prompt = "> ";

    cli_parser_t cli;
    memset(&cli, 0, sizeof(cli));
    cli_parser_init(&cli, &cfg);

    for (size_t i = 0; i < size; i++) {
        cli_parser_feed(&cli, data[i]);
        if (cli.line_len >= CLI_LINE_BUFSIZE) __builtin_trap();
    }

    run_line(&cli, data, size);

    return 0;
}
