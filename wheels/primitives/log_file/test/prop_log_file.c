/*
 * prop_log_file.c — property-based tests for log_file
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_log_file.c ../log_file.c -o prop_log_file
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_file.h"
#include <string.h>
#include <stdlib.h>

/* Property: sink never crashes on arbitrary input via attached FILE* */
PROPERTY(attach_no_crash, 10000) {
    log_file_t lf;
    FILE *fp = tmpfile();
    PROP_ASSERT(fp != NULL);

    int auto_flush = ARBI_UINT8(seed) % 2;
    log_file_attach(&lf, fp, auto_flush);

    char buf[128];
    unsigned len = (unsigned)ARBI_RANGE(seed, 0, 127);
    ARBI_FILL(seed, (uint8_t *)buf, len);
    for (unsigned i = 0; i < len; i++) {
        if (buf[i] == '\0') buf[i] = 'x';
    }

    log_file_sink(&lf, buf, len);
    fclose(fp);
    PROP_ASSERT(1);
}

/* Property: written bytes can be read back from FILE */
PROPERTY(write_read_roundtrip, 10000) {
    log_file_t lf;
    FILE *fp = tmpfile();
    PROP_ASSERT(fp != NULL);

    log_file_attach(&lf, fp, 1);

    char input[64];
    unsigned len = (unsigned)ARBI_RANGE(seed, 1, 64);
    ARBI_FILL(seed, (uint8_t *)input, len);
    for (unsigned i = 0; i < len; i++) {
        if (input[i] == '\0') input[i] = 'A';
    }

    log_file_sink(&lf, input, len);

    /* Read back */
    rewind(fp);
    char output[64];
    size_t read_len = fread(output, 1, sizeof(output), fp);
    fclose(fp);

    PROP_ASSERT_EQ(read_len, len);
    PROP_ASSERT_MEM_EQ(output, input, len);
}

/* Property: multiple writes accumulate correctly */
PROPERTY(multi_write_accumulate, 5000) {
    log_file_t lf;
    FILE *fp = tmpfile();
    PROP_ASSERT(fp != NULL);

    log_file_attach(&lf, fp, 1);

    size_t total = 0;
    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 10);
    for (unsigned i = 0; i < ops; i++) {
        char buf[16];
        unsigned len = (unsigned)ARBI_RANGE(seed, 1, 16);
        ARBI_FILL(seed, (uint8_t *)buf, len);
        for (unsigned j = 0; j < len; j++) {
            if (buf[j] == '\0') buf[j] = 'A';
        }
        log_file_sink(&lf, buf, len);
        total += len;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fclose(fp);

    PROP_ASSERT_EQ((size_t)file_size, total);
}

TEST_MAIN("log_file properties") {
    RUN(attach_no_crash);
    RUN(write_read_roundtrip);
    RUN(multi_write_accumulate);
}
TEST_MAIN_END()
