#include "test_harness.h"
#include "j1939.h"

/* ---------- decode ---------- */

TEST(test_decode_pdu2_broadcast)
{
    /* PGN 0xFEF1 (Engine Fluid Level/Pressure), priority 6, src 0x00 */
    /* CAN ID: pri=6 dp=0 PF=0xFE PS=0xF1 SA=0x00 */
    uint32_t can_id = (6u << 26) | (0xFEu << 16) | (0xF1u << 8) | 0x00;
    j1939_id_t id;
    ASSERT_EQ(j1939_decode_id(can_id, &id), J1939_OK);
    ASSERT_EQ(id.priority, 6);
    ASSERT_EQ(id.pgn, 0xFEF1u);
    ASSERT_EQ(id.source, 0x00);
    ASSERT_FALSE(id.pdu1);
    ASSERT_EQ(id.destination, J1939_ADDR_GLOBAL);
}

TEST(test_decode_pdu1_peer_to_peer)
{
    /* PGN 0xEA00 (Request), PF=0xEA (<240), dest=0x00, src=0xFE */
    uint32_t can_id = (6u << 26) | (0xEAu << 16) | (0x00u << 8) | 0xFE;
    j1939_id_t id;
    ASSERT_EQ(j1939_decode_id(can_id, &id), J1939_OK);
    ASSERT_EQ(id.pgn, 0xEA00u);
    ASSERT_TRUE(id.pdu1);
    ASSERT_EQ(id.destination, 0x00);
    ASSERT_EQ(id.source, 0xFE);
}

TEST(test_decode_with_data_page)
{
    /* Data page = 1, PF=0xFF, PS=0x00, SA=0x01 */
    uint32_t can_id = (3u << 26) | (1u << 24) | (0xFFu << 16) | (0x00u << 8) | 0x01;
    j1939_id_t id;
    j1939_decode_id(can_id, &id);
    ASSERT_EQ(id.pgn, 0x1FF00u);
    ASSERT_EQ(id.priority, 3);
}

TEST(test_decode_null)
{
    ASSERT_EQ(j1939_decode_id(0, NULL), J1939_ERR_PARAM);
}

TEST(test_decode_invalid_id)
{
    j1939_id_t id;
    ASSERT_EQ(j1939_decode_id(0x20000000, &id), J1939_ERR_FORMAT);
}

/* ---------- encode ---------- */

TEST(test_encode_pdu2)
{
    j1939_id_t id = { .priority = 6, .pgn = 0xFEF1, .source = 0x00, .destination = 0xFF };
    uint32_t can_id = 0;
    ASSERT_EQ(j1939_encode_id(&id, &can_id), J1939_OK);
    ASSERT_EQ(can_id, (6u << 26) | (0xFEu << 16) | (0xF1u << 8) | 0x00);
}

TEST(test_encode_pdu1)
{
    j1939_id_t id = { .priority = 6, .pgn = 0xEA00, .source = 0xFE, .destination = 0x00 };
    uint32_t can_id = 0;
    ASSERT_EQ(j1939_encode_id(&id, &can_id), J1939_OK);
    ASSERT_EQ(can_id, (6u << 26) | (0xEAu << 16) | (0x00u << 8) | 0xFE);
}

TEST(test_encode_null)
{
    uint32_t can_id;
    ASSERT_EQ(j1939_encode_id(NULL, &can_id), J1939_ERR_PARAM);
    j1939_id_t id = {0};
    ASSERT_EQ(j1939_encode_id(&id, NULL), J1939_ERR_PARAM);
}

TEST(test_encode_bad_priority)
{
    j1939_id_t id = { .priority = 8 };
    uint32_t can_id;
    ASSERT_EQ(j1939_encode_id(&id, &can_id), J1939_ERR_PARAM);
}

/* ---------- roundtrip ---------- */

TEST(test_roundtrip_pdu2)
{
    j1939_id_t src = { .priority = 3, .pgn = 0xFECA, .source = 0x21, .destination = 0xFF };
    uint32_t can_id = 0;
    j1939_encode_id(&src, &can_id);
    j1939_id_t dst;
    j1939_decode_id(can_id, &dst);
    ASSERT_EQ(dst.priority, 3);
    ASSERT_EQ(dst.pgn, 0xFECAu);
    ASSERT_EQ(dst.source, 0x21);
}

TEST(test_roundtrip_pdu1)
{
    j1939_id_t src = { .priority = 6, .pgn = 0xEA00, .source = 0x01, .destination = 0x00 };
    uint32_t can_id = 0;
    j1939_encode_id(&src, &can_id);
    j1939_id_t dst;
    j1939_decode_id(can_id, &dst);
    ASSERT_EQ(dst.pgn, 0xEA00u);
    ASSERT_EQ(dst.destination, 0x00);
    ASSERT_EQ(dst.source, 0x01);
}

/* ---------- helpers ---------- */

TEST(test_is_pdu1)
{
    ASSERT_TRUE(j1939_is_pdu1(0xEA00));
    ASSERT_TRUE(j1939_is_pdu1(0x0000));
    ASSERT_FALSE(j1939_is_pdu1(0xFEF1));
    ASSERT_FALSE(j1939_is_pdu1(0xFFFF));
}

TEST(test_pgn_from_id)
{
    uint32_t can_id = (6u << 26) | (0xFEu << 16) | (0xF1u << 8);
    ASSERT_EQ(j1939_pgn_from_id(can_id), 0xFEF1u);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)J1939_OK, 0);
    ASSERT_EQ((int)J1939_ERR_PARAM, -1);
    ASSERT_EQ((int)J1939_ERR_FORMAT, -2);
}

TEST_MAIN("j1939")
    RUN(test_decode_pdu2_broadcast);
    RUN(test_decode_pdu1_peer_to_peer);
    RUN(test_decode_with_data_page);
    RUN(test_decode_null);
    RUN(test_decode_invalid_id);
    RUN(test_encode_pdu2);
    RUN(test_encode_pdu1);
    RUN(test_encode_null);
    RUN(test_encode_bad_priority);
    RUN(test_roundtrip_pdu2);
    RUN(test_roundtrip_pdu1);
    RUN(test_is_pdu1);
    RUN(test_pgn_from_id);
    RUN(test_error_codes);
TEST_MAIN_END()
