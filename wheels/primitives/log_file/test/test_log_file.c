#include "test_harness.h"
#include "log_file.h"

#include <string.h>
#include <stdlib.h>

static const char *TMPFILE = "test_log_output.tmp";

static void cleanup(void) {
    remove(TMPFILE);
}

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { *out_len = 0; return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    *out_len = fread(buf, 1, (size_t)sz, f);
    buf[*out_len] = '\0';
    fclose(f);
    return buf;
}

TEST(test_open_write_close) {
    cleanup();
    log_file_t lf;
    ASSERT_EQ(log_file_open(&lf, TMPFILE, 1), 0);

    const char *msg = "[I] [app] boot\n";
    log_file_sink(&lf, msg, strlen(msg));
    log_file_close(&lf);

    size_t len;
    char *content = read_file(TMPFILE, &len);
    ASSERT_TRUE(content != NULL);
    ASSERT_EQ(len, strlen(msg));
    ASSERT_MEMEQ(content, "[I] [app] boot\n");
    free(content);
    cleanup();
}

TEST(test_append_mode) {
    cleanup();
    log_file_t lf;
    log_file_open(&lf, TMPFILE, 1);
    log_file_sink(&lf, "AAA\n", 4);
    log_file_close(&lf);

    log_file_open(&lf, TMPFILE, 1);
    log_file_sink(&lf, "BBB\n", 4);
    log_file_close(&lf);

    size_t len;
    char *content = read_file(TMPFILE, &len);
    ASSERT_EQ(len, 8);
    ASSERT_MEMEQ(content, "AAA\nBBB\n");
    free(content);
    cleanup();
}

TEST(test_attach_stderr) {
    log_file_t lf;
    log_file_attach(&lf, stderr, 0);
    ASSERT_TRUE(lf.fp == stderr);
    log_file_sink(&lf, "[D] [test] stderr\n", 18);
}

TEST(test_multiple_writes) {
    cleanup();
    log_file_t lf;
    log_file_open(&lf, TMPFILE, 0);
    log_file_sink(&lf, "line1\n", 6);
    log_file_sink(&lf, "line2\n", 6);
    log_file_sink(&lf, "line3\n", 6);
    log_file_close(&lf);

    size_t len;
    char *content = read_file(TMPFILE, &len);
    ASSERT_EQ(len, 18);
    ASSERT_MEMEQ(content, "line1\nline2\nline3\n");
    free(content);
    cleanup();
}

TEST(test_open_invalid_path) {
    log_file_t lf;
    ASSERT_EQ(log_file_open(&lf, NULL, 0), -1);
    ASSERT_EQ(log_file_open(NULL, TMPFILE, 0), -1);
}

TEST(test_null_safety) {
    log_file_sink(NULL, "x", 1);

    log_file_t lf;
    memset(&lf, 0, sizeof(lf));
    log_file_sink(&lf, "x", 1);
    log_file_sink(&lf, NULL, 0);
    log_file_close(NULL);
    log_file_close(&lf);
    log_file_attach(NULL, NULL, 0);
}

TEST_MAIN("log_file")
    RUN(test_open_write_close);
    RUN(test_append_mode);
    RUN(test_attach_stderr);
    RUN(test_multiple_writes);
    RUN(test_open_invalid_path);
    RUN(test_null_safety);
TEST_MAIN_END()
