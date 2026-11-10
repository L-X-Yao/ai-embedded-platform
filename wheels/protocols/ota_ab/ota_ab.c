// SPDX-License-Identifier: MIT
/*
 * ota_ab — A/B firmware slot manager with rollback
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      flash_hal
 * Instances: multi
 */

#include "ota_ab.h"
#include "contract.h"
#include <string.h>

/* ---------- CRC32 (IEEE 802.3, no lookup table) ---------- */

uint32_t ota_ab_crc32(const uint8_t *data, uint32_t len)
{
    if (!data) return 0;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

/* ---------- params helpers ---------- */

static uint32_t params_crc(const ota_ab_params_t *p)
{
    return ota_ab_crc32((const uint8_t *)p,
                        sizeof(*p) - sizeof(p->crc32));
}

static bool params_valid(const ota_ab_params_t *p)
{
    if (p->magic != OTA_AB_PARAMS_MAGIC) return false;
    if (p->crc32 != params_crc(p)) return false;
    if (p->active_slot > 1) return false;
    return true;
}

/* seq counts up and wraps, and the two copies stay within one of each other. */
static bool seq_is_newer(uint8_t seq, uint8_t than)
{
    uint8_t ahead = (uint8_t)(seq - than);
    return ahead != 0 && ahead < 128u;
}

static ota_ab_err_t params_read(ota_ab_t *m)
{
    ota_ab_params_t second;
    bool second_ok = false;

    if (!m->flash->read(m->flash->ctx, m->layout.params_addr,
                        (uint8_t *)&m->params, sizeof(m->params)))
        return OTA_AB_ERR_FLASH;
    bool first_ok = params_valid(&m->params);

    if (m->layout.params_addr2 != 0) {
        if (!m->flash->read(m->flash->ctx, m->layout.params_addr2,
                            (uint8_t *)&second, sizeof(second)))
            return OTA_AB_ERR_FLASH;
        second_ok = params_valid(&second);
    }

    m->stored_in_2 = second_ok &&
                     (!first_ok || seq_is_newer(second.seq, m->params.seq));
    if (m->stored_in_2) {
        m->params = second;
    } else if (!first_ok) {
        memset(&m->params, 0, sizeof(m->params));
        m->params.magic = OTA_AB_PARAMS_MAGIC;
        m->params.active_slot = 0;
        m->params.ota_flag = OTA_AB_FLAG_IDLE;
        m->params.crc32 = params_crc(&m->params);
    }
    m->stored_seq = m->params.seq;
    return OTA_AB_OK;
}

static ota_ab_err_t params_write(ota_ab_t *m)
{
    uint32_t addr = m->layout.params_addr;

    /* Never touch the copy that holds the newest good record. A retry after
     * a failed write reuses the same seq. */
    if (m->layout.params_addr2 != 0) {
        if (!m->stored_in_2) addr = m->layout.params_addr2;
        m->params.seq = (uint8_t)(m->stored_seq + 1u);
    }

    m->params.crc32 = params_crc(&m->params);
    if (!m->flash->erase(m->flash->ctx, addr, m->layout.params_size))
        return OTA_AB_ERR_FLASH;
    if (!m->flash->write(m->flash->ctx, addr,
                         (const uint8_t *)&m->params, sizeof(m->params)))
        return OTA_AB_ERR_FLASH;

    if (m->layout.params_addr2 != 0) {
        m->stored_in_2 = !m->stored_in_2;
        m->stored_seq = m->params.seq;
    }
    return OTA_AB_OK;
}

/* Erasing one region must not reach into the other. */
static bool params_regions_overlap(const ota_ab_layout_t *l)
{
    uint32_t a = l->params_addr;
    uint32_t b = l->params_addr2;
    return ((a < b) ? b - a : a - b) < l->params_size;
}

/* ---------- API ---------- */

ota_ab_err_t ota_ab_init(ota_ab_t *m, const ota_ab_flash_t *flash,
                         const ota_ab_layout_t *layout)
{
    if (!m || !flash || !layout) return OTA_AB_ERR_NULL;
    if (!flash->erase || !flash->write || !flash->read) return OTA_AB_ERR_NULL;
    /* Rollback needs boot_count > max, and boot_count stops at 255. */
    if (layout->max_boot_count == 0 || layout->max_boot_count == 255)
        return OTA_AB_ERR_PARAM;
    if (layout->params_size < sizeof(ota_ab_params_t)) return OTA_AB_ERR_PARAM;
    if (layout->params_addr2 != 0 && params_regions_overlap(layout))
        return OTA_AB_ERR_PARAM;
    m->flash = flash;
    m->layout = *layout;
    ota_ab_err_t err = params_read(m);
    if (err == OTA_AB_OK) {
        ENSURE(m->params.active_slot <= 1);
    }
    return err;
}

uint8_t ota_ab_active_slot(const ota_ab_t *m)
{
    if (!m) return 0;
    return m->params.active_slot;
}

uint8_t ota_ab_target_slot(const ota_ab_t *m)
{
    if (!m) return 1;
    return 1u - m->params.active_slot;
}

uint32_t ota_ab_slot_addr(const ota_ab_t *m, uint8_t slot)
{
    if (!m) return 0;
    return (slot == 0) ? m->layout.slot_a_addr : m->layout.slot_b_addr;
}

ota_ab_err_t ota_ab_mark_pending(ota_ab_t *m, uint32_t fw_size, uint32_t fw_crc)
{
    if (!m) return OTA_AB_ERR_NULL;
    if (fw_size == 0 || fw_size > m->layout.slot_max_size)
        return OTA_AB_ERR_SIZE;

    m->params.ota_flag = OTA_AB_FLAG_PENDING;
    m->params.fw_size = fw_size;
    m->params.fw_crc = fw_crc;
    m->params.boot_count = 0;
    return params_write(m);
}

static ota_ab_err_t verify_firmware_crc(ota_ab_t *m, uint8_t slot)
{
    /* A record with a good CRC can still describe no image a slot can hold. */
    if (m->params.fw_size == 0 || m->params.fw_size > m->layout.slot_max_size)
        return OTA_AB_ERR_SIZE;

    uint32_t base = (slot == 0) ? m->layout.slot_a_addr : m->layout.slot_b_addr;
    uint32_t remaining = m->params.fw_size;
    uint32_t offset = 0;
    uint32_t crc = 0xFFFFFFFFu;
    uint8_t buf[64];

    while (remaining > 0) {
        uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        if (!m->flash->read(m->flash->ctx, base + offset, buf, chunk))
            return OTA_AB_ERR_FLASH;
        for (uint32_t i = 0; i < chunk; i++) {
            crc ^= buf[i];
            for (int j = 0; j < 8; j++) {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320u;
                else
                    crc >>= 1;
            }
        }
        offset += chunk;
        remaining -= chunk;
    }
    crc = ~crc;
    if (crc != m->params.fw_crc)
        return OTA_AB_ERR_CRC;
    return OTA_AB_OK;
}

ota_ab_err_t ota_ab_boot_event(ota_ab_t *m)
{
    if (!m) return OTA_AB_ERR_NULL;

    if (m->params.ota_flag == OTA_AB_FLAG_PENDING) {
        uint8_t target = 1u - m->params.active_slot;
        ota_ab_err_t err = verify_firmware_crc(m, target);
        if (err != OTA_AB_OK)
            return err;
        m->params.active_slot = target;
        m->params.ota_flag = OTA_AB_FLAG_SUCCESS;
        m->params.boot_count = 1;
        return params_write(m);
    }

    if (m->params.ota_flag == OTA_AB_FLAG_SUCCESS) {
        if (m->params.boot_count < 255) m->params.boot_count++;
        if (m->params.boot_count > m->layout.max_boot_count) {
            m->params.active_slot = 1u - m->params.active_slot;
            m->params.ota_flag = OTA_AB_FLAG_IDLE;
            m->params.boot_count = 0;
            return params_write(m);
        }
        return params_write(m);
    }

    return OTA_AB_OK;
}

ota_ab_err_t ota_ab_confirm(ota_ab_t *m)
{
    if (!m) return OTA_AB_ERR_NULL;
    if (m->params.ota_flag != OTA_AB_FLAG_SUCCESS)
        return OTA_AB_OK;

    m->params.ota_flag = OTA_AB_FLAG_IDLE;
    m->params.boot_count = 0;
    return params_write(m);
}
