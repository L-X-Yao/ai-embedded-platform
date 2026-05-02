// SPDX-License-Identifier: MIT
/*
 * syslog_packet — RFC 5424 syslog message formatter
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "syslog_packet.h"
#include <string.h>

/* ---------- helpers --------------------------------------------------- */

static const char *nilval(const char *s)
{
    return (s && s[0] != '\0') ? s : "-";
}

/**
 * Minimal uint-to-decimal into buf[pos..cap-1].
 * Returns new pos, or cap+1 on overflow.
 */
static uint16_t put_u32(char *buf, uint16_t cap, uint16_t pos, uint32_t val)
{
    char tmp[11]; /* max 10 digits for uint32_t */
    int  len = 0;

    if (val == 0) {
        tmp[len++] = '0';
    } else {
        while (val > 0) {
            tmp[len++] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    /* reverse into buf */
    for (int i = len - 1; i >= 0; i--) {
        if (pos >= cap) return (uint16_t)(cap + 1);
        buf[pos++] = tmp[i];
    }
    return pos;
}

static uint16_t put_str(char *buf, uint16_t cap, uint16_t pos, const char *s)
{
    while (*s) {
        if (pos >= cap) return (uint16_t)(cap + 1);
        buf[pos++] = *s++;
    }
    return pos;
}

static uint16_t put_char(char *buf, uint16_t cap, uint16_t pos, char c)
{
    if (pos >= cap) return (uint16_t)(cap + 1);
    buf[pos++] = c;
    return pos;
}

/**
 * Format unix epoch into ISO 8601 subset used by RFC 5424:
 *   YYYY-MM-DDThh:mm:ssZ
 * Minimal implementation — no timezone offset, always UTC 'Z'.
 */
static uint16_t put_timestamp(char *buf, uint16_t cap, uint16_t pos, uint32_t epoch)
{
    /* Days in each month (non-leap). */
    static const uint16_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

    uint32_t secs  = epoch % 60;  epoch /= 60;
    uint32_t mins  = epoch % 60;  epoch /= 60;
    uint32_t hours = epoch % 24;  epoch /= 24;
    /* epoch is now days since 1970-01-01 */

    uint32_t year = 1970;
    for (;;) {
        uint32_t yd = 365;
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
            yd = 366;
        if (epoch < yd) break;
        epoch -= yd;
        year++;
    }

    int leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 1 : 0;
    uint32_t month = 0;
    for (month = 0; month < 12; month++) {
        uint32_t md = mdays[month];
        if (month == 1) md += (uint32_t)leap;
        if (epoch < md) break;
        epoch -= md;
    }
    uint32_t day = epoch + 1;
    month += 1;

    /* YYYY-MM-DDThh:mm:ssZ  (20 chars) */
    pos = put_u32(buf, cap, pos, year);
    pos = put_char(buf, cap, pos, '-');
    if (month < 10) pos = put_char(buf, cap, pos, '0');
    pos = put_u32(buf, cap, pos, month);
    pos = put_char(buf, cap, pos, '-');
    if (day < 10) pos = put_char(buf, cap, pos, '0');
    pos = put_u32(buf, cap, pos, day);
    pos = put_char(buf, cap, pos, 'T');
    if (hours < 10) pos = put_char(buf, cap, pos, '0');
    pos = put_u32(buf, cap, pos, hours);
    pos = put_char(buf, cap, pos, ':');
    if (mins < 10) pos = put_char(buf, cap, pos, '0');
    pos = put_u32(buf, cap, pos, mins);
    pos = put_char(buf, cap, pos, ':');
    if (secs < 10) pos = put_char(buf, cap, pos, '0');
    pos = put_u32(buf, cap, pos, secs);
    pos = put_char(buf, cap, pos, 'Z');

    return pos;
}

/* ---------- public API ------------------------------------------------ */

uint8_t syslog_pri(syslog_facility_t facility, syslog_severity_t severity)
{
    return (uint8_t)((uint8_t)facility * 8u + (uint8_t)severity);
}

syslog_err_t syslog_parse_pri(const char *buf, uint16_t len,
                               uint8_t *pri, uint16_t *offset)
{
    if (!buf || !pri || !offset)
        return SYSLOG_ERR_PARAM;
    if (len < 3)  /* minimum: "<N>" */
        return SYSLOG_ERR_PARAM;
    if (buf[0] != '<')
        return SYSLOG_ERR_PARAM;

    uint16_t val = 0;
    uint16_t i = 1;
    while (i < len && buf[i] != '>') {
        if (buf[i] < '0' || buf[i] > '9')
            return SYSLOG_ERR_PARAM;
        val = (uint16_t)(val * 10u + (uint16_t)(buf[i] - '0'));
        if (val > 191)  /* max PRI = 23*8+7 = 191 */
            return SYSLOG_ERR_PARAM;
        i++;
    }
    if (i >= len || buf[i] != '>')
        return SYSLOG_ERR_PARAM;
    if (i == 1)  /* empty: "<>" */
        return SYSLOG_ERR_PARAM;

    *pri = (uint8_t)val;
    *offset = (uint16_t)(i + 1);
    return SYSLOG_OK;
}

int syslog_format(const syslog_msg_t *msg, char *buf, uint16_t buf_size)
{
    if (!msg || !buf || buf_size == 0)
        return (int)SYSLOG_ERR_PARAM;
    if ((uint8_t)msg->facility > 23 || (uint8_t)msg->severity > 7)
        return (int)SYSLOG_ERR_PARAM;

    /* Reserve 1 byte for NUL terminator. */
    uint16_t cap = (uint16_t)(buf_size - 1);
    uint16_t pos = 0;

    /* <PRI>VERSION SP */
    uint8_t pri_val = syslog_pri(msg->facility, msg->severity);
    pos = put_char(buf, cap, pos, '<');
    pos = put_u32(buf, cap, pos, pri_val);
    pos = put_char(buf, cap, pos, '>');
    pos = put_char(buf, cap, pos, '1');  /* RFC 5424 VERSION = 1 */
    pos = put_char(buf, cap, pos, ' ');

    /* TIMESTAMP SP */
    if (msg->timestamp == 0) {
        pos = put_char(buf, cap, pos, '-');
    } else {
        pos = put_timestamp(buf, cap, pos, msg->timestamp);
    }
    pos = put_char(buf, cap, pos, ' ');

    /* HOSTNAME SP APP-NAME SP PROCID SP MSGID SP */
    pos = put_str(buf, cap, pos, nilval(msg->hostname));
    pos = put_char(buf, cap, pos, ' ');
    pos = put_str(buf, cap, pos, nilval(msg->app_name));
    pos = put_char(buf, cap, pos, ' ');
    pos = put_str(buf, cap, pos, nilval(msg->proc_id));
    pos = put_char(buf, cap, pos, ' ');
    pos = put_str(buf, cap, pos, nilval(msg->msg_id));

    /* STRUCTURED-DATA — always NILVALUE for this implementation */
    pos = put_char(buf, cap, pos, ' ');
    pos = put_char(buf, cap, pos, '-');

    /* MSG (BOM not used; UTF-8 assumed) */
    const char *message = msg->message;
    if (message && message[0] != '\0') {
        pos = put_char(buf, cap, pos, ' ');
        pos = put_str(buf, cap, pos, message);
    }

    /* Check overflow. pos > cap means we ran out of space. */
    if (pos > cap) {
        buf[0] = '\0';
        return (int)SYSLOG_ERR_OVERFLOW;
    }

    buf[pos] = '\0';
    return (int)pos;
}
