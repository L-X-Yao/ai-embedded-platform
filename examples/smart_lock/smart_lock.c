// SPDX-License-Identifier: MIT
/*
 * smart_lock -- integration recipe: multi-factor smart door lock
 *
 * Demonstrates six wheels working together:
 *   L0  aes128        -- encrypt card UIDs before persisting
 *   L1  rc522         -- MFRC522 RFID reader (mocked via SPI HAL)
 *   L1  cst816s       -- capacitive touch IC for PIN entry (mocked I2C)
 *   L1  buzzer        -- audible feedback (mocked PWM HAL)
 *   L3  event_bus     -- decouples reader / pin-pad / lock controller
 *   L3  flash_journal -- append-only audit log (mocked flash sectors)
 *
 * Scenario: a door lock authenticates a holder via RFID card + numeric
 * PIN entered on a touch keypad. Five scenarios exercise the full
 * state machine: success, lockout after PIN errors, unknown card,
 * PIN-only fallback, and an emergency master code. Every attempt --
 * accepted or rejected -- gets an encrypted audit-log entry on flash.
 *
 * Runs entirely on host with mocked SPI / I2C / PWM / flash -- no
 * hardware required.
 *
 * Build:  cmake -B build -S . && cmake --build build && ./build/smart_lock
 */

#include "test_harness.h"

/* wheels */
#include "aes128.h"
#include "rc522.h"
#include "cst816s.h"
#include "buzzer.h"
#include "event_bus.h"
#include "flash_journal.h"

#include <string.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

/* Authorized card UIDs (4 bytes each) -- normally provisioned in factory */
static const uint8_t auth_card_a[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
static const uint8_t auth_card_b[4] = { 0xCA, 0xFE, 0xBA, 0xBE };

/* PINs paired with each card (BCD-style 4-digit code) */
static const uint8_t auth_pin_a[4] = { 1, 2, 3, 4 };
static const uint8_t auth_pin_b[4] = { 9, 8, 7, 6 };

/* Master / panic PIN that opens without a card */
static const uint8_t master_pin[6] = { 7, 7, 7, 7, 7, 7 };

#define MAX_PIN_ATTEMPTS  3        /* PIN tries before lockout */

/* AES-128 key for encrypting audit log entries (mock keystore) */
static const uint8_t audit_key[16] = {
    'a','u','d','i','t','-','k','e','y','-','1','2','3','4','5','6'
};

/* ======================================================================
 * Event bus IDs
 * ====================================================================== */

#define EVT_CARD_PRESENT   0x0001u   /* data = card UID (4 bytes) */
#define EVT_PIN_DIGIT      0x0002u   /* data = uint8_t digit       */
#define EVT_PIN_COMMIT     0x0003u   /* data = entered PIN bytes   */
#define EVT_AUTH_OK        0x0010u   /* data = audit text          */
#define EVT_AUTH_FAIL      0x0011u
#define EVT_LOCKOUT        0x0012u

/* ======================================================================
 * Mock SPI for RC522
 *
 * We intercept SPI traffic and serve a canned response stream so the
 * real rc522 driver runs unmodified. The mock recognizes:
 *   - REG_VERSION read returns 0x92 (chip ID check during init)
 *   - the transceive flow: COMIRQ poll returns 0x30 (RxIRq | IdleIRq)
 *   - FIFOLEVEL read returns the staged response length
 *   - FIFODATA reads serve the staged response bytes
 *
 * For `detect_card` we plant ATQA (2 bytes). For `read_card_uid` we
 * plant a 5-byte response: UID + BCC.
 * ====================================================================== */

static uint8_t spi_addr;          /* register address latched per transaction */
static bool    spi_is_read;       /* true after a read instruction byte */
static uint8_t fifo_response[16]; /* staged FIFO data for next transceive */
static uint8_t fifo_resp_len;     /* length of staged FIFO response */
static uint8_t fifo_resp_idx;     /* read cursor into fifo_response */
static uint8_t fifo_resp_remaining; /* how many bytes still pending */

/* Stage a 4-byte UID response (the next read_card_uid will see this) */
static void rfid_plant_uid(const uint8_t uid[4])
{
    fifo_response[0] = uid[0];
    fifo_response[1] = uid[1];
    fifo_response[2] = uid[2];
    fifo_response[3] = uid[3];
    fifo_response[4] = (uint8_t)(uid[0] ^ uid[1] ^ uid[2] ^ uid[3]);  /* BCC */
    fifo_resp_len = 5;
    fifo_resp_idx = 0;
    fifo_resp_remaining = 5;
}

/* Stage the ATQA response for detect_card */
static void rfid_plant_atqa(void)
{
    fifo_response[0] = 0x04;
    fifo_response[1] = 0x00;
    fifo_resp_len = 2;
    fifo_resp_idx = 0;
    fifo_resp_remaining = 2;
}

static void rfid_no_card(void)
{
    fifo_resp_len = 0;
    fifo_resp_idx = 0;
    fifo_resp_remaining = 0;
}

static void rfid_cs_low(void *ctx)
{
    (void)ctx;
    spi_is_read = false;
}

static void rfid_cs_high(void *ctx)
{
    (void)ctx;
}

static uint8_t rfid_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;

    /* If we haven't latched an address yet, this byte is the instruction */
    if (spi_addr == 0xFFu) {
        if (tx & 0x80u) {
            spi_is_read = true;
        } else {
            spi_is_read = false;
        }
        spi_addr = (uint8_t)((tx >> 1) & 0x3Fu);
        return 0xFFu;
    }

    /* Subsequent bytes after the address are data */
    if (spi_is_read) {
        uint8_t val = 0;
        switch (spi_addr) {
        case RC522_REG_VERSION:
            val = 0x92u;        /* MFRC522 v2.0 */
            break;
        case RC522_REG_COMIRQ:
            val = 0x30u;        /* RxIRq | IdleIRq -- ready */
            break;
        case RC522_REG_ERROR:
            val = 0x00u;        /* no errors */
            break;
        case RC522_REG_FIFOLEVEL:
            val = fifo_resp_remaining;
            break;
        case RC522_REG_FIFODATA:
            if (fifo_resp_idx < fifo_resp_len) {
                val = fifo_response[fifo_resp_idx++];
                if (fifo_resp_remaining > 0) fifo_resp_remaining--;
            }
            break;
        case RC522_REG_STATUS1:
            val = 0x00u;
            break;
        default:
            val = 0x00u;
            break;
        }
        /* Same instruction can be re-issued for FIFO burst reads -- do
         * NOT reset spi_addr here. Reset happens on next cs_low. */
        return val;
    } else {
        /* writes -- only side effect we care about: flushing FIFO */
        if (spi_addr == RC522_REG_FIFOLEVEL && (tx & 0x80u)) {
            /* FLUSH flag: do nothing, keep staged response intact */
        }
        return 0xFFu;
    }
}

static void rfid_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

/* Reset spi_addr before each transceive cycle. We accomplish this by
 * wrapping cs_low to clear spi_addr. */
static void rfid_cs_low_reset(void *ctx)
{
    spi_addr = 0xFFu;
    rfid_cs_low(ctx);
}

static const rc522_spi_t rfid_spi = {
    .cs_low   = rfid_cs_low_reset,
    .cs_high  = rfid_cs_high,
    .xfer     = rfid_xfer,
    .delay_ms = rfid_delay_ms,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock I2C for CST816S
 *
 * For PIN entry we directly write the touch register file each time we
 * want to simulate a tap. cst816s_read() will then pull the planted
 * coordinates / gesture out.
 * ====================================================================== */

static uint8_t  touch_regs[256];
static bool     touch_event_ready;    /* gated by application code */

static void touch_plant_tap(uint16_t x, uint16_t y)
{
    /* Single tap gesture, finger count 1, "touch down" event */
    touch_regs[0x01] = (uint8_t)CST816S_GESTURE_SINGLE_TAP;
    touch_regs[0x02] = 1;                  /* fingers */
    touch_regs[0x03] = (uint8_t)((x >> 8) & 0x0Fu);  /* x high (+event in bits 7:6) */
    touch_regs[0x04] = (uint8_t)(x & 0xFFu);
    touch_regs[0x05] = (uint8_t)((y >> 8) & 0x0Fu);
    touch_regs[0x06] = (uint8_t)(y & 0xFFu);
    touch_event_ready = true;
}

static void touch_clear(void)
{
    touch_regs[0x01] = (uint8_t)CST816S_GESTURE_NONE;
    touch_regs[0x02] = 0;
    touch_regs[0x03] = 0;
    touch_regs[0x04] = 0;
    touch_regs[0x05] = 0;
    touch_regs[0x06] = 0;
    touch_event_ready = false;
}

static bool touch_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg,
                            uint8_t val)
{
    (void)ctx; (void)dev_addr;
    touch_regs[reg] = val;
    return true;
}

static bool touch_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    (void)ctx;
    if (dev_addr != CST816S_DEV_ADDR) return false;

    /* During init the driver reads CHIP_ID (0xA7) -- plant it here */
    if (reg == 0xA7) {
        if (len < 1) return false;
        buf[0] = CST816S_CHIP_ID;
        return true;
    }
    if (reg == 0xA9) {
        if (len < 1) return false;
        buf[0] = 0x01;   /* fw version */
        return true;
    }
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = touch_regs[(uint8_t)(reg + i)];
    }
    return true;
}

static const cst816s_i2c_t touch_i2c = {
    .write_reg = touch_write_reg,
    .read_regs = touch_read_regs,
    .ctx       = NULL,
};

/* Convert (x,y) of a tap to a digit 0-9, based on a 3x4 keypad layout */
static int8_t keypad_xy_to_digit(uint16_t x, uint16_t y)
{
    /* Layout: keypad covers 240x320, divided 3 cols x 4 rows:
     *   row 0: 1 2 3
     *   row 1: 4 5 6
     *   row 2: 7 8 9
     *   row 3: * 0 #
     */
    uint8_t col = (uint8_t)(x / 80);   /* 0,1,2 */
    uint8_t row = (uint8_t)(y / 80);   /* 0..3   */
    if (col > 2 || row > 3) return -1;

    if (row == 3) {
        if (col == 1) return 0;
        return -1;  /* * or # ignored */
    }
    return (int8_t)(row * 3 + col + 1);
}

/* ======================================================================
 * Mock PWM for buzzer
 * ====================================================================== */

static uint32_t buzz_freq;
static uint8_t  buzz_duty;
static uint32_t buzz_total_ms;

static void buzz_set_freq(void *ctx, uint32_t hz)
{
    (void)ctx;
    buzz_freq = hz;
}

static void buzz_set_duty(void *ctx, uint8_t pct)
{
    (void)ctx;
    buzz_duty = pct;
}

static void buzz_delay_ms(void *ctx, uint16_t ms)
{
    (void)ctx;
    if (buzz_freq > 0) buzz_total_ms += ms;
}

static const buzzer_pwm_t buzzer_pwm = {
    .set_freq = buzz_set_freq,
    .set_duty = buzz_set_duty,
    .delay_ms = buzz_delay_ms,
    .ctx      = NULL,
};

/* ======================================================================
 * Mock flash storage for flash_journal -- 4 sectors of 256 bytes
 * ====================================================================== */

#define JRNL_SECTOR_COUNT  4
#define JRNL_SECTOR_SIZE 256

static uint8_t jrnl_flash[JRNL_SECTOR_COUNT][JRNL_SECTOR_SIZE];

static flash_journal_err_t jrnl_read(void *ctx, uint32_t s, uint32_t off,
                                      uint8_t *buf, uint32_t len)
{
    (void)ctx;
    if (s >= JRNL_SECTOR_COUNT || off + len > JRNL_SECTOR_SIZE)
        return FLASH_JOURNAL_ERR_PARAM;
    memcpy(buf, &jrnl_flash[s][off], len);
    return FLASH_JOURNAL_OK;
}

static flash_journal_err_t jrnl_write(void *ctx, uint32_t s, uint32_t off,
                                       const uint8_t *data, uint32_t len)
{
    (void)ctx;
    if (s >= JRNL_SECTOR_COUNT || off + len > JRNL_SECTOR_SIZE)
        return FLASH_JOURNAL_ERR_PARAM;
    memcpy(&jrnl_flash[s][off], data, len);
    return FLASH_JOURNAL_OK;
}

static flash_journal_err_t jrnl_erase(void *ctx, uint32_t s)
{
    (void)ctx;
    if (s >= JRNL_SECTOR_COUNT) return FLASH_JOURNAL_ERR_PARAM;
    memset(jrnl_flash[s], 0xFF, JRNL_SECTOR_SIZE);
    return FLASH_JOURNAL_OK;
}

static const flash_journal_storage_t jrnl_storage = {
    .read  = jrnl_read,
    .write = jrnl_write,
    .erase = jrnl_erase,
    .ctx   = NULL,
};

/* ======================================================================
 * Lock controller state -- subscribes to all relevant events
 * ====================================================================== */

typedef enum {
    LOCK_IDLE = 0,
    LOCK_HAS_CARD,
    LOCK_AWAITING_PIN,
    LOCK_OPENED,
    LOCK_LOCKED_OUT,
} lock_state_t;

typedef struct {
    lock_state_t   state;
    uint8_t        cur_uid[4];
    uint8_t        pin_buf[6];
    uint8_t        pin_len;
    uint8_t        failed_attempts;
    uint32_t       opens_total;
    uint32_t       rejects_total;
    uint32_t       lockouts_total;
} lock_ctrl_t;

static lock_ctrl_t      lock;
static rc522_t          rfid;
static cst816s_t        touch;
static buzzer_t         buzz;
static aes128_t         audit_aes;
static event_bus_t      bus;
static flash_journal_t  jrnl;

/* Identify a card UID against the auth table; -1 = unknown */
static int8_t card_lookup(const uint8_t uid[4])
{
    if (memcmp(uid, auth_card_a, 4) == 0) return 0;
    if (memcmp(uid, auth_card_b, 4) == 0) return 1;
    return -1;
}

static bool pin_matches(const uint8_t *got, uint8_t got_len,
                        const uint8_t *expect, uint8_t expect_len)
{
    if (got_len != expect_len) return false;
    return memcmp(got, expect, got_len) == 0;
}

/* Write an encrypted audit-log entry to flash_journal.
 *
 * Plaintext layout (16 bytes, one AES block):
 *   [0]    event code (0=open, 1=reject, 2=lockout)
 *   [1-4]  card UID (zeros if unknown)
 *   [5-10] PIN digits (or 0xFF padding)
 *   [11-14] sequence number
 *   [15]   padding
 */
static void audit_log_write(uint8_t code, const uint8_t *uid,
                            const uint8_t *pin, uint8_t pin_len,
                            uint32_t seq)
{
    uint8_t plain[16] = {0};
    plain[0] = code;
    if (uid) memcpy(&plain[1], uid, 4);
    memset(&plain[5], 0xFF, 6);
    if (pin) memcpy(&plain[5], pin, pin_len);
    plain[11] = (uint8_t)(seq & 0xFFu);
    plain[12] = (uint8_t)((seq >> 8) & 0xFFu);
    plain[13] = (uint8_t)((seq >> 16) & 0xFFu);
    plain[14] = (uint8_t)((seq >> 24) & 0xFFu);

    uint8_t cipher[16];
    aes128_encrypt_block(&audit_aes, plain, cipher);

    flash_journal_append(&jrnl, cipher, sizeof(cipher));
}

static void play_beep(uint16_t count)
{
    buzzer_beep(&buzz, count, 80, 60);
}

/* Pull the card UID from event data and look it up. */
static void on_card_present(uint16_t event_id, const void *data,
                            size_t data_len, void *ctx)
{
    (void)event_id; (void)ctx;
    if (data_len < 4) return;
    if (lock.state == LOCK_LOCKED_OUT) return;

    memcpy(lock.cur_uid, data, 4);
    int8_t idx = card_lookup(lock.cur_uid);
    if (idx < 0) {
        /* unknown card -- short beep + reject log + back to IDLE */
        play_beep(2);
        lock.rejects_total++;
        audit_log_write(1, lock.cur_uid, NULL, 0,
                        lock.rejects_total + lock.opens_total +
                        lock.lockouts_total);
        event_bus_publish(&bus, EVT_AUTH_FAIL,
                          "unknown card", 12);
        lock.state = LOCK_IDLE;
        return;
    }
    /* Known card -- request PIN */
    play_beep(1);
    lock.state = LOCK_AWAITING_PIN;
    lock.pin_len = 0;
}

static void on_pin_digit(uint16_t event_id, const void *data,
                         size_t data_len, void *ctx)
{
    (void)event_id; (void)ctx;
    if (data_len < 1) return;
    if (lock.pin_len >= sizeof(lock.pin_buf)) return;
    if (lock.state != LOCK_AWAITING_PIN &&
        lock.state != LOCK_IDLE) return;
    lock.pin_buf[lock.pin_len++] = ((const uint8_t *)data)[0];
}

static void on_pin_commit(uint16_t event_id, const void *data,
                          size_t data_len, void *ctx)
{
    (void)event_id; (void)data; (void)data_len; (void)ctx;

    /* Path A: master/panic PIN works in any state */
    if (pin_matches(lock.pin_buf, lock.pin_len,
                    master_pin, sizeof(master_pin))) {
        lock.opens_total++;
        play_beep(3);
        audit_log_write(0, NULL, lock.pin_buf, lock.pin_len,
                        lock.rejects_total + lock.opens_total +
                        lock.lockouts_total);
        event_bus_publish(&bus, EVT_AUTH_OK, "master", 6);
        lock.state = LOCK_OPENED;
        /* clear PIN buffer */
        memset(lock.pin_buf, 0, sizeof(lock.pin_buf));
        lock.pin_len = 0;
        lock.failed_attempts = 0;
        return;
    }

    /* Path B: card present -- verify PIN against that card */
    if (lock.state == LOCK_AWAITING_PIN) {
        int8_t idx = card_lookup(lock.cur_uid);
        const uint8_t *expect_pin = (idx == 0) ? auth_pin_a : auth_pin_b;
        if (pin_matches(lock.pin_buf, lock.pin_len, expect_pin, 4)) {
            lock.opens_total++;
            play_beep(2);
            audit_log_write(0, lock.cur_uid, lock.pin_buf, lock.pin_len,
                            lock.rejects_total + lock.opens_total +
                            lock.lockouts_total);
            event_bus_publish(&bus, EVT_AUTH_OK,
                              "card+pin", 8);
            lock.state = LOCK_OPENED;
        } else {
            lock.failed_attempts++;
            lock.rejects_total++;
            audit_log_write(1, lock.cur_uid, lock.pin_buf, lock.pin_len,
                            lock.rejects_total + lock.opens_total +
                            lock.lockouts_total);
            event_bus_publish(&bus, EVT_AUTH_FAIL,
                              "bad pin", 7);
            play_beep(4);

            if (lock.failed_attempts >= MAX_PIN_ATTEMPTS) {
                lock.lockouts_total++;
                audit_log_write(2, lock.cur_uid, NULL, 0,
                                lock.rejects_total + lock.opens_total +
                                lock.lockouts_total);
                event_bus_publish(&bus, EVT_LOCKOUT,
                                  "lockout", 7);
                lock.state = LOCK_LOCKED_OUT;
            }
        }
        /* clear PIN buffer regardless */
        memset(lock.pin_buf, 0, sizeof(lock.pin_buf));
        lock.pin_len = 0;
        return;
    }

    /* Path C: no card and not master -- reject (PIN-only fallback failed) */
    lock.rejects_total++;
    audit_log_write(1, NULL, lock.pin_buf, lock.pin_len,
                    lock.rejects_total + lock.opens_total +
                    lock.lockouts_total);
    event_bus_publish(&bus, EVT_AUTH_FAIL,
                      "pin-only fail", 13);
    play_beep(3);
    memset(lock.pin_buf, 0, sizeof(lock.pin_buf));
    lock.pin_len = 0;
}

/* Pretty-printer used by the test driver */
static void on_auth_ok(uint16_t event_id, const void *data,
                       size_t data_len, void *ctx)
{
    (void)event_id; (void)ctx;
    printf("      AUTH_OK (%.*s)\n", (int)data_len, (const char *)data);
}

static void on_auth_fail(uint16_t event_id, const void *data,
                         size_t data_len, void *ctx)
{
    (void)event_id; (void)ctx;
    printf("      AUTH_FAIL (%.*s)\n", (int)data_len, (const char *)data);
}

static void on_lockout(uint16_t event_id, const void *data,
                       size_t data_len, void *ctx)
{
    (void)event_id; (void)ctx;
    printf("      LOCKOUT (%.*s)\n", (int)data_len, (const char *)data);
}

/* ======================================================================
 * Helpers: drive the lock through scripted scenarios
 * ====================================================================== */

static void simulate_card_swipe(const uint8_t uid[4])
{
    rfid_plant_atqa();
    ASSERT_EQ(rc522_detect_card(&rfid), RC522_OK);
    rfid_plant_uid(uid);
    rc522_uid_t got;
    ASSERT_EQ(rc522_read_card_uid(&rfid, &got), RC522_OK);
    event_bus_publish(&bus, EVT_CARD_PRESENT, got.bytes, got.len);
}

static void simulate_pin_entry(const uint8_t *digits, uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) {
        /* Plant a tap somewhere in the keypad grid, then convert back */
        uint8_t d = digits[i];
        /* compute a representative (x,y) for digit d */
        uint16_t row, col;
        if (d == 0) { row = 3; col = 1; }
        else        { row = (uint16_t)((d - 1) / 3); col = (uint16_t)((d - 1) % 3); }
        uint16_t x = (uint16_t)(col * 80 + 40);
        uint16_t y = (uint16_t)(row * 80 + 40);
        touch_plant_tap(x, y);
        cst816s_touch_t t;
        ASSERT_EQ(cst816s_read(&touch, &t), CST816S_OK);
        int8_t parsed = keypad_xy_to_digit(t.x, t.y);
        ASSERT_EQ(parsed, (int8_t)d);
        event_bus_publish(&bus, EVT_PIN_DIGIT, &d, sizeof(d));
        touch_clear();
    }
    event_bus_publish(&bus, EVT_PIN_COMMIT, NULL, 0);
}

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* Reset all mock state */
    memset(jrnl_flash, 0xFF, sizeof(jrnl_flash));
    rfid_no_card();
    touch_clear();
    buzz_freq = 0; buzz_duty = 0; buzz_total_ms = 0;
    memset(&lock, 0, sizeof(lock));

    /* AES key load */
    ASSERT_EQ(aes128_init(&audit_aes, audit_key), AES128_OK);

    /* event_bus */
    ASSERT_EQ(event_bus_init(&bus), EVENT_BUS_OK);

    /* flash_journal */
    ASSERT_EQ(flash_journal_init(&jrnl, &jrnl_storage,
                                 JRNL_SECTOR_COUNT, JRNL_SECTOR_SIZE),
              FLASH_JOURNAL_OK);

    /* RC522 init -- mock SPI returns version 0x92 */
    spi_addr = 0xFFu;
    ASSERT_EQ(rc522_init(&rfid, &rfid_spi), RC522_OK);
    ASSERT_EQ(rc522_get_version(&rfid), 0x92);

    /* CST816S init */
    ASSERT_EQ(cst816s_init(&touch, &touch_i2c), CST816S_OK);

    /* Buzzer init */
    ASSERT_EQ(buzzer_init(&buzz, &buzzer_pwm), BUZZER_OK);

    /* Subscribe lock controller to events */
    ASSERT_EQ(event_bus_subscribe(&bus, EVT_CARD_PRESENT,
                                  on_card_present, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_subscribe(&bus, EVT_PIN_DIGIT,
                                  on_pin_digit, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_subscribe(&bus, EVT_PIN_COMMIT,
                                  on_pin_commit, NULL), EVENT_BUS_OK);

    /* Subscribe pretty-printers */
    ASSERT_EQ(event_bus_subscribe(&bus, EVT_AUTH_OK,
                                  on_auth_ok, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_subscribe(&bus, EVT_AUTH_FAIL,
                                  on_auth_fail, NULL), EVENT_BUS_OK);
    ASSERT_EQ(event_bus_subscribe(&bus, EVT_LOCKOUT,
                                  on_lockout, NULL), EVENT_BUS_OK);

    printf("    all 6 wheels initialized; event bus has 6 subscribers\n");
}

TEST(test_scenario_card_pin_success)
{
    printf("\n  --- scenario 1: card A + correct PIN ---\n");
    simulate_card_swipe(auth_card_a);
    simulate_pin_entry(auth_pin_a, 4);
    ASSERT_EQ(lock.state, LOCK_OPENED);
    ASSERT_EQ(lock.opens_total, 1u);
    ASSERT_EQ(lock.rejects_total, 0u);

    /* Reset to IDLE for the next scenario (lock relocks) */
    lock.state = LOCK_IDLE;
}

TEST(test_scenario_pin_brute_force_lockout)
{
    printf("\n  --- scenario 2: card B + 3 wrong PINs -> lockout ---\n");
    simulate_card_swipe(auth_card_b);
    ASSERT_EQ(lock.state, LOCK_AWAITING_PIN);

    /* Three bad PINs in a row */
    static const uint8_t bad1[4] = {0,0,0,0};
    static const uint8_t bad2[4] = {1,1,1,1};
    static const uint8_t bad3[4] = {2,2,2,2};

    simulate_pin_entry(bad1, 4);
    ASSERT_EQ(lock.state, LOCK_AWAITING_PIN);
    ASSERT_EQ(lock.failed_attempts, 1u);

    simulate_pin_entry(bad2, 4);
    ASSERT_EQ(lock.state, LOCK_AWAITING_PIN);
    ASSERT_EQ(lock.failed_attempts, 2u);

    simulate_pin_entry(bad3, 4);
    ASSERT_EQ(lock.state, LOCK_LOCKED_OUT);
    ASSERT_EQ(lock.lockouts_total, 1u);
    ASSERT_GE(lock.rejects_total, 3u);
}

TEST(test_scenario_unknown_card)
{
    printf("\n  --- scenario 3: unknown card swipe (rejected) ---\n");
    /* While locked out, additional card swipes are no-ops; clear state */
    lock.state = LOCK_IDLE;
    lock.failed_attempts = 0;

    uint32_t rej_before = lock.rejects_total;
    static const uint8_t unknown_uid[4] = {0x11, 0x22, 0x33, 0x44};
    simulate_card_swipe(unknown_uid);
    ASSERT_EQ(lock.state, LOCK_IDLE);
    ASSERT_EQ(lock.rejects_total, rej_before + 1);
}

TEST(test_scenario_pin_only_fallback)
{
    printf("\n  --- scenario 4: PIN-only without a card (rejected) ---\n");
    lock.state = LOCK_IDLE;
    uint32_t rej_before = lock.rejects_total;

    /* User enters a card-PIN but never swiped a card -- should reject */
    simulate_pin_entry(auth_pin_a, 4);
    ASSERT_EQ(lock.state, LOCK_IDLE);
    ASSERT_EQ(lock.rejects_total, rej_before + 1);
}

TEST(test_scenario_master_emergency_pin)
{
    printf("\n  --- scenario 5: master PIN (777777) opens regardless ---\n");
    lock.state = LOCK_IDLE;
    uint32_t opens_before = lock.opens_total;

    simulate_pin_entry(master_pin, sizeof(master_pin));

    ASSERT_EQ(lock.state, LOCK_OPENED);
    ASSERT_EQ(lock.opens_total, opens_before + 1);
}

TEST(test_audit_journal_integrity)
{
    /* We should have written one journal entry per attempt:
     *  scenario 1: 1 OK
     *  scenario 2: 3 fails + 1 lockout = 4
     *  scenario 3: 1 reject
     *  scenario 4: 1 reject
     *  scenario 5: 1 OK
     *  total = 8
     */
    uint32_t count = flash_journal_count(&jrnl);
    ASSERT_EQ(count, 8u);

    /* Read latest and decrypt to verify AES roundtrip on persisted data */
    uint8_t cipher[16];
    uint32_t sz = sizeof(cipher);
    ASSERT_EQ(flash_journal_read_latest(&jrnl, cipher, &sz),
              FLASH_JOURNAL_OK);
    ASSERT_EQ(sz, 16u);

    uint8_t plain[16];
    ASSERT_EQ(aes128_decrypt_block(&audit_aes, cipher, plain), AES128_OK);

    /* Latest entry should be the master-pin OPEN: event code = 0 */
    ASSERT_EQ(plain[0], 0);

    printf("\n  --- audit journal ---\n");
    printf("    %u encrypted entries on flash\n", (unsigned)count);
    printf("    latest decrypted: code=%u seq=%u\n",
           (unsigned)plain[0],
           (unsigned)(plain[11] | (plain[12] << 8)));

    /* Zeroize AES context */
    aes128_clear(&audit_aes);
}

TEST(test_buzzer_was_used)
{
    /* The buzzer mock accumulated some on-time across the scenarios */
    ASSERT_GT(buzz_total_ms, 0);
    printf("    buzzer rang for %u ms total across all scenarios\n",
           (unsigned)buzz_total_ms);
}

/* ======================================================================
 * Main
 * ====================================================================== */

TEST_MAIN("smart_lock integration")
    RUN(test_init_all);
    RUN(test_scenario_card_pin_success);
    RUN(test_scenario_pin_brute_force_lockout);
    RUN(test_scenario_unknown_card);
    RUN(test_scenario_pin_only_fallback);
    RUN(test_scenario_master_emergency_pin);
    RUN(test_audit_journal_integrity);
    RUN(test_buzzer_was_used);
TEST_MAIN_END()
