// SPDX-License-Identifier: MIT
/*
 * hw_model_dht11.h -- DHT11 single-bus timing model
 *
 * Models the bus of the DHT11 Product Manual (Aosong) in microseconds. The pin
 * level comes from the time since the frame began, so a driver reads a bit only
 * if it samples at a moment when the line really shows it:
 *
 *   p.6  The sensor answers a low start signal longer than 18 ms, and nothing
 *        in the first second after power-up.
 *   p.6  After the host releases the bus: a wait, 80 us low, 80 us high.
 *   p.7  Each bit: 50 us low, then 26-28 us high for 0 or 70 us high for 1.
 *        After the 40 bits, 50 us low, then the pull-up takes the line high.
 *   p.6  A read gives the previous measurement. The sensor measures again once
 *        the frame has gone.
 *
 * The figure on p.6 leaves the wait after the release without a number, so
 * response_delay_us is the model's choice, and tests can change it. Time passes
 * only through dht11_model_delay_us().
 */

#ifndef HW_MODEL_DHT11_H
#define HW_MODEL_DHT11_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define DHT11_MODEL_POWER_UP_US      1000000u  /* p.6: "power after DHT11 1S" */
#define DHT11_MODEL_START_MIN_US     18000u    /* p.6: ">18ms" */
#define DHT11_MODEL_RESPONSE_US      80u       /* p.7: each half of the response */
#define DHT11_MODEL_BIT_LOW_US       50u
#define DHT11_MODEL_ONE_HIGH_US      70u
#define DHT11_MODEL_ZERO_HIGH_MAX_US 28u       /* p.7: 26 to 28 us */

typedef struct {
    /* the bus */
    uint32_t now_us;
    bool     host_output;
    bool     host_high;
    uint32_t host_low_since_us;
    bool     host_holding_low;
    bool     present;             /* a sensor is on the bus */

    /* the sensor */
    uint8_t  environment[4];      /* what it would measure now */
    uint8_t  recorded[5];         /* its last measurement, with parity */
    uint8_t  frame[5];            /* what the running frame sends */
    bool     frame_active;
    uint32_t frame_start_us;
    uint32_t power_left_us;
    uint32_t response_delay_us;
    uint32_t zero_high_us;

    /* diagnostics */
    bool     contention;          /* host drove high while the sensor drove low */
    int      frame_count;
} dht11_model_t;

/* p.5: the parity is the low 8 bits of the sum of the first four bytes. */
static void dht11_model_record(dht11_model_t *m)
{
    uint8_t sum = 0;
    for (int i = 0; i < 4; i++) {
        m->recorded[i] = m->environment[i];
        sum = (uint8_t)(sum + m->environment[i]);
    }
    m->recorded[4] = sum;
}

static void dht11_model_init(dht11_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->present = true;
    m->host_high = true;
    m->response_delay_us = 20u;
    m->zero_high_us = DHT11_MODEL_ZERO_HIGH_MAX_US;
    dht11_model_record(m);
}

/* Power-up: the first second answers nothing, then it has one measurement. */
__attribute__((unused))
static void dht11_model_power_up(dht11_model_t *m)
{
    m->power_left_us = DHT11_MODEL_POWER_UP_US;
    m->frame_active = false;
}

/* What the sensor sees from now on. p.6: the next read still gives the last. */
__attribute__((unused))
static void dht11_model_set_environment(dht11_model_t *m, uint8_t hum_i,
                                        uint8_t hum_d, uint8_t temp_i,
                                        uint8_t temp_d)
{
    m->environment[0] = hum_i;
    m->environment[1] = hum_d;
    m->environment[2] = temp_i;
    m->environment[3] = temp_d;
}

/* The same, as a measurement that has already happened. */
__attribute__((unused))
static void dht11_model_set_measured(dht11_model_t *m, uint8_t hum_i,
                                     uint8_t hum_d, uint8_t temp_i,
                                     uint8_t temp_d)
{
    dht11_model_set_environment(m, hum_i, hum_d, temp_i, temp_d);
    dht11_model_record(m);
}

static bool dht11_model_bit(const dht11_model_t *m, unsigned i)
{
    return ((m->frame[i / 8u] >> (7u - i % 8u)) & 1u) != 0;
}

/* Is the sensor pulling the line low, this long after its frame began? */
static bool dht11_model_sensor_low(const dht11_model_t *m, uint32_t elapsed_us)
{
    if (elapsed_us < DHT11_MODEL_RESPONSE_US)
        return true;
    elapsed_us -= DHT11_MODEL_RESPONSE_US;
    if (elapsed_us < DHT11_MODEL_RESPONSE_US)
        return false;
    elapsed_us -= DHT11_MODEL_RESPONSE_US;

    for (unsigned i = 0; i < 40u; i++) {
        uint32_t high_us = dht11_model_bit(m, i) ? DHT11_MODEL_ONE_HIGH_US
                                                 : m->zero_high_us;
        if (elapsed_us < DHT11_MODEL_BIT_LOW_US)
            return true;
        elapsed_us -= DHT11_MODEL_BIT_LOW_US;
        if (elapsed_us < high_us)
            return false;
        elapsed_us -= high_us;
    }
    return elapsed_us < DHT11_MODEL_BIT_LOW_US;   /* the end signal */
}

static uint32_t dht11_model_frame_us(const dht11_model_t *m)
{
    uint32_t us = 2u * DHT11_MODEL_RESPONSE_US + DHT11_MODEL_BIT_LOW_US;

    for (unsigned i = 0; i < 40u; i++)
        us += DHT11_MODEL_BIT_LOW_US + (dht11_model_bit(m, i)
                                        ? DHT11_MODEL_ONE_HIGH_US
                                        : m->zero_high_us);
    return us;
}

static bool dht11_model_sensor_driving_low(const dht11_model_t *m)
{
    if (!m->frame_active || m->now_us < m->frame_start_us)
        return false;
    return dht11_model_sensor_low(m, m->now_us - m->frame_start_us);
}

/* The bus: the pull-up holds it high unless someone pulls it low. */
static bool dht11_model_line(const dht11_model_t *m)
{
    if (m->host_output && !m->host_high)
        return false;
    return !dht11_model_sensor_driving_low(m);
}

/* The host stopped pulling low: a long enough pulse starts the frame. */
static void dht11_model_end_start_signal(dht11_model_t *m)
{
    uint32_t low_us = m->now_us - m->host_low_since_us;

    m->host_holding_low = false;
    if (!m->present || m->power_left_us != 0 || m->frame_active)
        return;
    if (low_us <= DHT11_MODEL_START_MIN_US)
        return;
    memcpy(m->frame, m->recorded, sizeof(m->frame));
    m->frame_active = true;
    m->frame_start_us = m->now_us + m->response_delay_us;
    m->frame_count++;
}

/* --- GPIO HAL callbacks --- */

__attribute__((unused))
static void dht11_model_set_output(void *ctx)
{
    dht11_model_t *m = (dht11_model_t *)ctx;
    m->host_output = true;
    if (!m->host_high && !m->host_holding_low) {
        m->host_holding_low = true;
        m->host_low_since_us = m->now_us;
    }
}

__attribute__((unused))
static void dht11_model_set_input(void *ctx)
{
    dht11_model_t *m = (dht11_model_t *)ctx;
    m->host_output = false;
    if (m->host_holding_low)
        dht11_model_end_start_signal(m);
}

__attribute__((unused))
static void dht11_model_write_pin(void *ctx, bool level)
{
    dht11_model_t *m = (dht11_model_t *)ctx;
    m->host_high = level;
    if (!m->host_output)
        return;
    if (!level && !m->host_holding_low) {
        m->host_holding_low = true;
        m->host_low_since_us = m->now_us;
    } else if (level && m->host_holding_low) {
        dht11_model_end_start_signal(m);
    }
}

__attribute__((unused))
static bool dht11_model_read_pin(void *ctx)
{
    return dht11_model_line((const dht11_model_t *)ctx);
}

/* The only clock the model has, stepped so that no edge is missed. */
__attribute__((unused))
static void dht11_model_delay_us(void *ctx, uint32_t us)
{
    dht11_model_t *m = (dht11_model_t *)ctx;

    for (uint32_t i = 0; i < us; i++) {
        m->now_us++;
        if (m->power_left_us != 0) {
            m->power_left_us--;
            if (m->power_left_us == 0)
                dht11_model_record(m);
        }
        if (m->host_output && m->host_high && dht11_model_sensor_driving_low(m))
            m->contention = true;
        if (m->frame_active && m->now_us >= m->frame_start_us &&
            m->now_us - m->frame_start_us >= dht11_model_frame_us(m)) {
            m->frame_active = false;
            dht11_model_record(m);   /* p.7: it measures again after the frame */
        }
    }
}

#endif /* HW_MODEL_DHT11_H */
