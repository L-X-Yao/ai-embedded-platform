// SPDX-License-Identifier: MIT
// LibFuzzer harness for json_rpc -- coverage-guided fuzzing
//
// Scans the input as raw text, then inside an envelope that spells the keys so
// the value parsers run on fuzz data. Built messages have to read back the same.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_json_rpc.c ../json_rpc.c -o fuzz_json_rpc
// Run:     ./fuzz_json_rpc -max_total_time=60 corpus_json_rpc/

#include "../json_rpc.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define NAME_MAX_LEN 32

static void scan(const char *text, size_t len) {
    (void)json_rpc_validate(text, len);

    size_t start = 0;
    size_t name_len = 0;
    if (json_rpc_extract_method(text, len, &start, &name_len) == JSON_RPC_OK &&
        start + name_len > len) {
        __builtin_trap();
    }

    int32_t id = 0;
    (void)json_rpc_extract_id(text, len, &id);
}

static void scan_wrapped(const char *head, const char *tail, const uint8_t *data, size_t size) {
    size_t head_len = strlen(head);
    size_t tail_len = strlen(tail);
    size_t len = head_len + size + tail_len;

    // Exact-size text, so ASan sees any read past the end.
    char *text = malloc(len ? len : 1);
    if (!text) return;

    memcpy(text, head, head_len);
    if (size > 0) memcpy(text + head_len, data, size);
    memcpy(text + head_len + size, tail, tail_len);
    scan(text, len);
    free(text);
}

static void check_request(const char *method, int32_t id) {
    char buf[128];
    size_t len = 0;
    if (json_rpc_build_request(buf, sizeof(buf), method, NULL, id, &len) != JSON_RPC_OK) {
        __builtin_trap();
    }
    if (json_rpc_validate(buf, len) != JSON_RPC_TYPE_REQUEST) __builtin_trap();

    size_t start = 0;
    size_t name_len = 0;
    if (json_rpc_extract_method(buf, len, &start, &name_len) != JSON_RPC_OK ||
        name_len != strlen(method) || memcmp(&buf[start], method, name_len) != 0) {
        __builtin_trap();
    }

    int32_t got = 0;
    if (json_rpc_extract_id(buf, len, &got) != JSON_RPC_OK || got != id) __builtin_trap();

    // Exact-size buffers: one byte short has to fail, the real size has to fit.
    char *tight = malloc(len + 1);
    if (!tight) return;
    size_t again = 0;
    if (json_rpc_build_request(tight, len, method, NULL, id, &again) != JSON_RPC_ERR_OVERFLOW) {
        __builtin_trap();
    }
    if (json_rpc_build_request(tight, len + 1, method, NULL, id, &again) != JSON_RPC_OK) {
        __builtin_trap();
    }
    free(tight);
}

static void check_replies(const char *text, int32_t id, int32_t code) {
    char buf[128];
    size_t len = 0;
    int32_t got = 0;

    if (json_rpc_build_response(buf, sizeof(buf), "1", id, &len) != JSON_RPC_OK) __builtin_trap();
    if (json_rpc_validate(buf, len) != JSON_RPC_TYPE_RESPONSE) __builtin_trap();
    if (json_rpc_extract_id(buf, len, &got) != JSON_RPC_OK || got != id) __builtin_trap();

    if (json_rpc_build_error(buf, sizeof(buf), code, text, id, &len) != JSON_RPC_OK) {
        __builtin_trap();
    }
    if (json_rpc_validate(buf, len) != JSON_RPC_TYPE_ERROR) __builtin_trap();
}

// Any text has to stay inside its string, so the frame keeps its type and id.
static void check_any_text(fuzz_input_t *in, int32_t id, int32_t code) {
    char text[NAME_MAX_LEN + 1];
    text[fuzz_take(in, text, NAME_MAX_LEN)] = 0;

    char buf[512];
    size_t len = 0;
    int32_t got = 0;

    if (json_rpc_build_request(buf, sizeof(buf), text, NULL, id, &len) != JSON_RPC_OK) {
        __builtin_trap();
    }
    if (json_rpc_validate(buf, len) != JSON_RPC_TYPE_REQUEST) __builtin_trap();
    if (json_rpc_extract_id(buf, len, &got) != JSON_RPC_OK || got != id) __builtin_trap();

    if (json_rpc_build_error(buf, sizeof(buf), code, text, id, &len) != JSON_RPC_OK) {
        __builtin_trap();
    }
    if (json_rpc_validate(buf, len) != JSON_RPC_TYPE_ERROR) __builtin_trap();
    if (json_rpc_extract_id(buf, len, &got) != JSON_RPC_OK || got != id) __builtin_trap();
}

static void round_trip(fuzz_input_t *in) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz._";
    char name[NAME_MAX_LEN + 1];

    uint32_t id_hi = fuzz_u16(in);
    int32_t id = (int32_t)((id_hi << 16) | fuzz_u16(in));
    int32_t code = (int32_t)fuzz_u16(in) - 32768;

    size_t len = 1 + fuzz_u8(in) % NAME_MAX_LEN;
    for (size_t i = 0; i < len; i++) {
        name[i] = alphabet[fuzz_u8(in) % (sizeof(alphabet) - 1)];
    }
    name[len] = 0;

    check_request(name, id);
    check_replies(name, id, code);
    check_any_text(in, id, code);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    scan((const char *)data, size);

    scan_wrapped("{\"jsonrpc\":\"2.0\",\"id\":", "}", data, size);
    scan_wrapped("{\"jsonrpc\":\"2.0\",\"method\":\"", "\",\"id\":1}", data, size);
    scan_wrapped("{\"jsonrpc\":", ",\"result\":1}", data, size);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    round_trip(&in);

    return 0;
}
