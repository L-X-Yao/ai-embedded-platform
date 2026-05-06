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

/* ---------- configuration ---------- */

#define AIR780EG_LINE_MAX  512
#define AIR780EG_TOPIC_MAX 128

/* ---------- instance ---------- */

typedef struct {
    const air780eg_io_t *io;                    /* internal */
    /* line accumulator */
    char     line_buf[AIR780EG_LINE_MAX];       /* internal */
    uint16_t line_pos;                          /* internal */
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
    /* diagnostics */
    uint32_t rdy_count;                         /* internal */
    int      last_csq;                          /* internal */
} air780eg_t;

/* ---------- API ---------- */

/** @thread_safe: no @isr_safe: no */
air780eg_err_t air780eg_init(air780eg_t *dev, const air780eg_io_t *io);

/** @thread_safe: no @isr_safe: yes (feed from ISR OK) — call from UART RX ISR or polling loop */
void air780eg_feed(air780eg_t *dev, uint8_t byte);

/** @thread_safe: no @isr_safe: no — call periodically to check timeouts */
void air780eg_poll(air780eg_t *dev);

/** @thread_safe: no @isr_safe: no — send raw AT command, set expected response */
air780eg_err_t air780eg_send_cmd(air780eg_t *dev, const char *cmd,
                                 const char *expect, uint32_t timeout_ms);

/** @thread_safe: no @isr_safe: no */
air780eg_cmd_status_t air780eg_cmd_status(const air780eg_t *dev);

/** @thread_safe: no @isr_safe: no */
const char *air780eg_get_matched_line(const air780eg_t *dev);

/** @thread_safe: no @isr_safe: no */
const char *air780eg_get_prev_line(const air780eg_t *dev);

/* --- callback registration --- */

/** @thread_safe: no @isr_safe: no */
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

/* --- diagnostics --- */

/** @thread_safe: no @isr_safe: no */
uint32_t air780eg_get_rdy_count(const air780eg_t *dev);

/** @thread_safe: no @isr_safe: no */
int air780eg_get_last_csq(const air780eg_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* AIR780EG_H */
