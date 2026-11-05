// SPDX-License-Identifier: MIT
// Corpus replay driver for libFuzzer harnesses.
//
// Runs every file in the given directories through LLVMFuzzerTestOneInput
// without the fuzzer runtime, so a gcc --coverage build can measure how much
// of a component the saved corpus reaches. Linked by run_libfuzzer.sh
// --coverage in place of libFuzzer's own main.
//
// Usage: replay CORPUS_DIR [CORPUS_DIR ...]

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static unsigned long replay_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    // Exact-size heap copy, the same shape libFuzzer hands to the harness.
    size_t size = (size_t)st.st_size;
    uint8_t *data = malloc(size ? size : 1);
    if (!data) {
        fclose(f);
        return 0;
    }
    size_t got = fread(data, 1, size, f);
    fclose(f);

    LLVMFuzzerTestOneInput(data, got);
    free(data);
    return 1;
}

int main(int argc, char **argv) {
    unsigned long inputs = 0;

    // libFuzzer always tries the empty input first.
    uint8_t empty = 0;
    LLVMFuzzerTestOneInput(&empty, 0);

    for (int i = 1; i < argc; i++) {
        DIR *dir = opendir(argv[i]);
        if (!dir) {
            fprintf(stderr, "cannot open %s\n", argv[i]);
            return 1;
        }
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            char path[4096];
            if (ent->d_name[0] == '.') continue;
            snprintf(path, sizeof(path), "%s/%s", argv[i], ent->d_name);
            inputs += replay_file(path);
        }
        closedir(dir);
    }

    printf("replayed %lu inputs\n", inputs);
    return 0;
}
