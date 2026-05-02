/*
 * flash_journal -- append-only log journal for flash storage with wear leveling
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none -- self-contained)
 */

#include "flash_journal.h"
#include <string.h>

/* ---------- Internal CRC32 (self-contained, no cross-wheel include) -------- */

static uint32_t crc32_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
    return crc;
}

/* ---------- Header encode / decode helpers --------------------------------- */

static void encode_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFFu);
    buf[1] = (uint8_t)((val >> 8) & 0xFFu);
    buf[2] = (uint8_t)((val >> 16) & 0xFFu);
    buf[3] = (uint8_t)((val >> 24) & 0xFFu);
}

static uint32_t decode_u32(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static void encode_u16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFFu);
    buf[1] = (uint8_t)((val >> 8) & 0xFFu);
}

static uint16_t decode_u16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

/* ---------- Entry read helper ---------------------------------------------- */

/*
 * Try to read and validate one entry at (sector, offset).
 * On success: *out_seq, *out_len set, payload written to payload_buf.
 * Returns FLASH_JOURNAL_OK or an error code.
 */
static flash_journal_err_t read_entry(const flash_journal_t *jrnl,
                                      uint32_t sector, uint32_t offset,
                                      uint32_t *out_seq, uint16_t *out_len,
                                      uint8_t *payload_buf, uint32_t buf_size)
{
    const flash_journal_storage_t *st = jrnl->storage;
    uint8_t hdr[FLASH_JOURNAL_HDR_SIZE];
    flash_journal_err_t err;

    /* enough room for at least a header? */
    if (offset + FLASH_JOURNAL_OVERHEAD > jrnl->sector_size) {
        return FLASH_JOURNAL_ERR_CORRUPT;
    }

    err = st->read(st->ctx, sector, offset, hdr, FLASH_JOURNAL_HDR_SIZE);
    if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;

    uint32_t magic = decode_u32(&hdr[0]);
    if (magic != FLASH_JOURNAL_MAGIC) return FLASH_JOURNAL_ERR_CORRUPT;

    *out_seq = decode_u32(&hdr[4]);
    *out_len = decode_u16(&hdr[8]);

    uint32_t total = FLASH_JOURNAL_OVERHEAD + (uint32_t)*out_len;
    if (offset + total > jrnl->sector_size) return FLASH_JOURNAL_ERR_CORRUPT;

    if (payload_buf != NULL) {
        if ((uint32_t)*out_len > buf_size) return FLASH_JOURNAL_ERR_SIZE;
        err = st->read(st->ctx, sector, offset + FLASH_JOURNAL_HDR_SIZE,
                       payload_buf, *out_len);
        if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;
    }

    /* Read and verify CRC over header + payload */
    uint8_t crc_buf[FLASH_JOURNAL_CRC_SIZE];
    err = st->read(st->ctx, sector,
                   offset + FLASH_JOURNAL_HDR_SIZE + (uint32_t)*out_len,
                   crc_buf, FLASH_JOURNAL_CRC_SIZE);
    if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;

    uint32_t stored_crc = decode_u32(crc_buf);

    /* Compute CRC over header + payload */
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < FLASH_JOURNAL_HDR_SIZE; i++) {
        crc = crc32_byte(crc, hdr[i]);
    }
    /* Re-read payload for CRC if we didn't keep it */
    if (payload_buf != NULL && *out_len > 0) {
        for (uint16_t i = 0; i < *out_len; i++) {
            crc = crc32_byte(crc, payload_buf[i]);
        }
    } else if (*out_len > 0) {
        /* Read payload in chunks for CRC check */
        uint8_t tmp[32];
        uint16_t remaining = *out_len;
        uint32_t pos = offset + FLASH_JOURNAL_HDR_SIZE;
        while (remaining > 0) {
            uint16_t chunk = remaining > 32u ? 32u : remaining;
            err = st->read(st->ctx, sector, pos, tmp, chunk);
            if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;
            for (uint16_t i = 0; i < chunk; i++) {
                crc = crc32_byte(crc, tmp[i]);
            }
            pos += chunk;
            remaining = (uint16_t)(remaining - chunk);
        }
    }
    crc ^= 0xFFFFFFFFu;

    if (crc != stored_crc) return FLASH_JOURNAL_ERR_CORRUPT;

    return FLASH_JOURNAL_OK;
}

/* Count entries in a single sector starting at offset 0. */
static uint32_t count_sector_entries(const flash_journal_t *jrnl,
                                     uint32_t sector,
                                     uint32_t *last_seq,
                                     uint32_t *last_offset)
{
    uint32_t offset = 0;
    uint32_t count = 0;
    uint32_t seq;
    uint16_t len;

    while (offset + FLASH_JOURNAL_OVERHEAD <= jrnl->sector_size) {
        flash_journal_err_t err = read_entry(jrnl, sector, offset,
                                             &seq, &len, NULL, 0);
        if (err != FLASH_JOURNAL_OK) break;
        if (last_seq != NULL)    *last_seq = seq;
        if (last_offset != NULL) *last_offset = offset;
        offset += FLASH_JOURNAL_OVERHEAD + (uint32_t)len;
        count++;
    }
    return count;
}

/* ---------- Public API ----------------------------------------------------- */

flash_journal_err_t flash_journal_init(flash_journal_t *jrnl,
                                       const flash_journal_storage_t *storage,
                                       uint32_t sector_count,
                                       uint32_t sector_size)
{
    if (jrnl == NULL || storage == NULL) return FLASH_JOURNAL_ERR_PARAM;
    if (storage->read == NULL || storage->write == NULL || storage->erase == NULL)
        return FLASH_JOURNAL_ERR_PARAM;
    if (sector_count == 0 || sector_count > FLASH_JOURNAL_MAX_SECTORS)
        return FLASH_JOURNAL_ERR_PARAM;
    if (sector_size <= FLASH_JOURNAL_OVERHEAD)
        return FLASH_JOURNAL_ERR_PARAM;

    memset(jrnl, 0, sizeof(*jrnl));
    jrnl->storage = storage;
    jrnl->sector_count = sector_count;
    jrnl->sector_size = sector_size;
    jrnl->next_seq = 1;

    /* Scan all sectors to find entries and recover state */
    uint32_t total_entries = 0;
    uint32_t max_seq = 0;
    uint32_t max_seq_sector = 0;
    uint32_t max_seq_offset = 0;
    uint32_t min_seq = UINT32_MAX;
    uint32_t min_seq_sector = 0;
    bool found_any = false;

    for (uint32_t s = 0; s < sector_count; s++) {
        uint32_t offset = 0;
        uint32_t seq;
        uint16_t len;

        while (offset + FLASH_JOURNAL_OVERHEAD <= sector_size) {
            flash_journal_err_t err = read_entry(jrnl, s, offset,
                                                 &seq, &len, NULL, 0);
            if (err != FLASH_JOURNAL_OK) break;

            total_entries++;
            if (!found_any || seq > max_seq) {
                max_seq = seq;
                max_seq_sector = s;
                max_seq_offset = offset;
            }
            if (!found_any || seq < min_seq) {
                min_seq = seq;
                min_seq_sector = s;
            }
            found_any = true;
            offset += FLASH_JOURNAL_OVERHEAD + (uint32_t)len;
        }
    }

    if (found_any) {
        jrnl->entry_count = total_entries;
        jrnl->next_seq = max_seq + 1;
        jrnl->oldest_sector = min_seq_sector;
        jrnl->oldest_offset = 0; /* oldest is always at sector start */

        /* Write position: after the last entry in max_seq's sector */
        uint16_t last_len;
        uint32_t dummy_seq;
        flash_journal_err_t err = read_entry(jrnl, max_seq_sector,
                                             max_seq_offset, &dummy_seq,
                                             &last_len, NULL, 0);
        if (err == FLASH_JOURNAL_OK) {
            jrnl->write_sector = max_seq_sector;
            jrnl->write_offset = max_seq_offset + FLASH_JOURNAL_OVERHEAD
                               + (uint32_t)last_len;
        }
    }

    return FLASH_JOURNAL_OK;
}

flash_journal_err_t flash_journal_append(flash_journal_t *jrnl,
                                         const uint8_t *data, uint16_t len)
{
    if (jrnl == NULL || data == NULL || len == 0) return FLASH_JOURNAL_ERR_PARAM;

    uint32_t entry_size = FLASH_JOURNAL_OVERHEAD + (uint32_t)len;
    if (entry_size > jrnl->sector_size) return FLASH_JOURNAL_ERR_FULL;

    /* If current sector doesn't have room, advance to next */
    if (jrnl->write_offset + entry_size > jrnl->sector_size) {
        uint32_t next = (jrnl->write_sector + 1) % jrnl->sector_count;

        /* If wrapping would overwrite oldest sector, subtract its entries */
        if (jrnl->entry_count > 0 && next == jrnl->oldest_sector) {
            uint32_t lost = count_sector_entries(jrnl, next, NULL, NULL);
            if (lost <= jrnl->entry_count) {
                jrnl->entry_count -= lost;
            }
            /* Oldest moves to sector after the one we're erasing */
            jrnl->oldest_sector = (next + 1) % jrnl->sector_count;
            jrnl->oldest_offset = 0;
        }

        flash_journal_err_t err = jrnl->storage->erase(jrnl->storage->ctx, next);
        if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;

        jrnl->write_sector = next;
        jrnl->write_offset = 0;
    }

    /* Build header */
    uint8_t hdr[FLASH_JOURNAL_HDR_SIZE];
    encode_u32(&hdr[0], FLASH_JOURNAL_MAGIC);
    encode_u32(&hdr[4], jrnl->next_seq);
    encode_u16(&hdr[8], len);

    /* Compute CRC over header + payload */
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < FLASH_JOURNAL_HDR_SIZE; i++) {
        crc = crc32_byte(crc, hdr[i]);
    }
    for (uint16_t i = 0; i < len; i++) {
        crc = crc32_byte(crc, data[i]);
    }
    crc ^= 0xFFFFFFFFu;

    uint8_t crc_bytes[FLASH_JOURNAL_CRC_SIZE];
    encode_u32(crc_bytes, crc);

    /* Write: header, payload, CRC */
    const flash_journal_storage_t *st = jrnl->storage;
    flash_journal_err_t err;

    err = st->write(st->ctx, jrnl->write_sector, jrnl->write_offset,
                    hdr, FLASH_JOURNAL_HDR_SIZE);
    if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;

    err = st->write(st->ctx, jrnl->write_sector,
                    jrnl->write_offset + FLASH_JOURNAL_HDR_SIZE,
                    data, len);
    if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;

    err = st->write(st->ctx, jrnl->write_sector,
                    jrnl->write_offset + FLASH_JOURNAL_HDR_SIZE + (uint32_t)len,
                    crc_bytes, FLASH_JOURNAL_CRC_SIZE);
    if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;

    jrnl->write_offset += entry_size;
    jrnl->next_seq++;
    jrnl->entry_count++;

    /* If this is the first entry, it's also the oldest */
    if (jrnl->entry_count == 1) {
        jrnl->oldest_sector = jrnl->write_sector;
        jrnl->oldest_offset = jrnl->write_offset - entry_size;
    }

    return FLASH_JOURNAL_OK;
}

flash_journal_err_t flash_journal_read_latest(const flash_journal_t *jrnl,
                                              uint8_t *buf, uint32_t *size)
{
    if (jrnl == NULL || buf == NULL || size == NULL)
        return FLASH_JOURNAL_ERR_PARAM;
    if (jrnl->entry_count == 0) return FLASH_JOURNAL_ERR_EMPTY;

    /* Find the last entry: scan the write sector backwards from write_offset */
    uint32_t sector = jrnl->write_sector;
    uint32_t offset = 0;
    uint32_t last_offset = 0;
    uint32_t seq;
    uint16_t len;
    bool found = false;

    while (offset + FLASH_JOURNAL_OVERHEAD <= jrnl->sector_size &&
           offset < jrnl->write_offset) {
        flash_journal_err_t err = read_entry(jrnl, sector, offset,
                                             &seq, &len, NULL, 0);
        if (err != FLASH_JOURNAL_OK) break;
        last_offset = offset;
        found = true;
        offset += FLASH_JOURNAL_OVERHEAD + (uint32_t)len;
    }

    if (!found) return FLASH_JOURNAL_ERR_EMPTY;

    flash_journal_err_t err = read_entry(jrnl, sector, last_offset,
                                         &seq, &len, buf, *size);
    if (err == FLASH_JOURNAL_OK) *size = (uint32_t)len;
    return err;
}

/*
 * Walk entries from oldest to find n-th (0-based).
 */
flash_journal_err_t flash_journal_read_nth(const flash_journal_t *jrnl,
                                           uint32_t n,
                                           uint8_t *buf, uint32_t *size)
{
    if (jrnl == NULL || buf == NULL || size == NULL)
        return FLASH_JOURNAL_ERR_PARAM;
    if (n >= jrnl->entry_count) return FLASH_JOURNAL_ERR_PARAM;

    uint32_t count = 0;
    uint32_t sector = jrnl->oldest_sector;

    for (uint32_t s = 0; s < jrnl->sector_count; s++) {
        uint32_t offset = 0;
        uint32_t seq;
        uint16_t len;

        while (offset + FLASH_JOURNAL_OVERHEAD <= jrnl->sector_size) {
            flash_journal_err_t err = read_entry(jrnl, sector, offset,
                                                 &seq, &len, NULL, 0);
            if (err != FLASH_JOURNAL_OK) break;

            if (count == n) {
                uint32_t entry_len = (uint32_t)len;
                flash_journal_err_t rc = read_entry(jrnl, sector, offset,
                                                     &seq, &len,
                                                     buf, entry_len);
                if (rc == FLASH_JOURNAL_OK) {
                    *size = entry_len;
                }
                return rc;
            }
            offset += FLASH_JOURNAL_OVERHEAD + (uint32_t)len;
            count++;
        }
        sector = (sector + 1) % jrnl->sector_count;
    }

    return FLASH_JOURNAL_ERR_PARAM;
}

uint32_t flash_journal_count(const flash_journal_t *jrnl)
{
    if (jrnl == NULL) return 0;
    return jrnl->entry_count;
}

flash_journal_err_t flash_journal_erase_all(flash_journal_t *jrnl)
{
    if (jrnl == NULL) return FLASH_JOURNAL_ERR_PARAM;

    for (uint32_t s = 0; s < jrnl->sector_count; s++) {
        flash_journal_err_t err = jrnl->storage->erase(jrnl->storage->ctx, s);
        if (err != FLASH_JOURNAL_OK) return FLASH_JOURNAL_ERR_STORAGE;
    }

    jrnl->write_sector = 0;
    jrnl->write_offset = 0;
    jrnl->next_seq = 1;
    jrnl->entry_count = 0;
    jrnl->oldest_sector = 0;
    jrnl->oldest_offset = 0;

    return FLASH_JOURNAL_OK;
}
