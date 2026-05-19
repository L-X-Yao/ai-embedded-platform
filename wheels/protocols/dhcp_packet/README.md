# dhcp_packet

**Status: candidate**
**Layer: L2 (protocols)**

Stateless DHCP message builder and parser per RFC 2131. Handles
DISCOVER/OFFER/REQUEST/ACK message types, option iteration, and common
option extraction (subnet, router, DNS, lease time). All buffers are
caller-provided. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). d


## Why self-written?

existing open-source has DHCP integrated into its network stack. This wheel provides a
standalone packet codec for bare-metal Ethernet stacks (e.g. W5500, ENC28J60)
that do not use VSF's full TCP/IP.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "dhcp_packet.h"

int      dhcp_packet_parse(const uint8_t *buf, size_t len, dhcp_header_t *hdr);
int      dhcp_packet_next_option(const uint8_t *buf, size_t len,
                                 size_t *offset, dhcp_option_t *opt);
int      dhcp_packet_get_option(const uint8_t *buf, size_t len,
                                uint8_t code, dhcp_option_t *opt);
int      dhcp_packet_build_discover(uint8_t *buf, size_t cap,
                                    uint32_t xid, const uint8_t mac[6]);
int      dhcp_packet_build_request(uint8_t *buf, size_t cap,
                                   uint32_t xid, const uint8_t mac[6],
                                   const uint8_t req_ip[4],
                                   const uint8_t server_ip[4]);
uint32_t dhcp_packet_option_u32(const dhcp_option_t *opt);
void     dhcp_packet_option_ip(const dhcp_option_t *opt, uint8_t ip[4]);
```

## Usage

```c
uint8_t pkt[576];
int len = dhcp_packet_build_discover(pkt, sizeof(pkt), xid, mac);
udp_send(pkt, len);  /* send to 255.255.255.255:67 */

/* On reply: */
dhcp_header_t hdr;
dhcp_packet_parse(rx_buf, rx_len, &hdr);
if (hdr.msg_type == DHCP_MSG_OFFER) {
    /* hdr.yiaddr contains offered IP */
}
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\dhcp_packet/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_dhcp_packet
```


## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.


## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
