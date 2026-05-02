# bitmap — fixed-size bitmap (bit array) for resource tracking

**Status: candidate**
**Layer: L0 (primitives)**

Caller-provided storage bitmap with set/clear/test/toggle, find-first-set/clear,
count, and alloc/free for slot management. All operations O(n/8) worst case.
No heap, no OS, pure C99.

## Origin

Proactive build for memory pool tracking, slot allocation, and peripheral
resource management in embedded systems.

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
typedef enum {
    BITMAP_OK        =  0,
    BITMAP_ERR_PARAM = -1,
    BITMAP_ERR_RANGE = -2,
    BITMAP_ERR_FULL  = -3,
} bitmap_err_t;

typedef struct {
    uint8_t  *data;   /* caller-provided storage, (bits+7)/8 bytes */
    uint16_t  bits;   /* total number of bits */
} bitmap_t;

bitmap_err_t bitmap_init(bitmap_t *bm, uint8_t *storage, uint16_t bits);
bitmap_err_t bitmap_set(bitmap_t *bm, uint16_t bit);
bitmap_err_t bitmap_clear(bitmap_t *bm, uint16_t bit);
bool         bitmap_test(const bitmap_t *bm, uint16_t bit);
bitmap_err_t bitmap_toggle(bitmap_t *bm, uint16_t bit);
void         bitmap_set_all(bitmap_t *bm);
void         bitmap_clear_all(bitmap_t *bm);
int          bitmap_find_first_set(const bitmap_t *bm);
int          bitmap_find_first_clear(const bitmap_t *bm);
uint16_t     bitmap_count_set(const bitmap_t *bm);
uint16_t     bitmap_count_clear(const bitmap_t *bm);
uint16_t     bitmap_size(const bitmap_t *bm);
int          bitmap_alloc(bitmap_t *bm);        /* find first clear, set it */
bitmap_err_t bitmap_free(bitmap_t *bm, uint16_t bit);  /* clear bit */
```

## Configuration (compile-time)

None. All configuration is through the `bitmap_init` call (size, storage).

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/bitmap/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bitmap
```

## Test status

- Unit tests (sim): 17 pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: caller-provided storage (no malloc); alloc/free pair for
  slot management; O(n/8) operations; padding bits in last byte always zeroed.

## Responsible human

@user
