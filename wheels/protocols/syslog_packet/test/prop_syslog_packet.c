/*
 * prop_syslog_packet.c — property-based tests for syslog_packet
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_syslog_packet.c ../syslog_packet.c -o prop_syslog_packet
 */

#include "test_harness.h"
#include "property_test.h"
#include "../syslog_packet.h"
#include <string.h>

/* Property: format then parse PRI roundtrip */
PROPERTY(pri_roundtrip, 10000) {
    syslog_facility_t fac = (syslog_facility_t)ARBI_RANGE(seed, 0, 23);
    syslog_severity_t sev = (syslog_severity_t)ARBI_RANGE(seed, 0, 7);
    uint8_t expected_pri = syslog_pri(fac, sev);

    syslog_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.facility = fac;
    msg.severity = sev;
    msg.message = "test";

    char buf[512];
    int written = syslog_format(&msg, buf, sizeof(buf));
    PROP_ASSERT(written > 0);

    uint8_t parsed_pri = 0;
    uint16_t offset = 0;
    syslog_err_t err = syslog_parse_pri(buf, (uint16_t)written, &parsed_pri, &offset);
    PROP_ASSERT_EQ(err, SYSLOG_OK);
    PROP_ASSERT_EQ(parsed_pri, expected_pri);
    PROP_ASSERT(offset > 0);
}

/* Property: format is deterministic */
PROPERTY(format_determinism, 5000) {
    syslog_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.facility = SYSLOG_FAC_USER;
    msg.severity = (syslog_severity_t)ARBI_RANGE(seed, 0, 7);
    msg.hostname = "host";
    msg.app_name = "app";
    msg.message = "test message";
    msg.timestamp = ARBI_UINT32(seed);

    char buf1[512], buf2[512];
    int len1 = syslog_format(&msg, buf1, sizeof(buf1));
    int len2 = syslog_format(&msg, buf2, sizeof(buf2));
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: parse_pri of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    char garbage[256];
    size_t len = ARBI_SIZE(seed, 256);
    ARBI_FILL(seed, garbage, len);
    /* Ensure NUL termination for safety */
    if (len > 0) garbage[len - 1] = '\0';

    uint8_t pri = 0;
    uint16_t offset = 0;
    syslog_parse_pri(garbage, (uint16_t)len, &pri, &offset);
}

TEST_MAIN("syslog_packet properties") {
    RUN(pri_roundtrip);
    RUN(format_determinism);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
