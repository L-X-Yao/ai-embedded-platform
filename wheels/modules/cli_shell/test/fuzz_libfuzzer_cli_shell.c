// SPDX-License-Identifier: MIT
// LibFuzzer harness for cli_shell -- coverage-guided fuzzing
//
// Types the input into a shell and keeps a separate model of the line, so a
// wrong edit shows up at once, and of how the line splits into arguments.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_cli_shell.c ../cli_shell.c -o fuzz_cli_shell
// Run:     ./fuzz_cli_shell -max_total_time=60 corpus_cli_shell/

#include "../cli_shell.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char   line[CLI_SHELL_LINE_SIZE];
    size_t len;
    bool   last_was_cr;
    bool   overflow;
    char   argv[CLI_SHELL_MAX_ARGS][CLI_SHELL_LINE_SIZE];
    int    argc;
    int    calls;
    size_t echoed;
    char   echo;
} model_t;

static void write_out(void *ctx, const char *data, uint16_t len) {
    model_t *m = ctx;
    if (!data || len == 0) __builtin_trap();
    m->echoed += len;
    m->echo = data[len - 1];
}

static void on_command(void *ctx, int argc, const char **argv, const cli_shell_io_t *io) {
    model_t *m = ctx;
    if (argc != m->argc) __builtin_trap();
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], m->argv[i]) != 0) __builtin_trap();
    }
    m->calls++;
    if (io && io->write) io->write(io->ctx, "ok", 2);
}

// Space separates arguments, and a double quote opens one that runs to the next
// quote or the end of the line.
static void model_split(model_t *m) {
    size_t i = 0;
    m->argc = 0;
    while (m->argc < CLI_SHELL_MAX_ARGS) {
        while (i < m->len && m->line[i] == ' ') i++;
        if (i >= m->len) break;

        bool quoted = m->line[i] == '"';
        char stop = quoted ? '"' : ' ';
        size_t n = 0;
        if (quoted) i++;
        while (i < m->len && m->line[i] != stop) m->argv[m->argc][n++] = m->line[i++];
        i++;
        m->argv[m->argc][n] = '\0';
        m->argc++;
    }
}

static void type_byte(cli_shell_t *sh, model_t *m, uint8_t byte) {
    int calls = m->calls;
    size_t echoed = m->echoed;
    bool newline = byte == '\r' || byte == '\n';
    bool ignored_lf = byte == '\n' && m->last_was_cr;

    if (newline && !ignored_lf) model_split(m);

    bool printable = byte >= 0x20 && byte < 0x7F;
    bool room = m->len < CLI_SHELL_LINE_SIZE - 1;
    cli_shell_feed(sh, byte);

    if (newline) {
        m->last_was_cr = byte == '\r';
        if (ignored_lf) {
            m->last_was_cr = false;
            if (m->echoed != echoed) __builtin_trap();
        } else {
            // A line that lost bytes must not run, whatever is left of it.
            if (m->overflow && m->calls != calls) __builtin_trap();
            m->overflow = false;
            m->len = 0;
        }
    } else {
        m->last_was_cr = false;
        if ((byte == '\b' || byte == 0x7F) && m->len > 0) m->len--;
        if (printable && room) m->line[m->len++] = (char)byte;
        if (printable && !room) m->overflow = true;
    }

    if (sh->line_pos != m->len || memcmp(sh->line, m->line, m->len) != 0) __builtin_trap();
    if (sh->io && printable && room && (m->echoed != echoed + 1 || m->echo != (char)byte)) {
        __builtin_trap();
    }
    if (!newline && m->calls != calls) __builtin_trap();
}

static const char *const names[] = {
    "a", "ab", "run", "set", "get", "log", "x1", "x2", "x3", "x4",
    "x5", "x6", "x7", "x8", "x9", "y1", "y2", "y3",
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_input_t in;
    fuzz_input_init(&in, data, size);

    cli_shell_t *sh = malloc(sizeof(*sh));
    model_t *m = calloc(1, sizeof(*m));
    if (sh && m) {
        cli_shell_io_t io = { write_out, m };
        uint8_t setup = fuzz_u8(&in);
        const cli_shell_io_t *use = (setup & 1) ? NULL : &io;
        if (cli_shell_init(sh, use, (setup & 2) ? NULL : "> ") != CLI_SHELL_OK) __builtin_trap();

        // Fill the table past its size. The built-in help holds the first slot.
        size_t want = (setup >> 2) % (sizeof(names) / sizeof(names[0]) + 1);
        for (size_t i = 0; i < want; i++) {
            cli_shell_err_t err = cli_shell_register(sh, names[i], (i & 1) ? "h" : NULL, on_command, m);
            if ((i + 1 < CLI_SHELL_MAX_CMDS) != (err == CLI_SHELL_OK)) __builtin_trap();
        }

        while (in.left > 0) {
            type_byte(sh, m, fuzz_u8(&in));
        }
    }
    free(sh);
    free(m);
    return 0;
}
