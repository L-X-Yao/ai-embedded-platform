// SPDX-License-Identifier: MIT
/*
 * secure_ota -- integration recipe: secure firmware OTA update
 *
 * Demonstrates seven wheels working together:
 *   L0  aes128       -- encrypt/decrypt firmware chunks
 *   L0  sha256       -- hash firmware image
 *   L0  hmac_sha256  -- authenticate firmware
 *   L0  crc32        -- per-chunk integrity
 *   L3  boot_counter -- track boot count + uptime
 *   L3  ota_manager  -- OTA state machine
 *   L3  flash_journal-- log update events
 *
 * Runs entirely on host with simulated flash -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/secure_ota
 */

#include "test_harness.h"

/* wheels */
#include "aes128.h"
#include "sha256.h"
#include "hmac_sha256.h"
#include "crc32.h"
#include "boot_counter.h"
#include "ota_manager.h"
#include "flash_journal.h"

#include <string.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define SECTOR_COUNT     8
#define SECTOR_SIZE    512

/* Simulated firmware: 4 chunks of 256 bytes = 1024 bytes total */
#define FW_CHUNK_SIZE  256
#define FW_CHUNK_COUNT   4
#define FW_TOTAL_SIZE  (FW_CHUNK_SIZE * FW_CHUNK_COUNT)

/* ======================================================================
 * Mock flash storage
 *
 * Shared between flash_journal and ota_manager. Journal uses sectors
 * 0..3, OTA firmware uses sectors 4..7 (via separate callbacks).
 * ====================================================================== */

static uint8_t flash_sim[SECTOR_COUNT][SECTOR_SIZE];

static void flash_sim_reset(void)
{
    memset(flash_sim, 0xFF, sizeof(flash_sim));
}

/* --- flash_journal storage callbacks (sectors 0..3) --- */

static flash_journal_err_t jrnl_flash_read(void *ctx, uint32_t sector,
                                           uint32_t offset, uint8_t *buf,
                                           uint32_t len)
{
    (void)ctx;
    if (sector >= 4 || offset + len > SECTOR_SIZE) {
        return FLASH_JOURNAL_ERR_PARAM;
    }
    memcpy(buf, &flash_sim[sector][offset], len);
    return FLASH_JOURNAL_OK;
}

static flash_journal_err_t jrnl_flash_write(void *ctx, uint32_t sector,
                                            uint32_t offset,
                                            const uint8_t *data, uint32_t len)
{
    (void)ctx;
    if (sector >= 4 || offset + len > SECTOR_SIZE) {
        return FLASH_JOURNAL_ERR_PARAM;
    }
    memcpy(&flash_sim[sector][offset], data, len);
    return FLASH_JOURNAL_OK;
}

static flash_journal_err_t jrnl_flash_erase(void *ctx, uint32_t sector)
{
    (void)ctx;
    if (sector >= 4) {
        return FLASH_JOURNAL_ERR_PARAM;
    }
    memset(flash_sim[sector], 0xFF, SECTOR_SIZE);
    return FLASH_JOURNAL_OK;
}

static const flash_journal_storage_t jrnl_storage = {
    .read  = jrnl_flash_read,
    .write = jrnl_flash_write,
    .erase = jrnl_flash_erase,
    .ctx   = NULL,
};

/* ======================================================================
 * Mock boot_counter storage (flat byte array in sector 0, offset 0)
 *
 * Uses a dedicated 16-byte region; journal starts after init so they
 * do not collide in this simplified demo.
 * ====================================================================== */

static uint8_t bc_persist[sizeof(boot_counter_record_t) + 8];
static bool    bc_persist_valid = true; /* starts valid with zeroed data */

static int bc_storage_read(uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (!bc_persist_valid) {
        return -1;
    }
    memcpy(buf, bc_persist, size);
    return 0;
}

static int bc_storage_write(const uint8_t *buf, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (size > sizeof(bc_persist)) {
        return -1;
    }
    memcpy(bc_persist, buf, size);
    bc_persist_valid = true;
    return 0;
}

static const boot_counter_storage_t bc_storage = {
    .read     = bc_storage_read,
    .write    = bc_storage_write,
    .user_ctx = NULL,
};

/* ======================================================================
 * Simulated firmware image + crypto material
 *
 * We build a deterministic "firmware" from a seed pattern, then compute
 * its SHA256 hash and HMAC for verification. Each chunk is encrypted
 * with AES-128 ECB (block by block) and tagged with CRC32.
 * ====================================================================== */

static uint8_t fw_plaintext[FW_TOTAL_SIZE];
static uint8_t fw_encrypted[FW_TOTAL_SIZE];
static uint32_t fw_chunk_crc[FW_CHUNK_COUNT];
static uint8_t fw_sha256[SHA256_DIGEST_SIZE];
static uint8_t fw_hmac[HMAC_SHA256_DIGEST_SIZE];

static const uint8_t aes_key[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

static const uint8_t hmac_key[16] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

static aes128_ctx_t aes_ctx;

static void prepare_firmware(void)
{
    /* Fill plaintext with deterministic pattern */
    for (uint32_t i = 0; i < FW_TOTAL_SIZE; i++) {
        fw_plaintext[i] = (uint8_t)((i * 7 + 0xAB) & 0xFF);
    }

    /* Compute SHA256 of plaintext */
    sha256_hash(fw_plaintext, FW_TOTAL_SIZE, fw_sha256);

    /* Compute HMAC-SHA256 of plaintext */
    hmac_sha256_compute(hmac_key, sizeof(hmac_key),
                        fw_plaintext, FW_TOTAL_SIZE, fw_hmac);

    /* Encrypt each 16-byte block with AES-128 ECB */
    aes128_err_t err = aes128_init(&aes_ctx, aes_key);
    (void)err; /* known-good key, cannot fail */
    for (uint32_t i = 0; i < FW_TOTAL_SIZE; i += 16) {
        (void)aes128_encrypt_block(&aes_ctx, &fw_plaintext[i], &fw_encrypted[i]);
    }

    /* Compute per-chunk CRC32 over the *encrypted* data */
    for (uint32_t c = 0; c < FW_CHUNK_COUNT; c++) {
        fw_chunk_crc[c] = crc32_compute(&fw_encrypted[c * FW_CHUNK_SIZE],
                                        FW_CHUNK_SIZE);
    }
}

/* ======================================================================
 * OTA transport mock -- simulates downloading encrypted firmware
 *
 * download_chunk returns sequential encrypted chunks with CRC32.
 * ====================================================================== */

static uint32_t ota_download_offset;
static uint8_t  ota_fw_area[FW_TOTAL_SIZE]; /* "OTA flash partition" */

static ota_mgr_err_t mock_check_version(void *ctx, const char *current_ver,
                                        bool *update_available,
                                        uint32_t *new_size)
{
    (void)ctx; (void)current_ver;
    *update_available = true;
    *new_size = FW_TOTAL_SIZE;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_download_chunk(void *ctx, uint32_t offset,
                                         uint8_t *buf, uint16_t buf_size,
                                         uint16_t *bytes_read)
{
    (void)ctx;
    if (offset >= FW_TOTAL_SIZE) {
        *bytes_read = 0;
        return OTA_MGR_OK;
    }
    uint32_t remaining = FW_TOTAL_SIZE - offset;
    uint16_t to_copy = (remaining < buf_size) ? (uint16_t)remaining : buf_size;
    memcpy(buf, &fw_encrypted[offset], to_copy);
    *bytes_read = to_copy;
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_write_chunk(void *ctx, uint32_t offset,
                                      const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (offset + len > FW_TOTAL_SIZE) {
        return OTA_MGR_ERR_WRITE;
    }
    memcpy(&ota_fw_area[offset], data, len);
    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_verify(void *ctx, uint32_t total_size)
{
    (void)ctx;
    /*
     * Real verify: decrypt the firmware in-place, then check SHA256 + HMAC.
     * We do the full crypto pipeline here to demonstrate the wheels.
     */

    /* Step 1: Decrypt all blocks in ota_fw_area */
    uint8_t decrypted[FW_TOTAL_SIZE];
    for (uint32_t i = 0; i < total_size; i += 16) {
        aes128_err_t err = aes128_decrypt_block(&aes_ctx,
                                                &ota_fw_area[i],
                                                &decrypted[i]);
        if (err != AES128_OK) {
            return OTA_MGR_ERR_VERIFY;
        }
    }

    /* Step 2: Verify SHA256 hash */
    uint8_t computed_hash[SHA256_DIGEST_SIZE];
    sha256_hash(decrypted, total_size, computed_hash);
    if (memcmp(computed_hash, fw_sha256, SHA256_DIGEST_SIZE) != 0) {
        return OTA_MGR_ERR_VERIFY;
    }

    /* Step 3: Verify HMAC-SHA256 */
    uint8_t computed_hmac[HMAC_SHA256_DIGEST_SIZE];
    hmac_sha256_compute(hmac_key, sizeof(hmac_key),
                        decrypted, total_size, computed_hmac);
    if (!hmac_sha256_verify(computed_hmac, fw_hmac)) {
        return OTA_MGR_ERR_VERIFY;
    }

    return OTA_MGR_OK;
}

static ota_mgr_err_t mock_activate(void *ctx)
{
    (void)ctx;
    return OTA_MGR_OK;
}

static const ota_mgr_ops_t ota_ops = {
    .check_version  = mock_check_version,
    .download_chunk = mock_download_chunk,
    .write_chunk    = mock_write_chunk,
    .verify         = mock_verify,
    .activate       = mock_activate,
    .ctx            = NULL,
};

/* ======================================================================
 * Global instances
 * ====================================================================== */

static boot_counter_t   bc;
static flash_journal_t  jrnl;
static ota_manager_t    ota;

/* ======================================================================
 * Tests -- verify the secure OTA integration end-to-end
 * ====================================================================== */

TEST(test_init_all)
{
    /* Prepare simulation environment */
    flash_sim_reset();
    memset(bc_persist, 0, sizeof(bc_persist));
    bc_persist_valid = true;  /* zeroed record = valid first-boot state */
    memset(ota_fw_area, 0, sizeof(ota_fw_area));
    ota_download_offset = 0;
    prepare_firmware();

    /* 1. aes128 -- already init'd in prepare_firmware */
    ASSERT_EQ(aes128_init(&aes_ctx, aes_key), AES128_OK);

    /* 2. sha256 -- stateless one-shot, no init needed beyond use */
    uint8_t test_hash[SHA256_DIGEST_SIZE];
    sha256_hash((const uint8_t *)"x", 1, test_hash);
    ASSERT_TRUE(test_hash[0] != 0 || test_hash[1] != 0);

    /* 3. hmac_sha256 -- stateless one-shot, verify computes */
    uint8_t test_mac[HMAC_SHA256_DIGEST_SIZE];
    hmac_sha256_compute(hmac_key, sizeof(hmac_key),
                        (const uint8_t *)"x", 1, test_mac);
    ASSERT_TRUE(test_mac[0] != 0 || test_mac[1] != 0);

    /* 4. crc32 -- stateless, verify known value */
    uint32_t test_crc = crc32_compute((const uint8_t *)"123456789", 9);
    ASSERT_EQ_HEX(test_crc, 0xCBF43926u);

    /* 5. boot_counter */
    boot_counter_err_t bc_err = boot_counter_init(&bc, &bc_storage);
    ASSERT_EQ(bc_err, BOOT_COUNTER_OK);

    /* 6. flash_journal (sectors 0..3) */
    flash_journal_err_t fj_err = flash_journal_init(&jrnl, &jrnl_storage,
                                                    4, SECTOR_SIZE);
    ASSERT_EQ(fj_err, FLASH_JOURNAL_OK);

    /* 7. ota_manager */
    ota_mgr_err_t ota_err = ota_manager_init(&ota, &ota_ops, "1.0.0");
    ASSERT_EQ(ota_err, OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&ota), OTA_MGR_STATE_IDLE);

    printf("    all 7 wheels initialized successfully\n");
}

TEST(test_boot_counter_lifecycle)
{
    /* First boot: load succeeds with zeroed record (count=0) */
    boot_counter_err_t err = boot_counter_load(&bc);
    ASSERT_EQ(err, BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&bc), 0);

    err = boot_counter_increment(&bc);
    ASSERT_EQ(err, BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&bc), 1);

    /* Simulate second boot: reload and increment again */
    boot_counter_t bc2;
    boot_counter_err_t err2 = boot_counter_init(&bc2, &bc_storage);
    ASSERT_EQ(err2, BOOT_COUNTER_OK);
    err2 = boot_counter_load(&bc2);
    ASSERT_EQ(err2, BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&bc2), 1);

    err2 = boot_counter_increment(&bc2);
    ASSERT_EQ(err2, BOOT_COUNTER_OK);
    ASSERT_EQ(boot_counter_get_count(&bc2), 2);

    /* Tick uptime a few times */
    boot_counter_tick_second(&bc);
    boot_counter_tick_second(&bc);
    boot_counter_tick_second(&bc);
    ASSERT_EQ(boot_counter_get_uptime_seconds(&bc), 3);

    printf("    boot_counter: count=%u, uptime=%us\n",
           (unsigned)boot_counter_get_count(&bc2),
           (unsigned)boot_counter_get_uptime_seconds(&bc));
}

TEST(test_journal_boot_event)
{
    /* Log a "boot" event */
    const char *boot_msg = "boot:v1.0.0";
    flash_journal_err_t err = flash_journal_append(&jrnl,
                                                   (const uint8_t *)boot_msg,
                                                   (uint16_t)strlen(boot_msg));
    ASSERT_EQ(err, FLASH_JOURNAL_OK);
    ASSERT_EQ(flash_journal_count(&jrnl), 1);

    /* Read it back */
    uint8_t buf[128];
    uint32_t size = sizeof(buf);
    err = flash_journal_read_latest(&jrnl, buf, &size);
    ASSERT_EQ(err, FLASH_JOURNAL_OK);
    ASSERT_EQ(size, strlen(boot_msg));
    ASSERT_TRUE(memcmp(buf, boot_msg, size) == 0);

    printf("    journal entry 0: \"%.*s\"\n", (int)size, buf);
}

TEST(test_aes_encrypt_decrypt_round_trip)
{
    uint8_t plain[16] = {
        0x32, 0x43, 0xF6, 0xA8, 0x88, 0x5A, 0x30, 0x8D,
        0x31, 0x31, 0x98, 0xA2, 0xE0, 0x37, 0x07, 0x34
    };
    uint8_t cipher[16];
    uint8_t recovered[16];

    aes128_err_t err = aes128_encrypt_block(&aes_ctx, plain, cipher);
    ASSERT_EQ(err, AES128_OK);

    /* Ciphertext must differ from plaintext */
    ASSERT_TRUE(memcmp(plain, cipher, 16) != 0);

    err = aes128_decrypt_block(&aes_ctx, cipher, recovered);
    ASSERT_EQ(err, AES128_OK);

    /* Recovered must match original */
    ASSERT_MEMEQ_N(recovered, plain, 16);

    printf("    AES-128 round-trip OK (16 bytes)\n");
}

TEST(test_sha256_firmware_hash)
{
    /* Hash the simulated firmware and compare with pre-computed value */
    uint8_t hash[SHA256_DIGEST_SIZE];
    sha256_hash(fw_plaintext, FW_TOTAL_SIZE, hash);

    ASSERT_MEMEQ_N(hash, fw_sha256, SHA256_DIGEST_SIZE);

    /* Also test streaming interface */
    sha256_ctx_t sha_ctx;
    sha256_err_t err = sha256_init(&sha_ctx);
    ASSERT_EQ(err, SHA256_OK);

    /* Feed in 4 chunks */
    for (uint32_t i = 0; i < FW_CHUNK_COUNT; i++) {
        err = sha256_update(&sha_ctx,
                            &fw_plaintext[i * FW_CHUNK_SIZE],
                            FW_CHUNK_SIZE);
        ASSERT_EQ(err, SHA256_OK);
    }

    uint8_t stream_hash[SHA256_DIGEST_SIZE];
    err = sha256_final(&sha_ctx, stream_hash);
    ASSERT_EQ(err, SHA256_OK);

    /* Streaming hash must match one-shot hash */
    ASSERT_MEMEQ_N(stream_hash, fw_sha256, SHA256_DIGEST_SIZE);

    printf("    SHA-256 firmware hash verified (one-shot + streaming)\n");
}

TEST(test_hmac_verify)
{
    /* Compute HMAC-SHA256 of firmware and verify against pre-computed */
    uint8_t mac[HMAC_SHA256_DIGEST_SIZE];
    hmac_sha256_compute(hmac_key, sizeof(hmac_key),
                        fw_plaintext, FW_TOTAL_SIZE, mac);

    ASSERT_TRUE(hmac_sha256_verify(mac, fw_hmac));

    /* Tampered data must NOT verify */
    uint8_t tampered[FW_TOTAL_SIZE];
    memcpy(tampered, fw_plaintext, FW_TOTAL_SIZE);
    tampered[0] ^= 0x01; /* flip one bit */

    uint8_t bad_mac[HMAC_SHA256_DIGEST_SIZE];
    hmac_sha256_compute(hmac_key, sizeof(hmac_key),
                        tampered, FW_TOTAL_SIZE, bad_mac);

    ASSERT_FALSE(hmac_sha256_verify(bad_mac, fw_hmac));

    printf("    HMAC-SHA256 verify: authentic=OK, tampered=rejected\n");
}

TEST(test_full_ota_flow)
{
    /* Step 1: Check for update */
    ota_mgr_err_t err = ota_manager_check(&ota);
    ASSERT_EQ(err, OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&ota), OTA_MGR_STATE_DOWNLOADING);

    /* Step 2: Download all chunks, verify per-chunk CRC32 */
    uint8_t chunk_buf[FW_CHUNK_SIZE];
    uint32_t chunks_downloaded = 0;

    while (ota_manager_get_state(&ota) == OTA_MGR_STATE_DOWNLOADING) {
        uint32_t offset_before = ota.downloaded;
        err = ota_manager_download_step(&ota, chunk_buf,
                                        (uint16_t)sizeof(chunk_buf));
        ASSERT_EQ(err, OTA_MGR_OK);

        /* Verify CRC32 of the downloaded chunk */
        if (ota.downloaded > offset_before) {
            uint32_t chunk_idx = offset_before / FW_CHUNK_SIZE;
            uint16_t chunk_len = (uint16_t)(ota.downloaded - offset_before);
            uint32_t crc = crc32_compute(chunk_buf, chunk_len);
            ASSERT_EQ_HEX(crc, fw_chunk_crc[chunk_idx]);
            chunks_downloaded++;
        }
    }

    ASSERT_EQ(chunks_downloaded, FW_CHUNK_COUNT);
    ASSERT_EQ(ota_manager_get_state(&ota), OTA_MGR_STATE_VERIFYING);
    ASSERT_EQ(ota_manager_get_progress(&ota), 100);

    printf("    downloaded %u chunks, all CRC32 verified\n",
           (unsigned)chunks_downloaded);

    /* Step 3: Verify firmware (decrypt + SHA256 + HMAC inside mock_verify) */
    err = ota_manager_verify(&ota);
    ASSERT_EQ(err, OTA_MGR_OK);
    ASSERT_EQ(ota_manager_get_state(&ota), OTA_MGR_STATE_READY);

    printf("    firmware verified: SHA-256 + HMAC-SHA256 OK\n");

    /* Step 4: Activate */
    err = ota_manager_activate(&ota);
    ASSERT_EQ(err, OTA_MGR_OK);

    printf("    OTA complete: idle -> check -> download -> verify -> activate\n");
}

TEST(test_journal_read_back)
{
    /* Log an OTA completion event */
    const char *ota_msg = "ota_complete:v2.0.0";
    flash_journal_err_t err = flash_journal_append(&jrnl,
                                                   (const uint8_t *)ota_msg,
                                                   (uint16_t)strlen(ota_msg));
    ASSERT_EQ(err, FLASH_JOURNAL_OK);

    /* We should now have 2 entries: boot + ota_complete */
    ASSERT_EQ(flash_journal_count(&jrnl), 2);

    /* Read entry 0 (oldest = boot event) */
    uint8_t buf[128];
    uint32_t size = sizeof(buf);
    err = flash_journal_read_nth(&jrnl, 0, buf, &size);
    ASSERT_EQ(err, FLASH_JOURNAL_OK);
    ASSERT_TRUE(memcmp(buf, "boot:", 5) == 0);

    printf("    entry 0: \"%.*s\"\n", (int)size, buf);

    /* Read entry 1 (newest = ota_complete) */
    size = sizeof(buf);
    err = flash_journal_read_nth(&jrnl, 1, buf, &size);
    ASSERT_EQ(err, FLASH_JOURNAL_OK);
    ASSERT_TRUE(memcmp(buf, "ota_complete:", 13) == 0);

    printf("    entry 1: \"%.*s\"\n", (int)size, buf);

    /* Also verify via read_latest */
    size = sizeof(buf);
    err = flash_journal_read_latest(&jrnl, buf, &size);
    ASSERT_EQ(err, FLASH_JOURNAL_OK);
    ASSERT_EQ(size, strlen(ota_msg));
    ASSERT_TRUE(memcmp(buf, ota_msg, size) == 0);

    printf("    journal integrity verified: %u entries\n",
           (unsigned)flash_journal_count(&jrnl));

    /* Cleanup: zeroize AES key material */
    aes128_clear(&aes_ctx);
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("secure_ota integration")
    RUN(test_init_all);
    RUN(test_boot_counter_lifecycle);
    RUN(test_journal_boot_event);
    RUN(test_aes_encrypt_decrypt_round_trip);
    RUN(test_sha256_firmware_hash);
    RUN(test_hmac_verify);
    RUN(test_full_ota_flow);
    RUN(test_journal_read_back);
TEST_MAIN_END()
