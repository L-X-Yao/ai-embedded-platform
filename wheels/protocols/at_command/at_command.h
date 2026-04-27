#ifndef AT_COMMAND_H
#define AT_COMMAND_H

#include <stdint.h>

#ifndef AT_CMD_LINE_SIZE
#define AT_CMD_LINE_SIZE   256
#endif
#ifndef AT_CMD_MATCH_SIZE
#define AT_CMD_MATCH_SIZE  256
#endif
#ifndef AT_CMD_MAX_URCS
#define AT_CMD_MAX_URCS    16
#endif

/* ---------- events ---------- */

typedef enum {
    AT_EVT_NONE = 0,
    AT_EVT_OK,
    AT_EVT_ERROR,
    AT_EVT_URC,
    AT_EVT_PROMPT,
    AT_EVT_LINE,
} at_evt_type_t;

typedef struct {
    at_evt_type_t type;
    const char   *line;
    uint16_t      line_len;
} at_evt_t;

/* ---------- URC callback ---------- */

typedef void (*at_urc_cb_t)(const char *line, void *ctx);

typedef struct {
    const char *prefix;
    at_urc_cb_t cb;
    void       *ctx;
} at_urc_entry_t;

/* ---------- HAL (caller provides) ---------- */

typedef struct {
    void (*send)(void *ctx, const uint8_t *data, uint16_t len);
    void *ctx;
} at_hal_t;

/* ---------- parser state ---------- */

typedef struct {
    const at_hal_t *hal;
    char            line_buf[AT_CMD_LINE_SIZE];
    uint16_t        line_idx;
    char            expected[64];
    char            matched[AT_CMD_MATCH_SIZE];
    char            prev_line[AT_CMD_MATCH_SIZE];
    at_urc_entry_t  urcs[AT_CMD_MAX_URCS];
    uint8_t         urc_count;
    uint8_t         waiting;
} at_cmd_t;

/* ---------- API ---------- */

void          at_cmd_init(at_cmd_t *at, const at_hal_t *hal);
void          at_cmd_send(at_cmd_t *at, const char *cmd);
void          at_cmd_send_raw(at_cmd_t *at, const uint8_t *data, uint16_t len);
void          at_cmd_expect(at_cmd_t *at, const char *expected);
void          at_cmd_cancel(at_cmd_t *at);
at_evt_type_t at_cmd_feed(at_cmd_t *at, uint8_t byte, at_evt_t *evt);
int           at_cmd_register_urc(at_cmd_t *at, const char *prefix,
                                   at_urc_cb_t cb, void *ctx);
const char   *at_cmd_get_matched(const at_cmd_t *at);
const char   *at_cmd_get_prev_line(const at_cmd_t *at);

#endif /* AT_COMMAND_H */
