// SPDX-License-Identifier: MIT
/*
 * syslog_packet — RFC 5424 syslog message formatter
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Stateless formatter for RFC 5424 syslog messages.
 * Produces "<PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID MSG".
 * NILVALUE "-" for missing fields. Formatter only — no transport.
 * Pure C99, no heap, zero-copy.
 * Thread safety: all functions are pure / reentrant (no global state).
 */

#ifndef SYSLOG_PACKET_H
#define SYSLOG_PACKET_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYSLOG_OK           =  0,
    SYSLOG_ERR_PARAM   = -1,
    SYSLOG_ERR_OVERFLOW = -2,
} syslog_err_t;

typedef enum {
    SYSLOG_SEV_EMERG   = 0,
    SYSLOG_SEV_ALERT   = 1,
    SYSLOG_SEV_CRIT    = 2,
    SYSLOG_SEV_ERR     = 3,
    SYSLOG_SEV_WARNING = 4,
    SYSLOG_SEV_NOTICE  = 5,
    SYSLOG_SEV_INFO    = 6,
    SYSLOG_SEV_DEBUG   = 7,
} syslog_severity_t;

typedef enum {
    SYSLOG_FAC_KERN   = 0,
    SYSLOG_FAC_USER   = 1,
    SYSLOG_FAC_MAIL   = 2,
    SYSLOG_FAC_DAEMON = 3,
    SYSLOG_FAC_AUTH   = 4,
    SYSLOG_FAC_LOCAL0 = 16,
    SYSLOG_FAC_LOCAL1 = 17,
    SYSLOG_FAC_LOCAL2 = 18,
    SYSLOG_FAC_LOCAL3 = 19,
    SYSLOG_FAC_LOCAL4 = 20,
    SYSLOG_FAC_LOCAL5 = 21,
    SYSLOG_FAC_LOCAL6 = 22,
    SYSLOG_FAC_LOCAL7 = 23,
} syslog_facility_t;

typedef struct {
    syslog_facility_t facility;
    syslog_severity_t severity;
    const char *hostname;       /**< NULL or "" => NILVALUE "-" */
    const char *app_name;       /**< NULL or "" => NILVALUE "-" */
    const char *proc_id;        /**< NULL or "" => NILVALUE "-" */
    const char *msg_id;         /**< NULL or "" => NILVALUE "-" */
    const char *message;        /**< NULL or "" => omitted */
    uint32_t    timestamp;      /**< unix epoch seconds; 0 => NILVALUE "-" */
} syslog_msg_t;

/**
 * Format RFC 5424 syslog message into buffer.
 * Returns bytes written (excluding NUL terminator), or negative syslog_err_t.
 * Output is always NUL-terminated if buf_size >= 1.
 * @thread_safe: yes (pure function)
 */
int syslog_format(const syslog_msg_t *msg, char *buf, uint16_t buf_size);

/**
 * Compute PRI value: facility * 8 + severity.
 * @thread_safe: yes (pure function)
 */
uint8_t syslog_pri(syslog_facility_t facility, syslog_severity_t severity);

/**
 * Parse PRI from "<NNN>" prefix.
 * On success, *pri holds the PRI value and *offset points past '>'.
 * @thread_safe: yes (pure function)
 */
syslog_err_t syslog_parse_pri(const char *buf, uint16_t len,
                               uint8_t *pri, uint16_t *offset);

#ifdef __cplusplus
}
#endif

#endif /* SYSLOG_PACKET_H */
