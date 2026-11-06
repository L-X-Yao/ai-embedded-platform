// SPDX-License-Identifier: MIT
/*
 * can_gateway -- integration recipe: CAN bus message gateway
 *
 * Demonstrates four wheels working together:
 *   L1  mcp2515   -- SPI CAN bus controller
 *   L2  canbus    -- generic CAN frame pack / unpack
 *   L2  isotp     -- ISO 15765-2 multi-frame transport reassembly
 *   L2  j1939     -- SAE J1939 PGN / priority / source decode
 *
 * Scenario: a vehicle-bus gateway sits on the CAN side of the harness,
 * receives a mix of single-frame J1939 broadcasts and ISO-TP multi-
 * frame UDS-style requests via an MCP2515 SPI controller, then dispatches
 * each frame to the right decoder. Reassembled messages and decoded
 * J1939 PGNs are pushed to a mock uplink for transport-level forwarding.
 *
 * Runs entirely on host with mock SPI -- no hardware required.
 * Build:  cmake -B build -S . && cmake --build build && ./build/can_gateway
 */

#include "test_harness.h"

/* wheels */
#include "mcp2515.h"
#include "canbus.h"
#include "isotp.h"
#include "j1939.h"

#include <string.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define ISOTP_BUFFER_SIZE  256u   /* reassembly capacity */
#define UPLINK_CAP          16u   /* how many decoded items the uplink keeps */

/* J1939 PGNs the gateway forwards */
#define PGN_EEC1   0xF004u   /* Electronic Engine Controller 1 */
#define PGN_VEHICLE_SPEED 0xFEF1u
#define PGN_FUEL_ECON     0xFEF2u

/* UDS-style multi-frame request goes to ID 0x18DA10F1 (peer-to-peer) */
#define UDS_RX_ID    0x18DA10F1u

/* ======================================================================
 * Mock MCP2515 register file with RX-buffer queue
 *
 * We emulate the small subset of MCP2515 registers the driver touches.
 * Crucially, we keep an external queue of "incoming CAN frames" that
 * the mock pre-loads into RXB0 (one at a time) so the driver's
 * mcp2515_receive() returns them in order.
 * ====================================================================== */

#define MOCK_REG_MAX 256
static uint8_t mock_regs[MOCK_REG_MAX];

/* MCP2515 register offsets used by the driver (mirror of mcp2515.c) */
#define R_CANSTAT      0x0E
#define R_CANCTRL      0x0F
#define R_CANINTF      0x2C
#define R_CNF1         0x2A
#define R_CNF2         0x29
#define R_CNF3         0x28
#define R_TXB0CTRL     0x30
#define R_TXB0SIDH     0x31
#define R_TXB0SIDL     0x32
#define R_TXB0EID8     0x33
#define R_TXB0EID0     0x34
#define R_TXB0DLC      0x35
#define R_TXB0D0       0x36
#define R_RXB0SIDH     0x61
#define R_RXB0SIDL     0x62
#define R_RXB0EID8     0x63
#define R_RXB0EID0     0x64
#define R_RXB0DLC      0x65
#define R_RXB0D0       0x66
#define R_RXB1CTRL     0x70

/* SPI instructions handled by the mock */
#define SPI_RESET       0xC0
#define SPI_READ        0x03
#define SPI_WRITE       0x02
#define SPI_RTS         0x80
#define SPI_READ_STATUS 0xA0
#define SPI_BIT_MODIFY  0x05

/* Frame queue waiting to be delivered into RXB0 by the gateway */
typedef struct {
    uint32_t id;
    uint8_t  data[8];
    uint8_t  dlc;
    bool     extended;
} mock_frame_t;

#define MOCK_QUEUE_CAP 32
static mock_frame_t mock_queue[MOCK_QUEUE_CAP];
static uint8_t mock_queue_head;
static uint8_t mock_queue_tail;

static void mock_queue_reset(void)
{
    mock_queue_head = 0;
    mock_queue_tail = 0;
    memset(mock_queue, 0, sizeof(mock_queue));
}

static bool mock_queue_push(uint32_t id, const uint8_t *data, uint8_t dlc,
                            bool extended)
{
    uint8_t next = (uint8_t)((mock_queue_tail + 1) % MOCK_QUEUE_CAP);
    if (next == mock_queue_head) return false;  /* full */
    mock_queue[mock_queue_tail].id       = id;
    mock_queue[mock_queue_tail].dlc      = dlc;
    mock_queue[mock_queue_tail].extended = extended;
    memcpy(mock_queue[mock_queue_tail].data, data, dlc);
    mock_queue_tail = next;
    return true;
}

static bool mock_queue_pop(mock_frame_t *out)
{
    if (mock_queue_head == mock_queue_tail) return false;
    *out = mock_queue[mock_queue_head];
    mock_queue_head = (uint8_t)((mock_queue_head + 1) % MOCK_QUEUE_CAP);
    return true;
}

/* Load the next queued frame into the RXB0 mock registers and set RX0IF */
static void mock_load_next_rx(void)
{
    mock_frame_t f;
    if (!mock_queue_pop(&f)) return;

    if (f.extended) {
        uint32_t id = f.id;
        uint16_t sid = (uint16_t)(id >> 18);       /* SID[10:0] */
        uint32_t eid = id & 0x3FFFFu;              /* EID[17:0] */

        mock_regs[R_RXB0SIDH] = (uint8_t)(sid >> 3);
        mock_regs[R_RXB0SIDL] = (uint8_t)(((sid & 0x07u) << 5) |
                                          0x08u |    /* IDE = 1 */
                                          ((eid >> 16) & 0x03u));
        mock_regs[R_RXB0EID8] = (uint8_t)(eid >> 8);
        mock_regs[R_RXB0EID0] = (uint8_t)(eid);
    } else {
        mock_regs[R_RXB0SIDH] = (uint8_t)(f.id >> 3);
        mock_regs[R_RXB0SIDL] = (uint8_t)((f.id << 5) & 0xE0u);
        mock_regs[R_RXB0EID8] = 0;
        mock_regs[R_RXB0EID0] = 0;
    }

    mock_regs[R_RXB0DLC] = f.dlc;
    memcpy(&mock_regs[R_RXB0D0], f.data, f.dlc);

    mock_regs[R_CANINTF] |= 0x01u;  /* set RX0IF */
}

/* ---------- SPI HAL (drives the mock register file) --------------------- */

typedef struct {
    uint8_t  state;             /* 0=idle, 1=read, 2=write, 3=bit_modify */
    uint8_t  addr;              /* register address latched after op byte */
    uint8_t  byte_index;        /* word counter within the current op    */
    uint8_t  bm_mask;           /* mask byte for BIT_MODIFY               */
} mock_spi_state_t;

static mock_spi_state_t spi_st;

static void mock_cs_low(void *ctx)  { (void)ctx; spi_st.state = 0;
                                       spi_st.byte_index = 0; }
static void mock_cs_high(void *ctx) { (void)ctx; spi_st.state = 0; }

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    uint8_t rx = 0xFF;

    if (spi_st.state == 0) {
        /* command byte */
        switch (tx) {
        case SPI_RESET:
            spi_st.state = 0;
            /* On reset the chip enters config mode */
            mock_regs[R_CANSTAT] = 0x80u;
            mock_regs[R_CANCTRL] = 0x80u;
            return 0xFF;
        case SPI_READ:
            spi_st.state = 1;  /* next byte = addr */
            break;
        case SPI_WRITE:
            spi_st.state = 2;  /* next byte = addr */
            break;
        case SPI_BIT_MODIFY:
            spi_st.state = 3;  /* next: addr, mask, value */
            spi_st.byte_index = 0;
            break;
        case SPI_READ_STATUS:
            /* one-byte response: RX0IF in bit0, RX1IF in bit1 */
            rx = (mock_regs[R_CANINTF] & 0x03u);
            return rx;
        case SPI_RTS:
        default:
            /* RTS sets TX request bit on TXB0; ignore the rest */
            mock_regs[R_TXB0CTRL] &= (uint8_t)~0x08u;
            return 0xFF;
        }
    } else if (spi_st.state == 1) {
        /* address byte for READ */
        spi_st.addr = tx;
        spi_st.state = 4;  /* next: data return */
    } else if (spi_st.state == 4) {
        /* return register value */
        rx = mock_regs[spi_st.addr];
        spi_st.addr++;
    } else if (spi_st.state == 2) {
        /* address byte for WRITE */
        spi_st.addr = tx;
        spi_st.state = 5;
    } else if (spi_st.state == 5) {
        /* data byte for WRITE */
        mock_regs[spi_st.addr] = tx;
        /* If the driver writes CANCTRL to change mode, update CANSTAT */
        if (spi_st.addr == R_CANCTRL) {
            uint8_t opmod = tx & 0xE0u;
            mock_regs[R_CANSTAT] = (uint8_t)((mock_regs[R_CANSTAT] & 0x1Fu) |
                                              opmod);
        }
        spi_st.addr++;
    } else if (spi_st.state == 3) {
        /* BIT_MODIFY: byte 0 = addr, byte 1 = mask, byte 2 = value */
        if (spi_st.byte_index == 0) {
            spi_st.addr = tx;
        } else if (spi_st.byte_index == 1) {
            spi_st.bm_mask = tx;
        } else {
            uint8_t cur = mock_regs[spi_st.addr];
            mock_regs[spi_st.addr] = (uint8_t)((cur & ~spi_st.bm_mask) |
                                                (tx & spi_st.bm_mask));
            /* Same CANCTRL -> CANSTAT shadowing as above */
            if (spi_st.addr == R_CANCTRL) {
                uint8_t opmod = mock_regs[R_CANCTRL] & 0xE0u;
                mock_regs[R_CANSTAT] = (uint8_t)((mock_regs[R_CANSTAT] & 0x1Fu) |
                                                  opmod);
            }
            /* If CANINTF cleared the RX0IF flag, optionally pre-load next */
            if (spi_st.addr == R_CANINTF) {
                if (!(mock_regs[R_CANINTF] & 0x01u)) {
                    /* RX0IF cleared -- nothing to do until gateway polls */
                }
            }
        }
        spi_st.byte_index++;
    }

    return rx;
}

/* The mock chip has no oscillator to wait for */
static void mock_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static const mcp2515_spi_t spi_hal = {
    .cs_low   = mock_cs_low,
    .cs_high  = mock_cs_high,
    .xfer     = mock_xfer,
    .ctx      = NULL,
    .delay_us = mock_delay_us,
};

/* ======================================================================
 * Uplink mock -- captures whatever the gateway forwards
 * ====================================================================== */

typedef enum {
    UPLINK_J1939_PGN = 1,
    UPLINK_ISOTP_MSG = 2,
    UPLINK_RAW_FRAME = 3,
} uplink_kind_t;

typedef struct {
    uplink_kind_t kind;
    uint32_t      can_id;
    uint32_t      pgn;
    uint8_t       priority;
    uint8_t       source;
    uint8_t       payload[ISOTP_BUFFER_SIZE];
    uint16_t      payload_len;
} uplink_item_t;

static uplink_item_t uplink[UPLINK_CAP];
static uint8_t       uplink_count;

static void uplink_reset(void)
{
    memset(uplink, 0, sizeof(uplink));
    uplink_count = 0;
}

static void uplink_push_pgn(uint32_t can_id, const j1939_id_t *jid,
                            const uint8_t *data, uint8_t dlc)
{
    if (uplink_count >= UPLINK_CAP) return;
    uplink[uplink_count].kind        = UPLINK_J1939_PGN;
    uplink[uplink_count].can_id      = can_id;
    uplink[uplink_count].pgn         = jid->pgn;
    uplink[uplink_count].priority    = jid->priority;
    uplink[uplink_count].source      = jid->source;
    memcpy(uplink[uplink_count].payload, data, dlc);
    uplink[uplink_count].payload_len = dlc;
    uplink_count++;
}

static void uplink_push_isotp(uint32_t can_id,
                              const uint8_t *data, size_t len)
{
    if (uplink_count >= UPLINK_CAP) return;
    if (len > ISOTP_BUFFER_SIZE) len = ISOTP_BUFFER_SIZE;
    uplink[uplink_count].kind        = UPLINK_ISOTP_MSG;
    uplink[uplink_count].can_id      = can_id;
    memcpy(uplink[uplink_count].payload, data, len);
    uplink[uplink_count].payload_len = (uint16_t)len;
    uplink_count++;
}

/* ======================================================================
 * Globals
 * ====================================================================== */

static mcp2515_t   can;
static isotp_rx_t  uds_rx;
static uint8_t     uds_rx_buf[ISOTP_BUFFER_SIZE];

/* ======================================================================
 * Helper -- queue a J1939 / UDS frame for the mock controller
 * ====================================================================== */

static void enqueue_j1939_eec1(uint8_t source, uint16_t engine_rpm_x_125)
{
    /* Encode J1939 ID for PGN 0xF004 (EEC1), priority 3 */
    j1939_id_t jid = {
        .priority    = 3,
        .pgn         = PGN_EEC1,
        .source      = source,
        .destination = J1939_ADDR_GLOBAL,
        .pdu1        = false,
    };
    uint32_t can_id = 0;
    j1939_encode_id(&jid, &can_id);

    /* J1939 EEC1 byte layout per SAE J1939-71: engine speed in bytes 3-4
     * (Intel byte order, 0.125 rpm/bit). We pack via canbus_signal_pack. */
    uint8_t data[8] = {0};
    canbus_signal_pack(data, 24, 16, false, engine_rpm_x_125);

    mock_queue_push(can_id, data, 8, true);
}

static void enqueue_j1939_speed(uint8_t source, uint16_t speed_kph_x_256)
{
    j1939_id_t jid = {
        .priority    = 6,
        .pgn         = PGN_VEHICLE_SPEED,
        .source      = source,
        .destination = J1939_ADDR_GLOBAL,
        .pdu1        = false,
    };
    uint32_t can_id = 0;
    j1939_encode_id(&jid, &can_id);

    uint8_t data[8] = {0};
    canbus_signal_pack(data, 8, 16, false, speed_kph_x_256);

    mock_queue_push(can_id, data, 8, true);
}

static void enqueue_isotp_sf(const char *payload)
{
    size_t plen = strlen(payload);
    if (plen == 0 || plen > 7) return;

    uint8_t data[8];
    isotp_encode_sf((const uint8_t *)payload, plen, data);
    mock_queue_push(UDS_RX_ID, data, 8, true);
}

/* Multi-frame ISO-TP: First Frame + N Consecutive Frames */
static void enqueue_isotp_multi(const uint8_t *payload, size_t len)
{
    if (len < 8 || len > ISOTP_BUFFER_SIZE) return;

    /* First Frame: 0x1n LL .... .... .... .... .... .... */
    uint8_t ff[8];
    ff[0] = (uint8_t)(0x10u | ((len >> 8) & 0x0Fu));
    ff[1] = (uint8_t)(len & 0xFFu);
    memcpy(&ff[2], payload, 6);
    mock_queue_push(UDS_RX_ID, ff, 8, true);

    /* Consecutive frames */
    size_t off = 6;
    uint8_t sn = 1;
    while (off < len) {
        uint8_t cf[8];
        cf[0] = (uint8_t)(0x20u | (sn & 0x0Fu));
        size_t copy = (len - off) < 7 ? (len - off) : 7;
        memset(&cf[1], 0xCC, 7);  /* pad */
        memcpy(&cf[1], &payload[off], copy);
        mock_queue_push(UDS_RX_ID, cf, 8, true);
        off += copy;
        sn = (uint8_t)((sn + 1) & 0x0Fu);
    }
}

/* ======================================================================
 * The gateway's per-frame dispatch routine
 * ====================================================================== */

static void gateway_dispatch_one(const mcp2515_msg_t *msg)
{
    /* Build a canbus_frame_t view for downstream consumers */
    canbus_frame_t cb = {
        .id       = msg->id,
        .dlc      = msg->dlc,
        .extended = msg->extended,
        .rtr      = msg->rtr,
    };
    memcpy(cb.data, msg->data, msg->dlc);

    /* Validate before any routing */
    if (canbus_frame_validate(&cb) != CANBUS_OK) return;

    /* Routing decision: UDS_RX_ID is the ISO-TP endpoint; everything else
     * extended is J1939. */
    if (msg->extended && msg->id == UDS_RX_ID) {
        isotp_err_t err = isotp_rx_feed(&uds_rx, msg->data, msg->dlc);
        if (err == ISOTP_OK) {
            uplink_push_isotp(msg->id,
                              isotp_rx_data(&uds_rx),
                              isotp_rx_length(&uds_rx));
            isotp_rx_reset(&uds_rx);
        }
        /* ISOTP_INCOMPLETE just keeps reassembling */
    } else if (msg->extended) {
        j1939_id_t jid;
        if (j1939_decode_id(msg->id, &jid) == J1939_OK) {
            uplink_push_pgn(msg->id, &jid, msg->data, msg->dlc);
        }
    } else {
        /* 11-bit IDs: forward raw (gateway "fall-through" path) */
        /* Not used in this scenario but kept here as a routing slot */
        if (uplink_count < UPLINK_CAP) {
            uplink[uplink_count].kind        = UPLINK_RAW_FRAME;
            uplink[uplink_count].can_id      = msg->id;
            memcpy(uplink[uplink_count].payload, msg->data, msg->dlc);
            uplink[uplink_count].payload_len = msg->dlc;
            uplink_count++;
        }
    }
}

/* Drain the mock queue through mcp2515_receive() one frame at a time */
static uint32_t gateway_drain(void)
{
    uint32_t processed = 0;
    while (mock_queue_head != mock_queue_tail) {
        mock_load_next_rx();
        mcp2515_msg_t msg;
        mcp2515_err_t err = mcp2515_receive(&can, &msg);
        if (err != MCP2515_OK) break;
        gateway_dispatch_one(&msg);
        processed++;
    }
    return processed;
}

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_queue_reset();
    uplink_reset();
    memset(&spi_st, 0, sizeof(spi_st));

    ASSERT_EQ(mcp2515_init(&can, &spi_hal, MCP2515_SPEED_500KBPS), MCP2515_OK);

    /* ISO-TP reassembly buffer */
    ASSERT_EQ(isotp_rx_init(&uds_rx, uds_rx_buf, sizeof(uds_rx_buf)),
              ISOTP_OK);

    printf("    mcp2515 + isotp + canbus + j1939 ready\n");
}

TEST(test_j1939_decode_helpers)
{
    /* Confirm raw J1939 decode utilities work on a known ID */
    j1939_id_t jid = {
        .priority    = 6,
        .pgn         = PGN_VEHICLE_SPEED,
        .source      = 0x11,
        .destination = J1939_ADDR_GLOBAL,
        .pdu1        = false,
    };
    uint32_t can_id = 0;
    ASSERT_EQ(j1939_encode_id(&jid, &can_id), J1939_OK);
    ASSERT_TRUE(can_id > 0);

    j1939_id_t back;
    ASSERT_EQ(j1939_decode_id(can_id, &back), J1939_OK);
    ASSERT_EQ(back.priority,    jid.priority);
    ASSERT_EQ(back.pgn,         jid.pgn);
    ASSERT_EQ(back.source,      jid.source);
    ASSERT_FALSE(back.pdu1);

    /* Make sure j1939_pgn_from_id matches */
    uint32_t pgn = j1939_pgn_from_id(can_id);
    ASSERT_EQ(pgn, jid.pgn);

    printf("    j1939 codec round-trip: pgn=0x%04X src=0x%02X prio=%u\n",
           (unsigned)back.pgn, (unsigned)back.source, (unsigned)back.priority);
}

TEST(test_canbus_frame_pack_round_trip)
{
    /* canbus pack/unpack must round-trip a J1939 broadcast frame */
    canbus_frame_t in = {
        .id       = 0x18FEF111u,
        .dlc      = 8,
        .extended = true,
        .rtr      = false,
        .data     = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
    };
    uint8_t buf[CANBUS_PACK_MAX_SIZE];
    int n = canbus_frame_pack(&in, buf, sizeof(buf));
    ASSERT_GT(n, 0);

    canbus_frame_t out;
    ASSERT_EQ(canbus_frame_unpack(buf, (uint16_t)n, &out), CANBUS_OK);
    ASSERT_EQ(out.id,       in.id);
    ASSERT_EQ(out.dlc,      in.dlc);
    ASSERT_EQ(out.extended, in.extended);
    ASSERT_MEMEQ_N(out.data, in.data, in.dlc);
}

TEST(test_dispatch_j1939_broadcasts)
{
    /* Enqueue 5 J1939 single frames */
    enqueue_j1939_eec1(0x00, /*rpm raw=*/ 6400);   /* 800 rpm */
    enqueue_j1939_speed(0x00, /*kph raw=*/ 12800); /* 50 km/h */
    enqueue_j1939_eec1(0x00, /*rpm raw=*/ 8000);   /* 1000 rpm */
    enqueue_j1939_speed(0x00, /*kph raw=*/ 17280); /* 67.5 km/h */
    enqueue_j1939_eec1(0x00, /*rpm raw=*/ 9600);   /* 1200 rpm */

    /* Drain through the gateway */
    uint32_t processed = gateway_drain();
    ASSERT_EQ(processed, 5);

    /* Verify uplink captured exactly 5 PGN items, alternating */
    ASSERT_EQ(uplink_count, 5);
    for (uint8_t i = 0; i < 5; i++) {
        ASSERT_EQ(uplink[i].kind, UPLINK_J1939_PGN);
    }

    /* Two distinct PGNs interleaved */
    ASSERT_EQ(uplink[0].pgn, PGN_EEC1);
    ASSERT_EQ(uplink[1].pgn, PGN_VEHICLE_SPEED);
    ASSERT_EQ(uplink[2].pgn, PGN_EEC1);

    /* Decode the engine RPM from the first EEC1 frame */
    uint64_t rpm_raw = canbus_signal_extract(uplink[0].payload, 24, 16, false);
    ASSERT_EQ(rpm_raw, 6400);

    /* Decode the speed from the first speed frame */
    uint64_t spd_raw = canbus_signal_extract(uplink[1].payload, 8, 16, false);
    ASSERT_EQ(spd_raw, 12800);

    printf("    dispatched %u J1939 broadcasts (PGN 0xF004 + 0xFEF1 mix)\n",
           (unsigned)processed);
    printf("    engine_rpm=%u (raw %llu * 0.125), vehicle_speed=%u (raw %llu / 256 kph)\n",
           (unsigned)(rpm_raw / 8), (unsigned long long)rpm_raw,
           (unsigned)(spd_raw / 256), (unsigned long long)spd_raw);
}

TEST(test_dispatch_isotp_single_frame)
{
    uplink_reset();

    /* A 5-byte UDS positive response: 62 F1 90 41 42 */
    enqueue_isotp_sf("AB123");

    uint32_t processed = gateway_drain();
    ASSERT_EQ(processed, 1);

    ASSERT_EQ(uplink_count, 1);
    ASSERT_EQ(uplink[0].kind,        UPLINK_ISOTP_MSG);
    ASSERT_EQ(uplink[0].payload_len, 5);
    ASSERT_MEMEQ_N(uplink[0].payload, "AB123", 5);

    printf("    ISO-TP single-frame reassembled: \"%.*s\" (%u bytes)\n",
           (int)uplink[0].payload_len, (const char *)uplink[0].payload,
           (unsigned)uplink[0].payload_len);
}

TEST(test_dispatch_isotp_multi_frame)
{
    uplink_reset();
    isotp_rx_reset(&uds_rx);

    /* 25-byte UDS payload spanning FF + 3 CFs */
    uint8_t payload[25];
    for (uint8_t i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)(i + 1);

    enqueue_isotp_multi(payload, sizeof(payload));

    uint32_t processed = gateway_drain();
    /* 1 FF + 3 CF (25 bytes = 6 in FF + 7+7+5 in CF) = 4 frames */
    ASSERT_EQ(processed, 4);

    /* Exactly one reassembled uplink item */
    ASSERT_EQ(uplink_count, 1);
    ASSERT_EQ(uplink[0].kind,        UPLINK_ISOTP_MSG);
    ASSERT_EQ(uplink[0].payload_len, sizeof(payload));
    ASSERT_MEMEQ_N(uplink[0].payload, payload, sizeof(payload));

    printf("    ISO-TP multi-frame reassembled: %u bytes from %u frames\n",
           (unsigned)uplink[0].payload_len, (unsigned)processed);
}

TEST(test_mixed_traffic)
{
    uplink_reset();
    isotp_rx_reset(&uds_rx);

    /* Interleave broadcasts and a multi-frame: 2 J1939 + 4 ISO-TP + 1 J1939 */
    enqueue_j1939_eec1(0x10, 10000);   /* PGN_EEC1 broadcast */
    enqueue_j1939_speed(0x10, 5120);   /* PGN_VEHICLE_SPEED */

    uint8_t big_payload[15];
    for (uint8_t i = 0; i < sizeof(big_payload); i++) big_payload[i] = i + 0xA0;
    enqueue_isotp_multi(big_payload, sizeof(big_payload));

    enqueue_j1939_eec1(0x20, 12000);

    uint32_t processed = gateway_drain();
    /* 2 broadcast + FF + 2 CF (15 bytes: 6+7+2) + 1 broadcast = 6 */
    ASSERT_EQ(processed, 6);

    /* Expected uplink composition: 2 PGN, 1 ISOTP_MSG, 1 PGN */
    ASSERT_EQ(uplink_count, 4);
    ASSERT_EQ(uplink[0].kind, UPLINK_J1939_PGN);
    ASSERT_EQ(uplink[0].pgn,  PGN_EEC1);
    ASSERT_EQ(uplink[1].kind, UPLINK_J1939_PGN);
    ASSERT_EQ(uplink[1].pgn,  PGN_VEHICLE_SPEED);
    ASSERT_EQ(uplink[2].kind, UPLINK_ISOTP_MSG);
    ASSERT_EQ(uplink[2].payload_len, sizeof(big_payload));
    ASSERT_MEMEQ_N(uplink[2].payload, big_payload, sizeof(big_payload));
    ASSERT_EQ(uplink[3].kind, UPLINK_J1939_PGN);
    ASSERT_EQ(uplink[3].pgn,  PGN_EEC1);
    ASSERT_EQ(uplink[3].source, 0x20);

    printf("    mixed: 2x PGN -> [%u-byte UDS msg] -> 1x PGN (sources mix)\n",
           (unsigned)uplink[2].payload_len);
}

/* ======================================================================
 * Main
 * ====================================================================== */

TEST_MAIN("can_gateway integration")
    RUN(test_init);
    RUN(test_j1939_decode_helpers);
    RUN(test_canbus_frame_pack_round_trip);
    RUN(test_dispatch_j1939_broadcasts);
    RUN(test_dispatch_isotp_single_frame);
    RUN(test_dispatch_isotp_multi_frame);
    RUN(test_mixed_traffic);
TEST_MAIN_END()
