// SPDX-License-Identifier: MIT
/*
 * w5500 -- SPI Ethernet controller driver for Wiznet W5500
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "w5500.h"
#include "contract.h"
#include <string.h>

/* ---------- W5500 SPI frame format ----------
 *
 * Every SPI transaction: [offset_hi][offset_lo][control][data...]
 * Control byte: BSB[4:0] | RWB[1] | OM[1:0]
 *   BSB = Block Select Bits (register block)
 *   RWB = 0 for read, 1 for write
 *   OM  = 00 for variable-length data mode
 */

/* Control byte building */
#define CTRL_READ   0x00U
#define CTRL_WRITE  0x04U
/* OM = 00 (variable-length data mode) is the low 2 bits = 0 */

/* Block Select Bits (shifted to [7:3] position in control byte) */
#define BSB_COMMON         (0x00U << 3)  /* Common registers */
#define BSB_SOCK_REG(n)    ((uint8_t)(((n) * 4U + 1U) << 3))  /* Socket n registers */
#define BSB_SOCK_TX(n)     ((uint8_t)(((n) * 4U + 2U) << 3))  /* Socket n TX buffer */
#define BSB_SOCK_RX(n)     ((uint8_t)(((n) * 4U + 3U) << 3))  /* Socket n RX buffer */

/* ---------- Common register addresses ---------- */

#define REG_MR         0x0000U  /* Mode Register */
#define REG_GAR0       0x0001U  /* Gateway Address */
#define REG_SUBR0      0x0005U  /* Subnet Mask Address */
#define REG_SHAR0      0x0009U  /* Source Hardware Address (MAC) */
#define REG_SIPR0      0x000FU  /* Source IP Address */
#define REG_VERSIONR   0x0039U  /* Chip Version (always 0x04) */

/* Mode Register bits */
#define MR_RST         0x80U    /* Software Reset */

/* ---------- Socket register offsets (within each socket block) ---------- */

#define SN_MR          0x0000U  /* Socket n Mode */
#define SN_CR          0x0001U  /* Socket n Command */
#define SN_IR          0x0002U  /* Socket n Interrupt */
#define SN_SR          0x0003U  /* Socket n Status */
#define SN_PORT0       0x0004U  /* Socket n Source Port */
#define SN_PORT1       0x0005U
#define SN_TXBUF_SIZE  0x001FU  /* Socket n TX Buffer Size */
#define SN_RXBUF_SIZE  0x001EU  /* Socket n RX Buffer Size */
#define SN_TX_FSR0     0x0020U  /* Socket n TX Free Size */
#define SN_TX_FSR1     0x0021U
#define SN_TX_RD0      0x0022U  /* Socket n TX Read Pointer */
#define SN_TX_RD1      0x0023U
#define SN_TX_WR0      0x0024U  /* Socket n TX Write Pointer */
#define SN_TX_WR1      0x0025U
#define SN_RX_RSR0     0x0026U  /* Socket n RX Received Size */
#define SN_RX_RSR1     0x0027U
#define SN_RX_RD0      0x0028U  /* Socket n RX Read Pointer */
#define SN_RX_RD1      0x0029U

/* ---------- Socket commands (written to Sn_CR) ---------- */

#define CMD_OPEN       0x01U
#define CMD_LISTEN     0x02U
#define CMD_CONNECT    0x04U
#define CMD_DISCON     0x08U
#define CMD_CLOSE      0x10U
#define CMD_SEND       0x20U
#define CMD_RECV       0x40U

/* ---------- timing ---------- */

#ifndef W5500_RESET_DELAY_MS
#define W5500_RESET_DELAY_MS  2U
#endif

#ifndef W5500_CMD_TIMEOUT
#define W5500_CMD_TIMEOUT  50000U
#endif

/* ---------- helpers: SPI read/write ---------- */

static void spi_write(const w5500_spi_t *s, uint16_t addr,
                       uint8_t bsb, const uint8_t *data, uint16_t len)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, (uint8_t)(addr >> 8));
    s->xfer(s->ctx, (uint8_t)(addr & 0xFFU));
    s->xfer(s->ctx, (uint8_t)(bsb | CTRL_WRITE));
    for (uint16_t i = 0; i < len; i++) {
        s->xfer(s->ctx, data[i]);
    }
    s->cs_high(s->ctx);
}

static void spi_read(const w5500_spi_t *s, uint16_t addr,
                      uint8_t bsb, uint8_t *data, uint16_t len)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, (uint8_t)(addr >> 8));
    s->xfer(s->ctx, (uint8_t)(addr & 0xFFU));
    s->xfer(s->ctx, (uint8_t)(bsb | CTRL_READ));
    for (uint16_t i = 0; i < len; i++) {
        data[i] = s->xfer(s->ctx, 0x00U);
    }
    s->cs_high(s->ctx);
}

/* ---------- helpers: register-level access ---------- */

static void write_common_reg(const w5500_spi_t *s, uint16_t addr, uint8_t val)
{
    spi_write(s, addr, BSB_COMMON, &val, 1);
}

static uint8_t read_common_reg(const w5500_spi_t *s, uint16_t addr)
{
    uint8_t val;
    spi_read(s, addr, BSB_COMMON, &val, 1);
    return val;
}

static void write_common_regs(const w5500_spi_t *s, uint16_t addr,
                                const uint8_t *data, uint16_t len)
{
    spi_write(s, addr, BSB_COMMON, data, len);
}

static void write_sock_reg(const w5500_spi_t *s, uint8_t sock,
                            uint16_t addr, uint8_t val)
{
    spi_write(s, addr, BSB_SOCK_REG(sock), &val, 1);
}

static uint8_t read_sock_reg(const w5500_spi_t *s, uint8_t sock,
                              uint16_t addr)
{
    uint8_t val;
    spi_read(s, addr, BSB_SOCK_REG(sock), &val, 1);
    return val;
}

static uint16_t read_sock_reg16(const w5500_spi_t *s, uint8_t sock,
                                 uint16_t addr)
{
    uint8_t buf[2];
    spi_read(s, addr, BSB_SOCK_REG(sock), buf, 2);
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

/* ---------- helpers: wait for command completion ---------- */

static w5500_err_t wait_cmd_clear(const w5500_spi_t *s, uint8_t sock)
{
    /* Sn_CR clears to 0x00 when the command is accepted */
    uint32_t timeout = W5500_CMD_TIMEOUT;
    while (timeout--) {
        if (read_sock_reg(s, sock, SN_CR) == 0x00U) {
            return W5500_OK;
        }
    }
    return W5500_ERR_TIMEOUT;
}

/* ---------- API ---------- */

w5500_err_t w5500_init(w5500_t *dev, const w5500_spi_t *spi)
{
    if (!dev || !spi || !spi->cs_low || !spi->cs_high || !spi->xfer)
        return W5500_ERR_PARAM;

    dev->spi = spi;
    memset(dev->mac_addr, 0, 6);

    /* Software reset via MR register */
    write_common_reg(spi, REG_MR, MR_RST);

    if (spi->delay_ms) {
        spi->delay_ms(spi->ctx, W5500_RESET_DELAY_MS);
    }

    /* Verify chip version */
    uint8_t ver = read_common_reg(spi, REG_VERSIONR);
    if (ver != W5500_VERSION) {
        return W5500_ERR_SPI;
    }

    /* Configure default 2KB TX/RX buffer per socket */
    for (uint8_t i = 0; i < W5500_MAX_SOCK; i++) {
        write_sock_reg(spi, i, SN_TXBUF_SIZE, 2);  /* 2KB */
        write_sock_reg(spi, i, SN_RXBUF_SIZE, 2);  /* 2KB */
    }

    return W5500_OK;
}

w5500_err_t w5500_set_mac(w5500_t *dev, const uint8_t mac[6])
{
    if (!dev || !dev->spi) return W5500_ERR_PARAM;
    if (!mac)              return W5500_ERR_PARAM;

    memcpy(dev->mac_addr, mac, 6);
    write_common_regs(dev->spi, REG_SHAR0, mac, 6);
    return W5500_OK;
}

w5500_err_t w5500_set_ip(w5500_t *dev, const uint8_t ip[4],
                          const uint8_t subnet[4],
                          const uint8_t gateway[4])
{
    if (!dev || !dev->spi) return W5500_ERR_PARAM;
    if (!ip || !subnet || !gateway) return W5500_ERR_PARAM;

    write_common_regs(dev->spi, REG_GAR0, gateway, 4);
    write_common_regs(dev->spi, REG_SUBR0, subnet, 4);
    write_common_regs(dev->spi, REG_SIPR0, ip, 4);
    return W5500_OK;
}

w5500_err_t w5500_socket_open(w5500_t *dev, uint8_t sock,
                               uint8_t protocol, uint16_t port)
{
    if (!dev || !dev->spi)   return W5500_ERR_PARAM;
    if (sock >= W5500_MAX_SOCK) return W5500_ERR_SOCK;
    if (protocol != W5500_PROTO_TCP &&
        protocol != W5500_PROTO_UDP &&
        protocol != W5500_PROTO_MACRAW)
        return W5500_ERR_PARAM;
    if (protocol == W5500_PROTO_MACRAW && sock != 0)
        return W5500_ERR_SOCK;

    const w5500_spi_t *s = dev->spi;

    /* Close socket first (in case it's already open) */
    write_sock_reg(s, sock, SN_CR, CMD_CLOSE);
    w5500_err_t err = wait_cmd_clear(s, sock);
    if (err != W5500_OK) return err;

    /* Set protocol */
    write_sock_reg(s, sock, SN_MR, protocol);

    /* Set source port (big-endian) */
    write_sock_reg(s, sock, SN_PORT0, (uint8_t)(port >> 8));
    write_sock_reg(s, sock, SN_PORT1, (uint8_t)(port & 0xFFU));

    /* Issue OPEN command */
    write_sock_reg(s, sock, SN_CR, CMD_OPEN);
    err = wait_cmd_clear(s, sock);
    if (err != W5500_OK) return err;

    /* Verify socket opened correctly */
    uint8_t sr = read_sock_reg(s, sock, SN_SR);
    if (protocol == W5500_PROTO_TCP && sr != W5500_SOCK_INIT)
        return W5500_ERR_SOCK;
    if (protocol == W5500_PROTO_UDP && sr != W5500_SOCK_UDP)
        return W5500_ERR_SOCK;
    if (protocol == W5500_PROTO_MACRAW && sr != W5500_SOCK_MACRAW)
        return W5500_ERR_SOCK;

    return W5500_OK;
}

w5500_err_t w5500_socket_close(w5500_t *dev, uint8_t sock)
{
    if (!dev || !dev->spi)      return W5500_ERR_PARAM;
    if (sock >= W5500_MAX_SOCK) return W5500_ERR_SOCK;

    write_sock_reg(dev->spi, sock, SN_CR, CMD_CLOSE);
    return wait_cmd_clear(dev->spi, sock);
}

w5500_err_t w5500_socket_send(w5500_t *dev, uint8_t sock,
                               const uint8_t *data, uint16_t len)
{
    if (!dev || !dev->spi)      return W5500_ERR_PARAM;
    if (!data)                  return W5500_ERR_PARAM;
    if (sock >= W5500_MAX_SOCK) return W5500_ERR_SOCK;
    if (len == 0)               return W5500_ERR_SIZE;
    if (len > W5500_SOCK_BUF_SIZE) return W5500_ERR_SIZE;

    const w5500_spi_t *s = dev->spi;

    /* Check TX free size */
    uint16_t free_size = read_sock_reg16(s, sock, SN_TX_FSR0);
    if (free_size < len) return W5500_ERR_SIZE;

    /* Get TX write pointer */
    uint16_t tx_wr = read_sock_reg16(s, sock, SN_TX_WR0);

    /* Write data to socket TX buffer */
    spi_write(s, tx_wr, BSB_SOCK_TX(sock), data, len);

    /* Update TX write pointer */
    tx_wr = (uint16_t)(tx_wr + len);
    write_sock_reg(s, sock, SN_TX_WR0, (uint8_t)(tx_wr >> 8));
    write_sock_reg(s, sock, SN_TX_WR1, (uint8_t)(tx_wr & 0xFFU));

    /* Issue SEND command */
    write_sock_reg(s, sock, SN_CR, CMD_SEND);
    return wait_cmd_clear(s, sock);
}

w5500_err_t w5500_socket_recv(w5500_t *dev, uint8_t sock,
                               uint8_t *buf, uint16_t buf_len,
                               uint16_t *out_len)
{
    if (!dev || !dev->spi)      return W5500_ERR_PARAM;
    if (!buf || !out_len)       return W5500_ERR_PARAM;
    if (buf_len == 0)           return W5500_ERR_PARAM;
    if (sock >= W5500_MAX_SOCK) return W5500_ERR_SOCK;

    const w5500_spi_t *s = dev->spi;

    /* Check received data size */
    uint16_t rx_size = read_sock_reg16(s, sock, SN_RX_RSR0);
    if (rx_size == 0) return W5500_ERR_NO_DATA;

    /* Clamp to buffer size */
    uint16_t read_len = (rx_size > buf_len) ? buf_len : rx_size;

    /* Get RX read pointer */
    uint16_t rx_rd = read_sock_reg16(s, sock, SN_RX_RD0);

    /* Read data from socket RX buffer */
    spi_read(s, rx_rd, BSB_SOCK_RX(sock), buf, read_len);

    /* Update RX read pointer */
    rx_rd = (uint16_t)(rx_rd + read_len);
    write_sock_reg(s, sock, SN_RX_RD0, (uint8_t)(rx_rd >> 8));
    write_sock_reg(s, sock, SN_RX_RD1, (uint8_t)(rx_rd & 0xFFU));

    /* Issue RECV command to notify W5500 */
    write_sock_reg(s, sock, SN_CR, CMD_RECV);
    w5500_err_t err = wait_cmd_clear(s, sock);
    if (err != W5500_OK) return err;

    *out_len = read_len;
    return W5500_OK;
}

w5500_err_t w5500_socket_status(w5500_t *dev, uint8_t sock,
                                 uint8_t *status)
{
    if (!dev || !dev->spi)      return W5500_ERR_PARAM;
    if (!status)                return W5500_ERR_PARAM;
    if (sock >= W5500_MAX_SOCK) return W5500_ERR_SOCK;

    *status = read_sock_reg(dev->spi, sock, SN_SR);
    return W5500_OK;
}

uint8_t w5500_read_version(w5500_t *dev)
{
    if (!dev || !dev->spi) return 0;
    return read_common_reg(dev->spi, REG_VERSIONR);
}
