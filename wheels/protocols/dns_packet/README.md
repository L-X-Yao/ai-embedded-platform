# dns_packet — DNS query/response encoder/decoder

**Status: candidate**
**Layer: L2 (protocols)**

Stateless DNS packet building and parsing. Handles query construction,
header/record parsing, domain name encoding with pointer compression.
Pure C99, no heap. All buffers caller-provided.

## Origin

Proactive build for embedded DNS clients (e.g. hostname resolution over
UDP in IoT devices).

## Why self-written?

Searched GitHub (2026-05-02). Referenced spc476/SPCDNS (LGPL-3.0). Excellent implementation but LGPL is problematic for static linking in embedded firmware. Self-written for platform convention consistency; implementation verified against RFC 1035.

## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    DNS_OK, DNS_ERR_PARAMS, DNS_ERR_SIZE, DNS_ERR_FORMAT, DNS_ERR_OVERFLOW,
} dns_err_t;

/* Record types */
typedef enum {
    DNS_TYPE_A=1, DNS_TYPE_CNAME=5, DNS_TYPE_PTR=12,
    DNS_TYPE_MX=15, DNS_TYPE_TXT=16, DNS_TYPE_AAAA=28,
} dns_type_t;

/* Build a DNS query packet */
int dns_build_query(uint8_t *buf, uint16_t buf_size,
                    uint16_t id, const char *name, dns_type_t type);

/* Parse DNS header */
dns_err_t dns_parse_header(const uint8_t *buf, uint16_t len, dns_header_t *hdr);

/* Parse a question or answer record at offset; returns new offset */
int dns_parse_record(const uint8_t *buf, uint16_t len, uint16_t offset,
                     dns_record_t *record, bool is_answer);

/* Extract IPv4 from A record rdata */
dns_err_t dns_get_ipv4(const dns_record_t *record, uint8_t ip[4]);

/* Encode domain name to wire format */
int dns_encode_name(const char *name, uint8_t *buf, uint16_t buf_size);

/* Decode domain name from wire format (handles pointer compression) */
int dns_decode_name(const uint8_t *pkt, uint16_t pkt_len,
                    uint16_t offset, char *name, uint16_t name_size);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `DNS_NAME_MAX` | 253 | Max decoded domain name length (RFC 1035 max) |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## Migration

- `dns_record_t.type` and `.cls` are `uint16_t` now, not `dns_type_t` and
  `dns_class_t`, and the struct is a different size on the target.
- Comparing them with `DNS_TYPE_*` and `DNS_CLASS_*` works unchanged. Copying one
  into a `dns_type_t` or `dns_class_t` variable still cuts it to a byte on
  arm-none-eabi-gcc, so make that variable a `uint16_t`.
- Do nothing else: a record of type 257 now reads as 257, and `dns_get_ipv4()`
  refuses it, where on the target it read as an A record.

## How to test

```bash
cd wheels/protocols/dns_packet/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_dns_packet
```

## Test status

- Unit tests (sim): 24 pass, 0 fail, also built with -fshort-enums to match
  arm-none-eabi-gcc, which makes these enums one byte
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Transport-agnostic DNS wire codec; pointer compression
  support in decode; name buffer size configurable via DNS_NAME_MAX.

## Responsible human

@user
