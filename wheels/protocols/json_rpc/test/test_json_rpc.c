// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "json_rpc.h"
#include <string.h>

/* ---------- build_request tests ---------- */

TEST(test_build_request_with_params)
{
    char buf[256];
    size_t len = 0;
    json_rpc_err_t err = json_rpc_build_request(buf, sizeof(buf),
        "add", "[1,2]", 1, &len);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_TRUE(len > 0);
    ASSERT_STR_EQ(buf,
        "{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"params\":[1,2],\"id\":1}");
}

TEST(test_build_request_no_params)
{
    char buf[256];
    size_t len = 0;
    json_rpc_err_t err = json_rpc_build_request(buf, sizeof(buf),
        "ping", NULL, 42, &len);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_STR_EQ(buf,
        "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":42}");
}

TEST(test_build_request_overflow)
{
    char buf[10];
    size_t len = 0;
    json_rpc_err_t err = json_rpc_build_request(buf, sizeof(buf),
        "method", NULL, 1, &len);
    ASSERT_EQ(err, JSON_RPC_ERR_OVERFLOW);
}

TEST(test_build_request_null_params)
{
    size_t len;
    ASSERT_EQ(json_rpc_build_request(NULL, 256, "m", NULL, 1, &len),
              JSON_RPC_ERR_PARAM);

    char buf[256];
    ASSERT_EQ(json_rpc_build_request(buf, sizeof(buf), NULL, NULL, 1, &len),
              JSON_RPC_ERR_PARAM);
    ASSERT_EQ(json_rpc_build_request(buf, sizeof(buf), "m", NULL, 1, NULL),
              JSON_RPC_ERR_PARAM);
}

/* ---------- build_response tests ---------- */

TEST(test_build_response)
{
    char buf[256];
    size_t len = 0;
    json_rpc_err_t err = json_rpc_build_response(buf, sizeof(buf),
        "3", 1, &len);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_STR_EQ(buf, "{\"jsonrpc\":\"2.0\",\"result\":3,\"id\":1}");
}

TEST(test_build_response_object)
{
    char buf[256];
    size_t len = 0;
    json_rpc_err_t err = json_rpc_build_response(buf, sizeof(buf),
        "{\"sum\":3}", 1, &len);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_STR_EQ(buf,
        "{\"jsonrpc\":\"2.0\",\"result\":{\"sum\":3},\"id\":1}");
}

TEST(test_build_response_null)
{
    size_t len;
    char buf[256];
    ASSERT_EQ(json_rpc_build_response(NULL, 256, "1", 1, &len),
              JSON_RPC_ERR_PARAM);
    ASSERT_EQ(json_rpc_build_response(buf, 256, NULL, 1, &len),
              JSON_RPC_ERR_PARAM);
    ASSERT_EQ(json_rpc_build_response(buf, 256, "1", 1, NULL),
              JSON_RPC_ERR_PARAM);
}

/* ---------- build_error tests ---------- */

TEST(test_build_error)
{
    char buf[256];
    size_t len = 0;
    json_rpc_err_t err = json_rpc_build_error(buf, sizeof(buf),
        JSON_RPC_ERR_METHOD_NOT_FOUND, "Method not found", 1, &len);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_STR_EQ(buf,
        "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"},\"id\":1}");
}

TEST(test_build_error_parse)
{
    char buf[256];
    size_t len = 0;
    json_rpc_err_t err = json_rpc_build_error(buf, sizeof(buf),
        JSON_RPC_ERR_PARSE, "Parse error", 0, &len);
    ASSERT_EQ(err, JSON_RPC_OK);
    /* Verify it contains the error code */
    ASSERT_TRUE(strstr(buf, "-32700") != NULL);
}

TEST(test_build_error_null)
{
    size_t len;
    char buf[256];
    ASSERT_EQ(json_rpc_build_error(NULL, 256, -32600, "msg", 1, &len),
              JSON_RPC_ERR_PARAM);
    ASSERT_EQ(json_rpc_build_error(buf, 256, -32600, NULL, 1, &len),
              JSON_RPC_ERR_PARAM);
    ASSERT_EQ(json_rpc_build_error(buf, 256, -32600, "msg", 1, NULL),
              JSON_RPC_ERR_PARAM);
}

/* ---------- validate tests ---------- */

TEST(test_validate_request)
{
    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"params\":[1,2],\"id\":1}";
    ASSERT_EQ(json_rpc_validate(req, strlen(req)), JSON_RPC_TYPE_REQUEST);
}

TEST(test_validate_response)
{
    const char *resp = "{\"jsonrpc\":\"2.0\",\"result\":3,\"id\":1}";
    ASSERT_EQ(json_rpc_validate(resp, strlen(resp)), JSON_RPC_TYPE_RESPONSE);
}

TEST(test_validate_error)
{
    const char *err = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,\"message\":\"Not found\"},\"id\":1}";
    ASSERT_EQ(json_rpc_validate(err, strlen(err)), JSON_RPC_TYPE_ERROR);
}

TEST(test_validate_invalid_no_version)
{
    const char *bad = "{\"method\":\"add\",\"id\":1}";
    ASSERT_EQ(json_rpc_validate(bad, strlen(bad)), JSON_RPC_TYPE_INVALID);
}

TEST(test_validate_invalid_wrong_version)
{
    const char *bad = "{\"jsonrpc\":\"1.0\",\"method\":\"add\",\"id\":1}";
    ASSERT_EQ(json_rpc_validate(bad, strlen(bad)), JSON_RPC_TYPE_INVALID);
}

TEST(test_validate_null)
{
    ASSERT_EQ(json_rpc_validate(NULL, 0), JSON_RPC_TYPE_INVALID);
    ASSERT_EQ(json_rpc_validate("", 0), JSON_RPC_TYPE_INVALID);
}

TEST(test_validate_empty_object)
{
    const char *obj = "{}";
    ASSERT_EQ(json_rpc_validate(obj, strlen(obj)), JSON_RPC_TYPE_INVALID);
}

/* ---------- extract_method tests ---------- */

TEST(test_extract_method)
{
    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"subtract\",\"params\":[42,23],\"id\":1}";
    size_t start = 0, mlen = 0;
    json_rpc_err_t err = json_rpc_extract_method(req, strlen(req), &start, &mlen);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_EQ(mlen, 8); /* "subtract" */
    ASSERT_TRUE(memcmp(req + start, "subtract", 8) == 0);
}

TEST(test_extract_method_not_found)
{
    const char *resp = "{\"jsonrpc\":\"2.0\",\"result\":19,\"id\":1}";
    size_t start, mlen;
    ASSERT_EQ(json_rpc_extract_method(resp, strlen(resp), &start, &mlen),
              JSON_RPC_ERR_PARAM);
}

TEST(test_extract_method_null)
{
    size_t start, mlen;
    ASSERT_EQ(json_rpc_extract_method(NULL, 10, &start, &mlen), JSON_RPC_ERR_PARAM);

    const char *req = "{\"method\":\"a\"}";
    ASSERT_EQ(json_rpc_extract_method(req, strlen(req), NULL, &mlen), JSON_RPC_ERR_PARAM);
    ASSERT_EQ(json_rpc_extract_method(req, strlen(req), &start, NULL), JSON_RPC_ERR_PARAM);
}

/* ---------- extract_id tests ---------- */

TEST(test_extract_id)
{
    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"id\":42}";
    int32_t id = 0;
    json_rpc_err_t err = json_rpc_extract_id(req, strlen(req), &id);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_EQ(id, 42);
}

TEST(test_extract_id_negative)
{
    /* Technically JSON-RPC ids should be positive, but we handle negative */
    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"x\",\"id\":-5}";
    int32_t id = 0;
    json_rpc_err_t err = json_rpc_extract_id(req, strlen(req), &id);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_EQ(id, -5);
}

TEST(test_extract_id_zero)
{
    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"x\",\"id\":0}";
    int32_t id = -1;
    json_rpc_err_t err = json_rpc_extract_id(req, strlen(req), &id);
    ASSERT_EQ(err, JSON_RPC_OK);
    ASSERT_EQ(id, 0);
}

TEST(test_extract_id_not_found)
{
    const char *req = "{\"jsonrpc\":\"2.0\",\"method\":\"x\"}";
    int32_t id;
    ASSERT_EQ(json_rpc_extract_id(req, strlen(req), &id), JSON_RPC_ERR_PARAM);
}

TEST(test_extract_id_null)
{
    int32_t id;
    ASSERT_EQ(json_rpc_extract_id(NULL, 10, &id), JSON_RPC_ERR_PARAM);

    const char *req = "{\"id\":1}";
    ASSERT_EQ(json_rpc_extract_id(req, strlen(req), NULL), JSON_RPC_ERR_PARAM);
}

/* ---------- round-trip tests ---------- */

TEST(test_roundtrip_request)
{
    char buf[256];
    size_t len = 0;
    json_rpc_build_request(buf, sizeof(buf), "multiply", "[3,4]", 7, &len);
    ASSERT_EQ(json_rpc_validate(buf, len), JSON_RPC_TYPE_REQUEST);

    size_t mstart, mlen;
    json_rpc_extract_method(buf, len, &mstart, &mlen);
    ASSERT_EQ(mlen, 8);
    ASSERT_TRUE(memcmp(buf + mstart, "multiply", 8) == 0);

    int32_t id;
    json_rpc_extract_id(buf, len, &id);
    ASSERT_EQ(id, 7);
}

TEST(test_roundtrip_error)
{
    char buf[256];
    size_t len = 0;
    json_rpc_build_error(buf, sizeof(buf),
        JSON_RPC_ERR_INTERNAL, "Internal error", 99, &len);
    ASSERT_EQ(json_rpc_validate(buf, len), JSON_RPC_TYPE_ERROR);

    int32_t id;
    json_rpc_extract_id(buf, len, &id);
    ASSERT_EQ(id, 99);
}

/* ---------- standard error codes ---------- */

TEST(test_standard_error_codes)
{
    ASSERT_EQ(JSON_RPC_ERR_PARSE, -32700);
    ASSERT_EQ(JSON_RPC_ERR_INVALID_REQ, -32600);
    ASSERT_EQ(JSON_RPC_ERR_METHOD_NOT_FOUND, -32601);
    ASSERT_EQ(JSON_RPC_ERR_INVALID_PARAMS, -32602);
    ASSERT_EQ(JSON_RPC_ERR_INTERNAL, -32603);
}

TEST(test_error_code_enum)
{
    ASSERT_EQ( 0, (int)JSON_RPC_OK);
    ASSERT_EQ(-1, (int)JSON_RPC_ERR_PARAM);
    ASSERT_EQ(-2, (int)JSON_RPC_ERR_OVERFLOW);
}

TEST(test_extract_id_limits)
{
    const char *max = "{\"id\":2147483647}";
    const char *min = "{\"id\":-2147483648}";
    int32_t id = 0;

    ASSERT_EQ(json_rpc_extract_id(max, strlen(max), &id), JSON_RPC_OK);
    ASSERT_EQ(id, 2147483647);
    ASSERT_EQ(json_rpc_extract_id(min, strlen(min), &id), JSON_RPC_OK);
    ASSERT_EQ(id, -2147483647 - 1);
}

TEST(test_extract_id_overflow)
{
    const char *over = "{\"id\":2147483648}";
    const char *under = "{\"id\":-2147483649}";
    const char *huge = "{\"id\":99999999999999999999}";
    int32_t id = 0;

    ASSERT_EQ(json_rpc_extract_id(over, strlen(over), &id), JSON_RPC_ERR_OVERFLOW);
    ASSERT_EQ(json_rpc_extract_id(under, strlen(under), &id), JSON_RPC_ERR_OVERFLOW);
    ASSERT_EQ(json_rpc_extract_id(huge, strlen(huge), &id), JSON_RPC_ERR_OVERFLOW);
}

TEST_MAIN("json_rpc")
    RUN(test_build_request_with_params);
    RUN(test_build_request_no_params);
    RUN(test_build_request_overflow);
    RUN(test_build_request_null_params);
    RUN(test_build_response);
    RUN(test_build_response_object);
    RUN(test_build_response_null);
    RUN(test_build_error);
    RUN(test_build_error_parse);
    RUN(test_build_error_null);
    RUN(test_validate_request);
    RUN(test_validate_response);
    RUN(test_validate_error);
    RUN(test_validate_invalid_no_version);
    RUN(test_validate_invalid_wrong_version);
    RUN(test_validate_null);
    RUN(test_validate_empty_object);
    RUN(test_extract_method);
    RUN(test_extract_method_not_found);
    RUN(test_extract_method_null);
    RUN(test_extract_id);
    RUN(test_extract_id_negative);
    RUN(test_extract_id_zero);
    RUN(test_extract_id_not_found);
    RUN(test_extract_id_null);
    RUN(test_roundtrip_request);
    RUN(test_roundtrip_error);
    RUN(test_standard_error_codes);
    RUN(test_error_code_enum);
    RUN(test_extract_id_limits);
    RUN(test_extract_id_overflow);
TEST_MAIN_END()
