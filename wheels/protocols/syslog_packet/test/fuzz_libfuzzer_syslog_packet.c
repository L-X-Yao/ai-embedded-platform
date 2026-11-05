// SPDX-License-Identifier: MIT
// LibFuzzer harness for syslog_packet -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_syslog_packet.c ../syslog_packet.c -o fuzz_syslog_packet
// Run:     ./fuzz_syslog_packet -max_total_time=60 corpus_syslog_packet/

#include "../syslog_packet.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Exercise syslog_parse_pri with fuzz data as string
    {
        // Need NUL-terminated string for parse_pri
        size_t copy_len = size < 512 ? size : 512;
        char str_buf[513];
        memcpy(str_buf, data, copy_len);
        str_buf[copy_len] = '\0';

        uint8_t pri = 0;
        uint16_t offset = 0;
        syslog_parse_pri(str_buf, (uint16_t)copy_len, &pri, &offset);
    }

    // Exercise syslog_pri with data-derived values
    if (size >= 2) {
        syslog_pri((syslog_facility_t)(data[0] % 24),
                   (syslog_severity_t)(data[1] % 8));
    }

    // Exercise syslog_format with data-derived strings
    {
        // Split fuzz input into multiple NUL-terminated field strings
        char hostname[64], app_name[64], proc_id[64], msg_id[64], message[256];
        memset(hostname, 0, sizeof(hostname));
        memset(app_name, 0, sizeof(app_name));
        memset(proc_id, 0, sizeof(proc_id));
        memset(msg_id, 0, sizeof(msg_id));
        memset(message, 0, sizeof(message));

        size_t off = 0;
        size_t chunk;

        chunk = (size - off) < 63 ? (size - off) : 63;
        if (chunk > 0) { memcpy(hostname, data + off, chunk); off += chunk; }

        chunk = (size - off) < 63 ? (size - off) : 63;
        if (chunk > 0) { memcpy(app_name, data + off, chunk); off += chunk; }

        chunk = (size - off) < 63 ? (size - off) : 63;
        if (chunk > 0) { memcpy(proc_id, data + off, chunk); off += chunk; }

        chunk = (size - off) < 63 ? (size - off) : 63;
        if (chunk > 0) { memcpy(msg_id, data + off, chunk); off += chunk; }

        chunk = (size - off) < 255 ? (size - off) : 255;
        if (chunk > 0) { memcpy(message, data + off, chunk); off += chunk; }

        syslog_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.facility = (syslog_facility_t)(size > 0 ? data[0] % 24 : 0);
        msg.severity = (syslog_severity_t)(size > 1 ? data[1] % 8 : 0);
        msg.hostname = hostname;
        msg.app_name = app_name;
        msg.proc_id = proc_id;
        msg.msg_id = msg_id;
        msg.message = message;
        msg.timestamp = (size >= 6)
            ? ((uint32_t)data[2] | ((uint32_t)data[3] << 8) | ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 24))
            : 0;

        char out_buf[1024];
        syslog_format(&msg, out_buf, sizeof(out_buf));

        // Also try with very small buffer
        char tiny[8];
        syslog_format(&msg, tiny, sizeof(tiny));
    }

    // Exercise format with NULL fields
    {
        syslog_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.facility = SYSLOG_FAC_USER;
        msg.severity = SYSLOG_SEV_INFO;

        char out_buf[256];
        syslog_format(&msg, out_buf, sizeof(out_buf));
    }

    return 0;
}
