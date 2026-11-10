// SPDX-License-Identifier: MIT
/*
 * air780eg — Non-blocking AT modem driver for Air780EG (4G/GNSS)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      (none, transport injected)
 * Instances: multi
 */

#ifndef AIR780EG_H
#define AIR780EG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    AIR780EG_OK          =  0,
    AIR780EG_ERR_PARAM   = -1,
    AIR780EG_ERR_TIMEOUT = -2,
    AIR780EG_ERR_NACK    = -3,
    AIR780EG_BUSY        = -4,
} air780eg_err_t;

/* ---------- command status ---------- */

typedef enum {
    AIR780EG_CMD_IDLE = 0,
    AIR780EG_CMD_PENDING,
    AIR780EG_CMD_OK,
    AIR780EG_CMD_ERROR,
    AIR780EG_CMD_TIMEOUT,
} air780eg_cmd_status_t;

/* ---------- transport HAL injection ---------- */

typedef struct {
    void     (*send)(void *ctx, const uint8_t *data, uint16_t len);
    uint32_t (*get_tick_ms)(void *ctx);
    void *ctx;
} air780eg_io_t;

/* ---------- URC callback types ---------- */

typedef void (*air780eg_mqtt_rx_cb_t)(void *user, const char *topic,
                                      const uint8_t *payload, uint16_t len);
typedef void (*air780eg_event_cb_t)(void *user);
typedef void (*air780eg_nmea_cb_t)(void *user, const char *line);
typedef void (*air780eg_csq_cb_t)(void *user, int rssi);
typedef void (*air780eg_fail_cb_t)(void *user, const char *line);

/* ---------- configuration ---------- */

#define AIR780EG_LINE_MAX  512
/* A topic and its NUL: the module allows 256 bytes (manual p.243). */
#define AIR780EG_TOPIC_MAX 257
/* The longest message the module sends (p.243). */
#define AIR780EG_MSG_MAX   4100

/* Bytes kept for one incoming message, the largest part of air780eg_t. Define it
 * lower to save RAM: a longer message is then skipped and counted, never cut. */
#ifndef AIR780EG_PAYLOAD_MAX
#define AIR780EG_PAYLOAD_MAX AIR780EG_MSG_MAX
#endif

/* Silence inside a message for this long means its bytes were lost. */
#ifndef AIR780EG_MSG_STALL_MS
#define AIR780EG_MSG_STALL_MS 2000u
#endif

/* ---------- instance ---------- */

typedef struct {
    const air780eg_io_t *io;                    /* internal */
    /* line accumulator */
    char     line_buf[AIR780EG_LINE_MAX];       /* internal */
    uint16_t line_pos;                          /* internal */
    /* +MSUB message read by its length field */
    uint8_t  msg_payload[AIR780EG_PAYLOAD_MAX]; /* internal */
    volatile uint16_t msg_left;                 /* internal: bytes still to come */
    uint16_t msg_pos;                           /* internal */
    uint16_t msg_topic_off;                     /* internal: topic, in line_buf */
    bool     msg_skip;                          /* internal: count, do not deliver */
    volatile bool msg_abort;                    /* internal: poll gave it up */
    uint16_t msg_watch_left;                    /* internal: poll's last look */
    uint32_t msg_watch_ms;                      /* internal */
    /* command state */
    air780eg_cmd_status_t cmd_status;           /* internal */
    char     expect[64];                        /* internal: expected response token */
    uint32_t cmd_start_ms;                      /* internal */
    uint32_t cmd_timeout_ms;                    /* internal */
    char     matched_line[AIR780EG_LINE_MAX];   /* internal */
    char     prev_line[AIR780EG_LINE_MAX];      /* internal */
    /* callbacks */
    air780eg_mqtt_rx_cb_t mqtt_rx_cb;           /* internal */
    void *mqtt_rx_user;                         /* internal */
    air780eg_event_cb_t   disconnect_cb;        /* internal */
    void *disconnect_user;                      /* internal */
    air780eg_event_cb_t   pdp_deact_cb;         /* internal */
    void *pdp_deact_user;                       /* internal */
    air780eg_event_cb_t   restart_cb;           /* internal */
    void *restart_user;                         /* internal */
    air780eg_nmea_cb_t    nmea_cb;              /* internal */
    void *nmea_user;                            /* internal */
    air780eg_csq_cb_t     csq_cb;              /* internal */
    void *csq_user;                             /* internal */
    air780eg_fail_cb_t    fail_cb;              /* internal */
    void *fail_user;                            /* internal */
    /* diagnostics */
    uint32_t rdy_count;                         /* internal */
    uint32_t msg_drop_count;                    /* internal */
    int      last_csq;                          /* internal */
} air780eg_t;

/* ---------- API ---------- */

/** @thread_safe: no @isr_safe: no */
air780eg_err_t air780eg_init(air780eg_t *dev, const air780eg_io_t *io);

/** @thread_safe: no @isr_safe: yes (feed from ISR OK) — call from UART RX ISR or polling loop */
void air780eg_feed(air780eg_t *dev, uint8_t byte);

/** @thread_safe: no @isr_safe: no — call periodically to check timeouts */
void air780eg_poll(air780eg_t *dev);

/**
 * @thread_safe: no @isr_safe: no — send raw AT command, set expected response.
 *
 * expect NULL or "OK": the command ends on a line that is exactly OK.
 * Any other expect: the command ends on a line containing it, and a plain OK
 * leaves it pending, because for AT+MCONNECT, AT+MSUB and AT+MIPSTART the OK
 * only says the module took the command. Whatever expect is, ERROR, +CME ERROR,
 * +CMS ERROR or a line ending in FAIL ends the command with AIR780EG_CMD_ERROR.
 */
air780eg_err_t air780eg_send_cmd(air780eg_t *dev, const char *cmd,
                                 const char *expect, uint32_t timeout_ms);

/** @thread_safe: no @isr_safe: no */
air780eg_cmd_status_t air780eg_cmd_status(const air780eg_t *dev);

/** @thread_safe: no @isr_safe: no */
const char *air780eg_get_matched_line(const air780eg_t *dev);

/** @thread_safe: no @isr_safe: no */
const char *air780eg_get_prev_line(const air780eg_t *dev);

/* --- callback registration --- */

/**
 * @thread_safe: no @isr_safe: no
 *
 * A +MSUB message, in direct mode (AT+MQTTMSGSET=0) or from AT+MQTTMSGGET. It is
 * read by its <len> field, so it arrives whole and unchanged, line ends included,
 * or not at all: see air780eg_get_msg_drop_count. topic and payload are valid only
 * during the call. A message never ends a command, so send AT+MQTTMSGGET with
 * expect NULL: the callback gets the messages and the final OK ends the command.
 */
void air780eg_on_mqtt_rx(air780eg_t *dev, air780eg_mqtt_rx_cb_t cb, void *user);

/** @thread_safe: no @isr_safe: no */
void air780eg_on_disconnect(air780eg_t *dev, air780eg_event_cb_t cb, void *user);

/** @thread_safe: no @isr_safe: no */
void air780eg_on_pdp_deact(air780eg_t *dev, air780eg_event_cb_t cb, void *user);

/** @thread_safe: no @isr_safe: no */
void air780eg_on_restart(air780eg_t *dev, air780eg_event_cb_t cb, void *user);

/** @thread_safe: no @isr_safe: no */
void air780eg_on_nmea(air780eg_t *dev, air780eg_nmea_cb_t cb, void *user);

/** @thread_safe: no @isr_safe: no */
void air780eg_on_csq(air780eg_t *dev, air780eg_csq_cb_t cb, void *user);

/** @thread_safe: no @isr_safe: no — a line ending in FAIL with no command pending */
void air780eg_on_fail(air780eg_t *dev, air780eg_fail_cb_t cb, void *user);

/* --- diagnostics --- */

/** @thread_safe: no @isr_safe: no */
uint32_t air780eg_get_rdy_count(const air780eg_t *dev);

/** @thread_safe: no @isr_safe: no */
int air780eg_get_last_csq(const air780eg_t *dev);

/**
 * @thread_safe: no @isr_safe: no
 *
 * Messages not delivered: topic over 256 bytes, payload over AIR780EG_PAYLOAD_MAX,
 * a <len> over 4100, or bytes that stopped for AIR780EG_MSG_STALL_MS.
 */
uint32_t air780eg_get_msg_drop_count(const air780eg_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* AIR780EG_H */
