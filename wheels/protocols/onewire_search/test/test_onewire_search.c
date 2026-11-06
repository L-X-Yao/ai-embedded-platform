// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "onewire_search.h"
#include <string.h>

/* ========== Mock 1-Wire bus simulation ========== */

/*
 * Simulates N devices on a 1-Wire bus. Each device has an 8-byte ROM code.
 * The mock responds to the SEARCH ROM tree-walk protocol:
 *   - For each bit position, all active devices contribute their bit value.
 *   - read_bit returns: first call = OR of all active device bits at that position,
 *                        second call = OR of all active device complement bits.
 *   - write_bit narrows the active set to devices matching the chosen direction.
 */

#define MOCK_MAX_DEVICES  8

typedef struct {
    uint8_t roms[MOCK_MAX_DEVICES][8];
    uint8_t num_devices;
    /* Bus state during a search */
    bool    active[MOCK_MAX_DEVICES];  /* which devices are still responding */
    uint8_t bit_pos;                   /* current bit position (0-63) */
    uint8_t read_phase;                /* 0 = id_bit, 1 = complement */
    bool    presence;                  /* whether reset returns presence */
    /* Track command bits received */
    uint8_t cmd_bits_received;
    uint8_t cmd_byte;
    bool    in_command;                /* still receiving 8-bit command */
} mock_bus_t;

static void mock_bus_reset_state(mock_bus_t *m)
{
    for (uint8_t i = 0; i < m->num_devices; i++) {
        m->active[i] = true;
    }
    m->bit_pos = 0;
    m->read_phase = 0;
    m->cmd_bits_received = 0;
    m->cmd_byte = 0;
    m->in_command = true;
}

static bool mock_reset(void *ctx)
{
    mock_bus_t *m = (mock_bus_t *)ctx;
    mock_bus_reset_state(m);
    return m->presence && (m->num_devices > 0);
}

static void mock_write_bit(void *ctx, uint8_t bit)
{
    mock_bus_t *m = (mock_bus_t *)ctx;

    if (m->in_command) {
        /* Receiving SEARCH ROM command byte, bit by bit */
        if (bit) {
            m->cmd_byte |= (uint8_t)(1u << m->cmd_bits_received);
        }
        m->cmd_bits_received++;
        if (m->cmd_bits_received >= 8u) {
            m->in_command = false;
            /* cmd_byte should be 0xF0 (SEARCH ROM) */
        }
        return;
    }

    /* Direction bit for current bit_pos -- narrow active device set */
    uint8_t byte_idx = (uint8_t)(m->bit_pos / 8u);
    uint8_t bit_idx  = (uint8_t)(m->bit_pos % 8u);

    for (uint8_t i = 0; i < m->num_devices; i++) {
        if (!m->active[i]) { continue; }
        uint8_t dev_bit = (uint8_t)((m->roms[i][byte_idx] >> bit_idx) & 0x01u);
        if (dev_bit != (bit & 0x01u)) {
            m->active[i] = false;  /* device deselected */
        }
    }

    m->bit_pos++;
    m->read_phase = 0;
}

static uint8_t mock_read_bit(void *ctx)
{
    mock_bus_t *m = (mock_bus_t *)ctx;

    if (m->in_command) {
        /* Should not be reading during command phase, return 1 (idle) */
        return 1;
    }

    uint8_t byte_idx = (uint8_t)(m->bit_pos / 8u);
    uint8_t bit_idx  = (uint8_t)(m->bit_pos % 8u);

    if (m->read_phase == 0) {
        /* id_bit: wired-AND — 0 if ANY active device has bit=0 (pulls bus low) */
        m->read_phase = 1;
        uint8_t result = 1u;  /* pull-up default */
        uint8_t any_active = 0;
        for (uint8_t i = 0; i < m->num_devices; i++) {
            if (!m->active[i]) { continue; }
            any_active = 1;
            uint8_t dev_bit = (uint8_t)((m->roms[i][byte_idx] >> bit_idx) & 0x01u);
            if (!dev_bit) { result = 0u; }
        }
        if (!any_active) { return 1; } /* no active devices -> 1,1 = error */
        return result;
    } else {
        /* cmp_bit: wired-AND of complement — 0 if ANY device has bit=1 */
        m->read_phase = 0;
        uint8_t result = 1u;
        uint8_t any_active = 0;
        for (uint8_t i = 0; i < m->num_devices; i++) {
            if (!m->active[i]) { continue; }
            any_active = 1;
            uint8_t dev_bit = (uint8_t)((m->roms[i][byte_idx] >> bit_idx) & 0x01u);
            if (dev_bit) { result = 0u; }
        }
        if (!any_active) { return 1; } /* no active devices -> 1,1 */
        return result;
    }
}

/* Helper: compute CRC8 for a 7-byte ROM prefix and set byte[7] */
static void mock_set_rom_with_crc(uint8_t rom[8])
{
    /* CRC8 of first 7 bytes */
    uint8_t crc = 0;
    for (uint8_t i = 0; i < 7u; i++) {
        uint8_t byte = rom[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            uint8_t mix = (uint8_t)((crc ^ byte) & 0x01u);
            crc >>= 1;
            if (mix) { crc ^= 0x8Cu; }
            byte >>= 1;
        }
    }
    rom[7] = crc;
}

/* ========== Tests ========== */

/* ---------- single device ---------- */

TEST(test_single_device)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = true;
    mock.num_devices = 1;
    /* DS18B20-like ROM: family=0x28, serial=0x012345678900 */
    mock.roms[0][0] = 0x28;
    mock.roms[0][1] = 0x01;
    mock.roms[0][2] = 0x23;
    mock.roms[0][3] = 0x45;
    mock.roms[0][4] = 0x67;
    mock.roms[0][5] = 0x89;
    mock.roms[0][6] = 0x00;
    mock_set_rom_with_crc(mock.roms[0]);

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    onewire_search_rom_t rom;
    ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_OK);
    ASSERT_MEMEQ_N(rom.rom, mock.roms[0], 8);

    /* Should be done now */
    ASSERT_TRUE(onewire_search_done(&state));

    /* Next search should return NONE_FOUND */
    ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_ERR_NONE_FOUND);
}

/* ---------- two devices ---------- */

TEST(test_two_devices)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = true;
    mock.num_devices = 2;

    /* Device 0: family=0x28 */
    mock.roms[0][0] = 0x28;
    mock.roms[0][1] = 0xAA;
    mock.roms[0][2] = 0xBB;
    mock.roms[0][3] = 0xCC;
    mock.roms[0][4] = 0xDD;
    mock.roms[0][5] = 0xEE;
    mock.roms[0][6] = 0x01;
    mock_set_rom_with_crc(mock.roms[0]);

    /* Device 1: family=0x28, different serial */
    mock.roms[1][0] = 0x28;
    mock.roms[1][1] = 0x11;
    mock.roms[1][2] = 0x22;
    mock.roms[1][3] = 0x33;
    mock.roms[1][4] = 0x44;
    mock.roms[1][5] = 0x55;
    mock.roms[1][6] = 0x02;
    mock_set_rom_with_crc(mock.roms[1]);

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    /* Find first device */
    onewire_search_rom_t rom1;
    ASSERT_EQ(onewire_search_next(&state, &rom1), ONEWIRE_SEARCH_OK);
    ASSERT_FALSE(onewire_search_done(&state));

    /* Find second device */
    onewire_search_rom_t rom2;
    ASSERT_EQ(onewire_search_next(&state, &rom2), ONEWIRE_SEARCH_OK);
    ASSERT_TRUE(onewire_search_done(&state));

    /* Both devices should be found (order depends on ROM bit ordering) */
    bool found0 = (memcmp(rom1.rom, mock.roms[0], 8) == 0) ||
                  (memcmp(rom2.rom, mock.roms[0], 8) == 0);
    bool found1 = (memcmp(rom1.rom, mock.roms[1], 8) == 0) ||
                  (memcmp(rom2.rom, mock.roms[1], 8) == 0);
    ASSERT_TRUE(found0);
    ASSERT_TRUE(found1);

    /* ROMs should be different */
    ASSERT_TRUE(memcmp(rom1.rom, rom2.rom, 8) != 0);
}

/* ---------- three devices ---------- */

TEST(test_three_devices)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = true;
    mock.num_devices = 3;

    mock.roms[0][0] = 0x28;
    mock.roms[0][1] = 0x10;
    mock.roms[0][2] = 0x20;
    mock.roms[0][3] = 0x30;
    mock.roms[0][4] = 0x40;
    mock.roms[0][5] = 0x50;
    mock.roms[0][6] = 0x60;
    mock_set_rom_with_crc(mock.roms[0]);

    mock.roms[1][0] = 0x28;
    mock.roms[1][1] = 0xA0;
    mock.roms[1][2] = 0xB0;
    mock.roms[1][3] = 0xC0;
    mock.roms[1][4] = 0xD0;
    mock.roms[1][5] = 0xE0;
    mock.roms[1][6] = 0xF0;
    mock_set_rom_with_crc(mock.roms[1]);

    mock.roms[2][0] = 0x10;  /* different family */
    mock.roms[2][1] = 0x01;
    mock.roms[2][2] = 0x02;
    mock.roms[2][3] = 0x03;
    mock.roms[2][4] = 0x04;
    mock.roms[2][5] = 0x05;
    mock.roms[2][6] = 0x06;
    mock_set_rom_with_crc(mock.roms[2]);

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    onewire_search_rom_t found[3];
    ASSERT_EQ(onewire_search_next(&state, &found[0]), ONEWIRE_SEARCH_OK);
    ASSERT_FALSE(onewire_search_done(&state));

    ASSERT_EQ(onewire_search_next(&state, &found[1]), ONEWIRE_SEARCH_OK);
    ASSERT_FALSE(onewire_search_done(&state));

    ASSERT_EQ(onewire_search_next(&state, &found[2]), ONEWIRE_SEARCH_OK);
    ASSERT_TRUE(onewire_search_done(&state));

    /* All 3 devices found */
    for (int d = 0; d < 3; d++) {
        bool matched = false;
        for (int f = 0; f < 3; f++) {
            if (memcmp(mock.roms[d], found[f].rom, 8) == 0) {
                matched = true;
                break;
            }
        }
        ASSERT_TRUE(matched);
    }

    /* After last, next returns NONE_FOUND */
    onewire_search_rom_t extra;
    ASSERT_EQ(onewire_search_next(&state, &extra), ONEWIRE_SEARCH_ERR_NONE_FOUND);
}

/* ---------- no device on bus ---------- */

TEST(test_no_device)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = true;   /* presence = true but 0 devices */
    mock.num_devices = 0;

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    onewire_search_rom_t rom;
    /* reset returns false because num_devices=0 */
    ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_ERR_BUS);
    ASSERT_TRUE(onewire_search_done(&state));
}

/* ---------- no presence pulse ---------- */

TEST(test_no_presence)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = false;
    mock.num_devices = 1;
    mock.roms[0][0] = 0x28;
    mock_set_rom_with_crc(mock.roms[0]);

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    onewire_search_rom_t rom;
    ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_ERR_BUS);
}

/* ---------- CRC8 known vectors ---------- */

TEST(test_crc8_known_vectors)
{
    /* Dallas CRC8 of a valid ROM should give 0 when computed over all 8 bytes */
    uint8_t rom[8] = {0x28, 0x01, 0x23, 0x45, 0x67, 0x89, 0x00, 0x00};
    mock_set_rom_with_crc(rom);
    ASSERT_EQ(onewire_search_crc8(rom, 8), 0);

    /* CRC of first 7 bytes should equal byte 7 */
    uint8_t expected_crc = rom[7];
    ASSERT_EQ(onewire_search_crc8(rom, 7), expected_crc);

    /* Empty data */
    ASSERT_EQ(onewire_search_crc8(rom, 0), 0);

    /* Single byte: CRC8 of {0x00} */
    uint8_t zero = 0x00;
    ASSERT_EQ(onewire_search_crc8(&zero, 1), 0);

    /* Single byte: CRC8 of {0x01} = 0x5E (Dallas CRC) */
    uint8_t one = 0x01;
    /* Verify by computation: byte=0x01, bit0: mix=1, crc=0x8C; bits1-7: mix=0 */
    ASSERT_EQ(onewire_search_crc8(&one, 1), 0x5E);
}

/* ---------- CRC8 multiple ROMs ---------- */

TEST(test_crc8_multiple_roms)
{
    /* Generate several ROM codes and verify CRC integrity */
    uint8_t rom1[8] = {0x10, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x00};
    mock_set_rom_with_crc(rom1);
    ASSERT_EQ(onewire_search_crc8(rom1, 8), 0);

    uint8_t rom2[8] = {0x22, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00};
    mock_set_rom_with_crc(rom2);
    ASSERT_EQ(onewire_search_crc8(rom2, 8), 0);

    /* Corrupt one byte -- CRC should not be 0 */
    rom2[3] ^= 0x01;
    ASSERT_TRUE(onewire_search_crc8(rom2, 8) != 0);
}

/* ---------- search_done after last device ---------- */

TEST(test_done_after_last)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = true;
    mock.num_devices = 1;
    mock.roms[0][0] = 0x28;
    mock.roms[0][1] = 0x55;
    mock.roms[0][2] = 0x66;
    mock.roms[0][3] = 0x77;
    mock.roms[0][4] = 0x88;
    mock.roms[0][5] = 0x99;
    mock.roms[0][6] = 0xAA;
    mock_set_rom_with_crc(mock.roms[0]);

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    /* Before any search, not done */
    ASSERT_FALSE(onewire_search_done(&state));

    onewire_search_rom_t rom;
    ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_OK);

    /* After finding the only device, should be done */
    ASSERT_TRUE(onewire_search_done(&state));
}

/* ---------- search_first convenience ---------- */

TEST(test_search_first)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = true;
    mock.num_devices = 2;

    mock.roms[0][0] = 0x28;
    mock.roms[0][1] = 0xAA;
    mock.roms[0][2] = 0xBB;
    mock.roms[0][3] = 0xCC;
    mock.roms[0][4] = 0xDD;
    mock.roms[0][5] = 0xEE;
    mock.roms[0][6] = 0xFF;
    mock_set_rom_with_crc(mock.roms[0]);

    mock.roms[1][0] = 0x28;
    mock.roms[1][1] = 0x11;
    mock.roms[1][2] = 0x22;
    mock.roms[1][3] = 0x33;
    mock.roms[1][4] = 0x44;
    mock.roms[1][5] = 0x55;
    mock.roms[1][6] = 0x66;
    mock_set_rom_with_crc(mock.roms[1]);

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    /* Find first device normally */
    onewire_search_rom_t rom1;
    ASSERT_EQ(onewire_search_next(&state, &rom1), ONEWIRE_SEARCH_OK);

    /* Re-search from beginning using search_first */
    onewire_search_rom_t rom_first;
    ASSERT_EQ(onewire_search_first(&state, &rom_first), ONEWIRE_SEARCH_OK);

    /* Should get the same first device */
    ASSERT_MEMEQ_N(rom1.rom, rom_first.rom, 8);
}

/* ---------- NULL rejection ---------- */

TEST(test_null_rejection)
{
    onewire_search_state_t state;
    onewire_search_rom_t rom;

    /* init NULL */
    ASSERT_EQ(onewire_search_init(NULL, NULL), ONEWIRE_SEARCH_ERR_PARAM);
    ASSERT_EQ(onewire_search_init(&state, NULL), ONEWIRE_SEARCH_ERR_PARAM);

    /* Bus with NULL function pointers */
    onewire_search_bus_t bad_bus = { .ctx = NULL, .reset = NULL, .write_bit = NULL, .read_bit = NULL };
    ASSERT_EQ(onewire_search_init(&state, &bad_bus), ONEWIRE_SEARCH_ERR_PARAM);

    /* next NULL */
    ASSERT_EQ(onewire_search_next(NULL, &rom), ONEWIRE_SEARCH_ERR_PARAM);
    ASSERT_EQ(onewire_search_next(&state, NULL), ONEWIRE_SEARCH_ERR_PARAM);

    /* first NULL */
    ASSERT_EQ(onewire_search_first(NULL, &rom), ONEWIRE_SEARCH_ERR_PARAM);
    ASSERT_EQ(onewire_search_first(&state, NULL), ONEWIRE_SEARCH_ERR_PARAM);

    /* done NULL */
    ASSERT_TRUE(onewire_search_done(NULL));

    /* CRC8 NULL */
    ASSERT_EQ(onewire_search_crc8(NULL, 5), 0);
}

/* ---------- re-search from beginning ---------- */

TEST(test_re_search)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = true;
    mock.num_devices = 2;

    mock.roms[0][0] = 0x28;
    mock.roms[0][1] = 0x01;
    mock.roms[0][2] = 0x02;
    mock.roms[0][3] = 0x03;
    mock.roms[0][4] = 0x04;
    mock.roms[0][5] = 0x05;
    mock.roms[0][6] = 0x06;
    mock_set_rom_with_crc(mock.roms[0]);

    mock.roms[1][0] = 0x28;
    mock.roms[1][1] = 0xF1;
    mock.roms[1][2] = 0xF2;
    mock.roms[1][3] = 0xF3;
    mock.roms[1][4] = 0xF4;
    mock.roms[1][5] = 0xF5;
    mock.roms[1][6] = 0xF6;
    mock_set_rom_with_crc(mock.roms[1]);

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    /* First full enumeration */
    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    onewire_search_rom_t pass1[2];
    ASSERT_EQ(onewire_search_next(&state, &pass1[0]), ONEWIRE_SEARCH_OK);
    ASSERT_EQ(onewire_search_next(&state, &pass1[1]), ONEWIRE_SEARCH_OK);
    ASSERT_TRUE(onewire_search_done(&state));

    /* Second full enumeration -- re-init */
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    onewire_search_rom_t pass2[2];
    ASSERT_EQ(onewire_search_next(&state, &pass2[0]), ONEWIRE_SEARCH_OK);
    ASSERT_EQ(onewire_search_next(&state, &pass2[1]), ONEWIRE_SEARCH_OK);
    ASSERT_TRUE(onewire_search_done(&state));

    /* Same results both times */
    ASSERT_MEMEQ_N(pass1[0].rom, pass2[0].rom, 8);
    ASSERT_MEMEQ_N(pass1[1].rom, pass2[1].rom, 8);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)ONEWIRE_SEARCH_OK, 0);
    ASSERT_EQ((int)ONEWIRE_SEARCH_ERR_PARAM, -1);
    ASSERT_EQ((int)ONEWIRE_SEARCH_ERR_BUS, -2);
    ASSERT_EQ((int)ONEWIRE_SEARCH_ERR_CRC, -3);
    ASSERT_EQ((int)ONEWIRE_SEARCH_ERR_NONE_FOUND, -4);
}

/* ---------- constants ---------- */

TEST(test_constants)
{
    ASSERT_EQ(ONEWIRE_SEARCH_ROM_LEN, 8);
    ASSERT_EQ(ONEWIRE_SEARCH_CMD_SEARCH, 0xF0);
}

/* ---------- search_first on uninitialized bus ptr ---------- */

TEST(test_search_first_no_bus)
{
    onewire_search_state_t state;
    memset(&state, 0, sizeof(state));
    state.bus = NULL;

    onewire_search_rom_t rom;
    ASSERT_EQ(onewire_search_first(&state, &rom), ONEWIRE_SEARCH_ERR_PARAM);
}

/* ---------- eight devices (MOCK_MAX_DEVICES capacity) ---------- */

TEST(test_eight_devices)
{
    mock_bus_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.presence = true;
    mock.num_devices = 8;

    /* Use distinct family codes to ensure unique ROMs across all 8 devices */
    for (uint8_t d = 0; d < 8u; d++) {
        mock.roms[d][0] = (uint8_t)(0x10u + d);  /* family codes 0x10..0x17 */
        mock.roms[d][1] = d;
        mock.roms[d][2] = (uint8_t)(d * 2u);
        mock.roms[d][3] = (uint8_t)(d * 3u);
        mock.roms[d][4] = (uint8_t)(d * 5u);
        mock.roms[d][5] = (uint8_t)(d * 7u);
        mock.roms[d][6] = (uint8_t)(d * 11u);
        mock_set_rom_with_crc(mock.roms[d]);
    }

    onewire_search_bus_t bus = {
        .ctx = &mock,
        .reset = mock_reset,
        .write_bit = mock_write_bit,
        .read_bit = mock_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    onewire_search_rom_t found[8];
    for (uint8_t i = 0; i < 8u; i++) {
        if (i < 7u) {
            ASSERT_EQ(onewire_search_next(&state, &found[i]), ONEWIRE_SEARCH_OK);
            ASSERT_FALSE(onewire_search_done(&state));
        } else {
            ASSERT_EQ(onewire_search_next(&state, &found[i]), ONEWIRE_SEARCH_OK);
            ASSERT_TRUE(onewire_search_done(&state));
        }
    }

    /* Verify all 8 devices were discovered (order may vary) */
    for (uint8_t d = 0; d < 8u; d++) {
        bool matched = false;
        for (uint8_t f = 0; f < 8u; f++) {
            if (memcmp(mock.roms[d], found[f].rom, 8) == 0) {
                matched = true;
                break;
            }
        }
        ASSERT_TRUE(matched);
    }

    /* After last, next returns NONE_FOUND */
    onewire_search_rom_t extra;
    ASSERT_EQ(onewire_search_next(&state, &extra), ONEWIRE_SEARCH_ERR_NONE_FOUND);
}

/* ---------- CRC error during search ---------- */

/* A mock bus that returns a device with a bad CRC byte */
static uint8_t bad_crc_rom[8] = {0x28, 0x01, 0x23, 0x45, 0x67, 0x89, 0x00, 0xFF};

static mock_bus_t bad_crc_mock;

static void bad_crc_reset_state(mock_bus_t *m)
{
    m->active[0] = true;
    m->bit_pos = 0;
    m->read_phase = 0;
    m->cmd_bits_received = 0;
    m->cmd_byte = 0;
    m->in_command = true;
}

static bool bad_crc_reset(void *ctx)
{
    mock_bus_t *m = (mock_bus_t *)ctx;
    bad_crc_reset_state(m);
    return true;
}

static void bad_crc_write_bit(void *ctx, uint8_t bit)
{
    mock_bus_t *m = (mock_bus_t *)ctx;

    if (m->in_command) {
        m->cmd_bits_received++;
        if (m->cmd_bits_received >= 8u) { m->in_command = false; }
        return;
    }

    uint8_t byte_idx = (uint8_t)(m->bit_pos / 8u);
    uint8_t bit_idx  = (uint8_t)(m->bit_pos % 8u);
    uint8_t dev_bit  = (uint8_t)((bad_crc_rom[byte_idx] >> bit_idx) & 0x01u);
    if (dev_bit != (bit & 0x01u)) { m->active[0] = false; }
    m->bit_pos++;
    m->read_phase = 0;
}

static uint8_t bad_crc_read_bit(void *ctx)
{
    mock_bus_t *m = (mock_bus_t *)ctx;

    if (m->in_command || !m->active[0]) { return 1u; }

    uint8_t byte_idx = (uint8_t)(m->bit_pos / 8u);
    uint8_t bit_idx  = (uint8_t)(m->bit_pos % 8u);
    uint8_t dev_bit  = (uint8_t)((bad_crc_rom[byte_idx] >> bit_idx) & 0x01u);

    if (m->read_phase == 0) {
        m->read_phase = 1;
        return dev_bit;
    } else {
        m->read_phase = 0;
        return (uint8_t)(dev_bit ^ 0x01u);  /* complement */
    }
}

TEST(test_crc_error)
{
    /* bad_crc_rom has byte[7]=0xFF which is not the correct CRC of bytes[0..6] */
    uint8_t check_crc = 0;
    for (uint8_t i = 0; i < 7u; i++) {
        uint8_t byte = bad_crc_rom[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            uint8_t mix = (uint8_t)((check_crc ^ byte) & 0x01u);
            check_crc >>= 1;
            if (mix) { check_crc ^= 0x8Cu; }
            byte >>= 1;
        }
    }
    /* Confirm ROM's CRC byte does NOT match computed CRC */
    ASSERT_TRUE(bad_crc_rom[7] != check_crc);

    memset(&bad_crc_mock, 0, sizeof(bad_crc_mock));
    bad_crc_mock.presence = true;
    bad_crc_mock.num_devices = 1;
    bad_crc_mock.active[0] = true;

    onewire_search_bus_t bus = {
        .ctx = &bad_crc_mock,
        .reset = bad_crc_reset,
        .write_bit = bad_crc_write_bit,
        .read_bit = bad_crc_read_bit
    };

    onewire_search_state_t state;
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);

    onewire_search_rom_t rom;
    ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_ERR_CRC);
}

/* ---------- a bus that goes wrong on one walk ---------- */

typedef struct {
    mock_bus_t mock;
    int  walk;         /* resets so far */
    int  reads;        /* read_bit calls in this walk */
    int  fault_walk;   /* the walk to disturb, from 1 */
    int  first_read;   /* first read_bit call to invert, from 1 */
    int  last_read;    /* last read_bit call to invert */
} glitch_bus_t;

/* A walk reads two bits per ROM bit, so ROM bit n is reads 2n-1 and 2n. */
#define LAST_BIT_FIRST_READ 127
#define LAST_BIT_LAST_READ  128

static bool glitch_reset(void *ctx)
{
    glitch_bus_t *g = (glitch_bus_t *)ctx;
    g->walk++;
    g->reads = 0;
    return mock_reset(&g->mock);
}

static void glitch_write_bit(void *ctx, uint8_t bit)
{
    mock_write_bit(&((glitch_bus_t *)ctx)->mock, bit);
}

static uint8_t glitch_read_bit(void *ctx)
{
    glitch_bus_t *g = (glitch_bus_t *)ctx;
    uint8_t bit = mock_read_bit(&g->mock);

    g->reads++;
    if (g->walk == g->fault_walk &&
        g->reads >= g->first_read && g->reads <= g->last_read) {
        bit ^= 1u;
    }
    return bit;
}

/* Three devices, found in this order: they first differ at ROM bit 9. */
static void glitch_setup(glitch_bus_t *g, onewire_search_bus_t *bus,
                         onewire_search_state_t *state)
{
    static const uint8_t serial[3] = { 0x10, 0x21, 0x33 };

    memset(g, 0, sizeof(*g));
    g->mock.presence = true;
    g->mock.num_devices = 3;
    for (uint8_t i = 0; i < 3u; i++) {
        g->mock.roms[i][0] = 0x28;
        g->mock.roms[i][1] = serial[i];
        mock_set_rom_with_crc(g->mock.roms[i]);
    }
    bus->ctx = g;
    bus->reset = glitch_reset;
    bus->write_bit = glitch_write_bit;
    bus->read_bit = glitch_read_bit;
    (void)onewire_search_init(state, bus);
}

/* The devices from index first on have to turn up in order, then the end. */
static void expect_rest(onewire_search_state_t *state, const glitch_bus_t *g,
                        uint8_t first)
{
    onewire_search_rom_t rom;

    for (uint8_t i = first; i < g->mock.num_devices; i++) {
        ASSERT_EQ(onewire_search_next(state, &rom), ONEWIRE_SEARCH_OK);
        ASSERT_TRUE(memcmp(rom.rom, g->mock.roms[i], 8) == 0);
    }
    ASSERT_TRUE(onewire_search_done(state));
    ASSERT_EQ(onewire_search_next(state, &rom), ONEWIRE_SEARCH_ERR_NONE_FOUND);
}

TEST(test_clean_bus_finds_all_three)
{
    glitch_bus_t g;
    onewire_search_bus_t bus;
    onewire_search_state_t state;

    glitch_setup(&g, &bus, &state);
    expect_rest(&state, &g, 0);
}

static void crc_error_on_walk(int walk, uint8_t found_before)
{
    glitch_bus_t g;
    onewire_search_bus_t bus;
    onewire_search_state_t state;
    onewire_search_rom_t rom;

    glitch_setup(&g, &bus, &state);
    g.fault_walk = walk;
    g.first_read = LAST_BIT_FIRST_READ;
    g.last_read = LAST_BIT_LAST_READ;

    for (uint8_t i = 0; i < found_before; i++) {
        ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_OK);
    }
    ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_ERR_CRC);
    ASSERT_FALSE(onewire_search_done(&state));
    expect_rest(&state, &g, found_before);
}

TEST(test_crc_error_on_the_first_walk_loses_no_device)
{
    crc_error_on_walk(1, 0);
}

TEST(test_crc_error_mid_search_loses_no_device)
{
    crc_error_on_walk(2, 1);
}

TEST(test_crc_error_on_the_last_device_loses_no_device)
{
    crc_error_on_walk(3, 2);
}

/* ---------- a ROM of zeros is no device ---------- */

static bool always_present(void *ctx)
{
    (void)ctx;
    return true;
}

static void ignore_bit(void *ctx, uint8_t bit)
{
    (void)ctx;
    (void)bit;
}

static uint8_t always_low(void *ctx)
{
    (void)ctx;
    return 0;
}

TEST(test_bus_stuck_low_is_a_bus_error)
{
    /* Every bit reads 0,0 and the CRC of eight zero bytes is 0. */
    onewire_search_bus_t bus = {
        .ctx = NULL,
        .reset = always_present,
        .write_bit = ignore_bit,
        .read_bit = always_low
    };
    onewire_search_state_t state;
    onewire_search_rom_t rom;

    memset(&rom, 0xA5, sizeof(rom));
    ASSERT_EQ(onewire_search_init(&state, &bus), ONEWIRE_SEARCH_OK);
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_ERR_BUS);
        ASSERT_EQ(rom.rom[0], 0xA5);
    }
}

TEST(test_family_code_zero_is_refused)
{
    glitch_bus_t g;
    onewire_search_bus_t bus;
    onewire_search_state_t state;
    onewire_search_rom_t rom;

    glitch_setup(&g, &bus, &state);
    g.mock.num_devices = 1;
    g.mock.roms[0][0] = 0x00;
    mock_set_rom_with_crc(g.mock.roms[0]);
    ASSERT_EQ(onewire_search_crc8(g.mock.roms[0], 8u), 0);

    ASSERT_EQ(onewire_search_next(&state, &rom), ONEWIRE_SEARCH_ERR_BUS);
}

/* ---------- main ---------- */

TEST_MAIN("onewire_search")
    RUN(test_bus_stuck_low_is_a_bus_error);
    RUN(test_family_code_zero_is_refused);
    RUN(test_clean_bus_finds_all_three);
    RUN(test_crc_error_on_the_first_walk_loses_no_device);
    RUN(test_crc_error_mid_search_loses_no_device);
    RUN(test_crc_error_on_the_last_device_loses_no_device);
    RUN(test_single_device);
    RUN(test_two_devices);
    RUN(test_three_devices);
    RUN(test_no_device);
    RUN(test_no_presence);
    RUN(test_crc8_known_vectors);
    RUN(test_crc8_multiple_roms);
    RUN(test_done_after_last);
    RUN(test_search_first);
    RUN(test_null_rejection);
    RUN(test_re_search);
    RUN(test_error_codes);
    RUN(test_constants);
    RUN(test_search_first_no_bus);
    RUN(test_eight_devices);
    RUN(test_crc_error);
TEST_MAIN_END()
