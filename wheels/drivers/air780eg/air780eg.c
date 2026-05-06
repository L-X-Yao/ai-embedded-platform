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
#include <string.h>

/* ---------- internal helpers ---------- */

static bool starts_with(const char *line, const char *prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

static void dispatch_urc(air780eg_t *dev, const char *line)
{
    /* MQTT subscription message: +MSUB: "topic",payload_len,"payload" */
    if (starts_with(line, "+MSUB:") && dev->mqtt_rx_cb != NULL) {
        const char *p = line + 6;
        while (*p == ' ') p++;
        if (*p != '"') return;
        p++;
        const char *topic_end = strchr(p, '"');
        if (topic_end == NULL) return;
        char topic[AIR780EG_TOPIC_MAX];
        size_t tlen = (size_t)(topic_end - p);
        if (tlen >= sizeof(topic)) tlen = sizeof(topic) - 1;
        memcpy(topic, p, tlen);
        topic[tlen] = '\0';

        /* Skip to payload: after second comma, find quoted payload */
        p = topic_end + 1;
        const char *comma = strchr(p, ',');
        if (comma == NULL) return;
        p = comma + 1;
        comma = strchr(p, ',');
        if (comma == NULL) return;
        p = comma + 1;
        /* Payload may or may not be quoted */
        uint16_t plen;
        const uint8_t *payload;
        if (*p == '"') {
            p++;
            const char *end = strrchr(p, '"');
            if (end == NULL) end = p + strlen(p);
            plen = (uint16_t)(end - p);
            payload = (const uint8_t *)p;
        } else {
            plen = (uint16_t)strlen(p);
            payload = (const uint8_t *)p;
        }
        dev->mqtt_rx_cb(dev->mqtt_rx_user, topic, payload, plen);
        return;
    }

    /* TCP/MQTT disconnect */
    if (starts_with(line, "+IPCLOSE:") || starts_with(line, "CLOSED")) {
        if (dev->disconnect_cb != NULL) {
            dev->disconnect_cb(dev->disconnect_user);
        }
        return;
    }

    /* PDP context deactivation */
    if (strstr(line, "PDP: DEACT") != NULL ||
        starts_with(line, "+CGEV: NW DEACT")) {
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
        int rssi = 0;
        while (*p >= '0' && *p <= '9') {
            rssi = rssi * 10 + (*p - '0');
            p++;
        }
        dev->last_csq = rssi;
        if (dev->csq_cb != NULL) {
            dev->csq_cb(dev->csq_user, rssi);
        }
        return;
    }
}

static void process_line(air780eg_t *dev, const char *line)
{
    /* Skip empty lines */
    if (line[0] == '\0') return;

    /* Check if this line matches expected response for pending command */
    if (dev->cmd_status == AIR780EG_CMD_PENDING) {
        if (strcmp(line, "OK") == 0 ||
            (dev->expect[0] != '\0' && strstr(line, dev->expect) != NULL)) {
            strncpy(dev->matched_line, line, AIR780EG_LINE_MAX - 1);
            dev->matched_line[AIR780EG_LINE_MAX - 1] = '\0';
            dev->cmd_status = AIR780EG_CMD_OK;
            return;
        }
        if (strcmp(line, "ERROR") == 0 || starts_with(line, "+CME ERROR") ||
            starts_with(line, "+CMS ERROR")) {
            strncpy(dev->matched_line, line, AIR780EG_LINE_MAX - 1);
            dev->matched_line[AIR780EG_LINE_MAX - 1] = '\0';
            dev->cmd_status = AIR780EG_CMD_ERROR;
            return;
        }
        /* Store as prev_line (for responses that come before "OK") */
        strncpy(dev->prev_line, line, AIR780EG_LINE_MAX - 1);
        dev->prev_line[AIR780EG_LINE_MAX - 1] = '\0';
    }

    /* Dispatch as URC (unsolicited result code) */
    dispatch_urc(dev, line);
}

/* ---------- public API ---------- */

air780eg_err_t air780eg_init(air780eg_t *dev, const air780eg_io_t *io)
{
    if (dev == NULL || io == NULL) {
        return AIR780EG_ERR_PARAM;
    }
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
    if (dev == NULL) return;

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
        }
    }
}

void air780eg_poll(air780eg_t *dev)
{
    if (dev == NULL || dev->io == NULL) return;

    if (dev->cmd_status == AIR780EG_CMD_PENDING) {
        uint32_t now = dev->io->get_tick_ms(dev->io->ctx);
        if (now >= dev->cmd_deadline_ms) {
            dev->cmd_status = AIR780EG_CMD_TIMEOUT;
        }
    }
}

air780eg_err_t air780eg_send_cmd(air780eg_t *dev, const char *cmd,
                                 const char *expect, uint32_t timeout_ms)
{
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

    /* Set deadline */
    uint32_t now = dev->io->get_tick_ms(dev->io->ctx);
    dev->cmd_deadline_ms = now + timeout_ms;
    dev->cmd_status = AIR780EG_CMD_PENDING;
    dev->matched_line[0] = '\0';
    dev->prev_line[0] = '\0';

    /* Send command + CRLF */
    uint16_t len = (uint16_t)strlen(cmd);
    dev->io->send(dev->io->ctx, (const uint8_t *)cmd, len);
    dev->io->send(dev->io->ctx, (const uint8_t *)"\r\n", 2);

    return AIR780EG_OK;
}

air780eg_cmd_status_t air780eg_cmd_status(const air780eg_t *dev)
{
    if (dev == NULL) return AIR780EG_CMD_IDLE;
    return dev->cmd_status;
}

const char *air780eg_get_matched_line(const air780eg_t *dev)
{
    if (dev == NULL) return "";
    return dev->matched_line;
}

const char *air780eg_get_prev_line(const air780eg_t *dev)
{
    if (dev == NULL) return "";
    return dev->prev_line;
}

/* --- callback registration --- */

void air780eg_on_mqtt_rx(air780eg_t *dev, air780eg_mqtt_rx_cb_t cb, void *user)
{
    if (dev == NULL) return;
    dev->mqtt_rx_cb = cb;
    dev->mqtt_rx_user = user;
}

void air780eg_on_disconnect(air780eg_t *dev, air780eg_event_cb_t cb, void *user)
{
    if (dev == NULL) return;
    dev->disconnect_cb = cb;
    dev->disconnect_user = user;
}

void air780eg_on_pdp_deact(air780eg_t *dev, air780eg_event_cb_t cb, void *user)
{
    if (dev == NULL) return;
    dev->pdp_deact_cb = cb;
    dev->pdp_deact_user = user;
}

void air780eg_on_restart(air780eg_t *dev, air780eg_event_cb_t cb, void *user)
{
    if (dev == NULL) return;
    dev->restart_cb = cb;
    dev->restart_user = user;
}

void air780eg_on_nmea(air780eg_t *dev, air780eg_nmea_cb_t cb, void *user)
{
    if (dev == NULL) return;
    dev->nmea_cb = cb;
    dev->nmea_user = user;
}

void air780eg_on_csq(air780eg_t *dev, air780eg_csq_cb_t cb, void *user)
{
    if (dev == NULL) return;
    dev->csq_cb = cb;
    dev->csq_user = user;
}

/* --- diagnostics --- */

uint32_t air780eg_get_rdy_count(const air780eg_t *dev)
{
    if (dev == NULL) return 0;
    return dev->rdy_count;
}

int air780eg_get_last_csq(const air780eg_t *dev)
{
    if (dev == NULL) return -1;
    return dev->last_csq;
}
