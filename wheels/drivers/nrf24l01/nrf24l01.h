/*
 * nrf24l01 — TODO: one-line description
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    spi_hal
 */

#ifndef NRF24L01_H
#define NRF24L01_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NRF24L01_OK          =  0,
    NRF24L01_ERR_SPI     = -1,
    NRF24L01_ERR_ID      = -2,
    NRF24L01_ERR_TIMEOUT = -3,
} nrf24l01_err_t;

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    *ctx;
} nrf24l01_spi_t;

/** Initialize and verify chip identity. @thread_safe: caller_must_lock */
nrf24l01_err_t nrf24l01_init(const nrf24l01_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* NRF24L01_H */
