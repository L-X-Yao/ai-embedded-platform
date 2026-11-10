// SPDX-License-Identifier: MIT
/*
 * air780eg — Non-blocking AT modem driver for Air780EG (4G/GNSS)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      (none, transport injected)
 * Instances: multi
 */

#include "air780eg.h"
#include "contract.h"
#include <string.h>

/* ---------- internal helpers ---------- */

static bool starts_with(const char *line, const char *prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

/* The message read by length is complete, or there never was anything to read. */
static void msg_finish(air780eg_t *dev)
{
    if (dev->msg_skip) {
        dev->msg_drop_count++;
    } else if (dev->mqtt_rx_cb != NULL) {
        dev->mqtt_rx_cb(dev->mqtt_rx_user, dev->line_buf + dev->msg_topic_off,
                        dev->msg_payload, dev->msg_pos);
    }
    dev->line_pos = 0;
}

static void msg_byte(air780eg_t *dev, uint8_t byte)
{
    if (!dev->msg_skip) {
        dev->msg_payload[dev->msg_pos++] = byte;
    }
    dev->msg_left--;
    if (dev->msg_left == 0) {
        msg_finish(dev);
    }
}

/*
 * Called at each comma of a +MSUB line. The header ends at the comma after <len>:
 *
 *   +MSUB: "mqtt/topic",9 byte,SSSSddddd    direct mode, p.248
 *   +MSUB: mqtt/topic,9 byte,SSSSddddd      read from the cache, p.247
 *
 * The syntax line of p.243 has a bare <len>. What follows is <len> bytes of data
 * with no quotes around it, so it is read by count, not as a line: it can hold
 * anything, a line end included, and it can be longer than a line.
 */
static void msg_header(air780eg_t *dev)
{
    dev->line_buf[dev->line_pos] = '\0';

    char *topic = dev->line_buf + 6;
    while (*topic == ' ') topic++;
    char *topic_end;
    const char *p;
    if (*topic == '"') {
        topic++;
        topic_end = strstr(topic, "\",");
        if (topic_end == NULL) return;
        p = topic_end + 2;
    } else {
        topic_end = strchr(topic, ',');
        if (topic_end == NULL) return;
        p = topic_end + 1;
    }

    const char *digits = p;
    uint32_t len = 0;
    while (*p >= '0' && *p <= '9') {
        if (len <= AIR780EG_MSG_MAX) {
            len = len * 10u + (uint32_t)(*p - '0');
        }
        p++;
    }
    if (p == digits) return;
    if (starts_with(p, " byte")) p += 5;
    /* Only the comma just stored ends the header: a later one is in a payload. */
    if (p[0] != ',' || p[1] != '\0') return;

    /* No module sends this, so it is a garbled line and not a count to trust. */
    if (len > AIR780EG_MSG_MAX) {
        dev->msg_drop_count++;
        return;
    }

    size_t tlen = (size_t)(topic_end - topic);
    *topic_end = '\0';
    dev->msg_topic_off = (uint16_t)(topic - dev->line_buf);
    dev->msg_pos = 0;
    /* A shortened topic could match a different subscription, and a shortened
     * message is a wrong one: both are skipped whole and counted. */
    dev->msg_skip = tlen >= AIR780EG_TOPIC_MAX || len > AIR780EG_PAYLOAD_MAX;
    dev->msg_left = (uint16_t)len;
    if (len == 0) {
        msg_finish(dev);
    }
}

/* Lost bytes must not leave the parser waiting for a count that never comes.
 * The abort flag hands the decision to feed, which owns the parser state. */
static void msg_check_stall(air780eg_t *dev)
{
    uint16_t left = dev->msg_left;
    if (left == 0) {
        dev->msg_watch_left = 0;
        return;
    }
    uint32_t now = dev->io->get_tick_ms(dev->io->ctx);
    if (left != dev->msg_watch_left) {
        dev->msg_watch_left = left;
        dev->msg_watch_ms = now;
    } else if ((now - dev->msg_watch_ms) >= AIR780EG_MSG_STALL_MS) {
        dev->msg_watch_left = 0;
        dev->msg_abort = true;
    }
}

static void dispatch_urc(air780eg_t *dev, const char *line)
{
    /* Cache mode: +MSUB: <store_addr>, 0 to 3 (p.243). Messages themselves are
     * read in feed and never come here. */
    if (starts_with(line, "+MSUB:")) {
        const char *p = line + 6;
        while (*p == ' ') p++;
        if (p[0] >= '0' && p[0] <= '3' && p[1] == '\0' &&
            dev->mqtt_cached_cb != NULL) {
            dev->mqtt_cached_cb(dev->mqtt_cached_user, (uint8_t)(p[0] - '0'));
        }
        return;
    }

    /* TCP/MQTT disconnect (manual p.249) */
    if (starts_with(line, "CLOSED")) {
        if (dev->disconnect_cb != NULL) {
            dev->disconnect_cb(dev->disconnect_user);
        }
        return;
    }

    /* PDP context deactivation: +PDP DEACT (p.249), +PDP:DEACT (p.173),
     * +CGEV: NW PDN DEACT and +CGEV: ME PDN DEACT (p.137). */
    if ((starts_with(line, "+PDP") && strstr(line, "DEACT") != NULL) ||
        starts_with(line, "+CGEV: NW PDN DEACT") ||
        starts_with(line, "+CGEV: ME PDN DEACT")) {
        if (dev->pdp_deact_cb != NULL) {
            dev->pdp_deact_cb(dev->pdp_deact_user);
        }
        return;
    }

    /* Module restart (RDY URC) */
    if (strcmp(line, "RDY") == 0) {
        dev->rdy_count++;
        if (dev->restart_cb != NULL) {
            dev->restart_cb(dev->restart_user);
        }
        return;
    }

    /* NMEA sentence (from GNSS) */
    if (line[0] == '$' && (starts_with(line, "$GNGGA") ||
                           starts_with(line, "$GNRMC") ||
                           starts_with(line, "$GNGSA") ||
                           starts_with(line, "$GPGGA") ||
                           starts_with(line, "$GPRMC"))) {
        if (dev->nmea_cb != NULL) {
            dev->nmea_cb(dev->nmea_user, line);
        }
        return;
    }

    /* CSQ response: +CSQ: rssi,ber */
    if (starts_with(line, "+CSQ:")) {
        const char *p = line + 5;
        while (*p == ' ') p++;
        /* The reply to AT+CSQ=? is a list, with no number here (p.67). */
        if (*p < '0' || *p > '9') return;
        int rssi = 0;
        while (*p >= '0' && *p <= '9') {
            rssi = rssi * 10 + (*p - '0');
            if (rssi > 99) return;
            p++;
        }
        /* p.67: 0 to 31, or 99 for unknown */
        if (rssi > 31 && rssi != 99) return;
        dev->last_csq = rssi;
        if (dev->csq_cb != NULL) {
            dev->csq_cb(dev->csq_user, rssi);
        }
        return;
    }
}

static bool ends_with(const char *line, const char *suffix)
{
    size_t llen = strlen(line);
    size_t slen = strlen(suffix);
    return llen >= slen && strcmp(line + (llen - slen), suffix) == 0;
}

/* CONNECT FAIL and SEND FAIL (manual p.155, p.159, p.239), with or without the
 * link number in front, are reports like ERROR. */
static bool is_failure(const char *line)
{
    return strcmp(line, "ERROR") == 0 || starts_with(line, "+CME ERROR") ||
           starts_with(line, "+CMS ERROR") || ends_with(line, "FAIL");
}

/* While the caller waits for a later token such as CONNACK OK, a plain OK only
 * says the module took the command (p.240). */
static bool is_success(const air780eg_t *dev, const char *line)
{
    if (dev->expect[0] == '\0' || strcmp(dev->expect, "OK") == 0) {
        return strcmp(line, "OK") == 0;
    }
    return strstr(line, dev->expect) != NULL;
}

static void finish_cmd(air780eg_t *dev, const char *line,
                       air780eg_cmd_status_t status)
{
    strncpy(dev->matched_line, line, AIR780EG_LINE_MAX - 1);
    dev->matched_line[AIR780EG_LINE_MAX - 1] = '\0';
    dev->cmd_status = status;
}

static void process_line(air780eg_t *dev, const char *line)
{
    /* Skip empty lines */
    if (line[0] == '\0') return;

    /* Check if this line ends the pending command. Failures come first: the
     * token CONNECT is inside CONNECT FAIL. */
    if (dev->cmd_status == AIR780EG_CMD_PENDING) {
        if (is_failure(line)) {
            finish_cmd(dev, line, AIR780EG_CMD_ERROR);
            return;
        }
        if (is_success(dev, line)) {
            finish_cmd(dev, line, AIR780EG_CMD_OK);
            return;
        }
        /* Store as prev_line (for responses that come before "OK") */
        strncpy(dev->prev_line, line, AIR780EG_LINE_MAX - 1);
        dev->prev_line[AIR780EG_LINE_MAX - 1] = '\0';
    } else if (ends_with(line, "FAIL") && dev->fail_cb != NULL) {
        dev->fail_cb(dev->fail_user, line);
        return;
    }

    /* Dispatch as URC (unsolicited result code) */
    dispatch_urc(dev, line);
}

/* ---------- public API ---------- */

air780eg_err_t air780eg_init(air780eg_t *dev, const air780eg_io_t *io)
{
    REQUIRE(dev != NULL);
    REQUIRE(io != NULL);
    if (dev == NULL || io == NULL) {
        return AIR780EG_ERR_PARAM;
    }
    REQUIRE(io->send != NULL);
    REQUIRE(io->get_tick_ms != NULL);
    if (io->send == NULL || io->get_tick_ms == NULL) {
        return AIR780EG_ERR_PARAM;
    }
    memset(dev, 0, sizeof(*dev));
    dev->io = io;
    dev->cmd_status = AIR780EG_CMD_IDLE;
    dev->last_csq = -1;
    return AIR780EG_OK;
}

void air780eg_feed(air780eg_t *dev, uint8_t byte)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;

    if (dev->msg_abort) {
        dev->msg_abort = false;
        if (dev->msg_left > 0) {
            dev->msg_left = 0;
            dev->msg_drop_count++;
            dev->line_pos = 0;
        }
    }
    if (dev->msg_left > 0) {
        msg_byte(dev, byte);
        return;
    }

    if (byte == '\n') {
        /* Strip trailing CR if present */
        if (dev->line_pos > 0 && dev->line_buf[dev->line_pos - 1] == '\r') {
            dev->line_pos--;
        }
        dev->line_buf[dev->line_pos] = '\0';
        process_line(dev, dev->line_buf);
        dev->line_pos = 0;
    } else if (byte == '\r') {
        /* Ignore standalone CR (wait for LF) */
    } else {
        if (dev->line_pos < AIR780EG_LINE_MAX - 1) {
            dev->line_buf[dev->line_pos++] = (char)byte;
            if (byte == ',' && memcmp(dev->line_buf, "+MSUB:", 6) == 0) {
                msg_header(dev);
            }
        }
    }
}

void air780eg_poll(air780eg_t *dev)
{
    REQUIRE(dev != NULL);
    if (dev == NULL || dev->io == NULL) return;

    if (dev->cmd_status == AIR780EG_CMD_PENDING) {
        uint32_t now = dev->io->get_tick_ms(dev->io->ctx);
        if ((now - dev->cmd_start_ms) >= dev->cmd_timeout_ms) {
            dev->cmd_status = AIR780EG_CMD_TIMEOUT;
        }
    }
    msg_check_stall(dev);
}

air780eg_err_t air780eg_send_cmd(air780eg_t *dev, const char *cmd,
                                 const char *expect, uint32_t timeout_ms)
{
    REQUIRE(dev != NULL);
    REQUIRE(cmd != NULL);
    if (dev == NULL || dev->io == NULL || cmd == NULL) {
        return AIR780EG_ERR_PARAM;
    }
    if (dev->cmd_status == AIR780EG_CMD_PENDING) {
        return AIR780EG_BUSY;
    }

    /* Set expectation */
    if (expect != NULL) {
        strncpy(dev->expect, expect, sizeof(dev->expect) - 1);
        dev->expect[sizeof(dev->expect) - 1] = '\0';
    } else {
        dev->expect[0] = '\0';
    }

    /* Record start time + timeout (unsigned subtraction in poll is wrap-safe) */
    dev->cmd_start_ms = dev->io->get_tick_ms(dev->io->ctx);
    dev->cmd_timeout_ms = timeout_ms;
    dev->cmd_status = AIR780EG_CMD_PENDING;
    dev->matched_line[0] = '\0';
    dev->prev_line[0] = '\0';

    /* Send command + CRLF */
    size_t cmd_len = strlen(cmd);
    uint16_t len = (cmd_len > UINT16_MAX) ? UINT16_MAX : (uint16_t)cmd_len;
    dev->io->send(dev->io->ctx, (const uint8_t *)cmd, len);
    dev->io->send(dev->io->ctx, (const uint8_t *)"\r\n", 2);

    return AIR780EG_OK;
}

air780eg_cmd_status_t air780eg_cmd_status(const air780eg_t *dev)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return AIR780EG_CMD_IDLE;
    return dev->cmd_status;
}

const char *air780eg_get_matched_line(const air780eg_t *dev)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return "";
    return dev->matched_line;
}

const char *air780eg_get_prev_line(const air780eg_t *dev)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return "";
    return dev->prev_line;
}

/* --- callback registration --- */

void air780eg_on_mqtt_rx(air780eg_t *dev, air780eg_mqtt_rx_cb_t cb, void *user)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;
    dev->mqtt_rx_cb = cb;
    dev->mqtt_rx_user = user;
}

void air780eg_on_mqtt_cached(air780eg_t *dev, air780eg_mqtt_cached_cb_t cb,
                             void *user)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;
    dev->mqtt_cached_cb = cb;
    dev->mqtt_cached_user = user;
}

void air780eg_on_disconnect(air780eg_t *dev, air780eg_event_cb_t cb, void *user)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;
    dev->disconnect_cb = cb;
    dev->disconnect_user = user;
}

void air780eg_on_pdp_deact(air780eg_t *dev, air780eg_event_cb_t cb, void *user)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;
    dev->pdp_deact_cb = cb;
    dev->pdp_deact_user = user;
}

void air780eg_on_restart(air780eg_t *dev, air780eg_event_cb_t cb, void *user)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;
    dev->restart_cb = cb;
    dev->restart_user = user;
}

void air780eg_on_nmea(air780eg_t *dev, air780eg_nmea_cb_t cb, void *user)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;
    dev->nmea_cb = cb;
    dev->nmea_user = user;
}

void air780eg_on_csq(air780eg_t *dev, air780eg_csq_cb_t cb, void *user)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;
    dev->csq_cb = cb;
    dev->csq_user = user;
}

void air780eg_on_fail(air780eg_t *dev, air780eg_fail_cb_t cb, void *user)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return;
    dev->fail_cb = cb;
    dev->fail_user = user;
}

/* --- diagnostics --- */

uint32_t air780eg_get_rdy_count(const air780eg_t *dev)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return 0;
    return dev->rdy_count;
}

uint32_t air780eg_get_msg_drop_count(const air780eg_t *dev)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return 0;
    return dev->msg_drop_count;
}

int air780eg_get_last_csq(const air780eg_t *dev)
{
    REQUIRE(dev != NULL);
    if (dev == NULL) return -1;
    return dev->last_csq;
}
