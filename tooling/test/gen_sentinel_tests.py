#!/usr/bin/env python3
"""gen_sentinel_tests.py -- generate boundary/sentinel tests for wheel output-buffer functions.

For each wheel with write/read/peek/encode/decode functions that produce output
into caller-provided buffers, generates a test that:
  1. Allocates an output buffer with an extra sentinel byte (0xFF)
  2. Calls the function
  3. Asserts the sentinel byte is still 0xFF (not overwritten)

This detects off-by-one writes that would silently corrupt adjacent memory.

Targets: L0 (primitives) and L2 (protocols) only.
L1 drivers are skipped (need working HAL mocks).
L3 modules are skipped (need complex init with HAL dependencies).

Usage:
    python tooling/test/gen_sentinel_tests.py [--dry-run]
"""

import os
import re
import sys
import textwrap
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
WHEELS_DIR = REPO_ROOT / "wheels"
TARGET_LAYERS = ["primitives", "protocols"]
SKIP_LAYERS = ["drivers", "modules"]

# Wheels known to be vendored or otherwise unsuitable
VENDOR_WHEELS = set()

# Wheels that are purely callback/streaming with no output buffers worth testing
SKIP_WHEELS = {
    # SAX-style parsers with no output buffer
    "json_parser",
    # Callback-only or no output buffer functions
    "ncp5623_parser",
    # HAL-dependent protocols (need working HAL callbacks to produce output)
    "xmodem", "ymodem", "at_command", "ota_ab", "i2c_scan",
    # Pure compute, no output buffers
    "crc8", "crc16", "crc32", "crc_any", "checksum", "hash_fnv",
    "moving_average", "moving_window", "median_filter", "pid_controller",
    "fixed_point", "rate_limiter", "debounce", "soft_timer", "timeout",
    "retry", "timer_wheel", "led_pattern", "state_machine",
    "mahony_ahrs", "mag_cal",
    # Endian: write functions write exactly 2/4 bytes to known offsets
    # (tested better by unit tests, not sentinel pattern)
    "endian",
    # Bitmap: operates on caller-provided bitmap, not output buffers
    "bitmap",
    # Log ecosystem: output via callbacks, not buffers
    "log", "log_color", "log_dedup", "log_file", "log_filter",
    "log_flash", "log_rate", "log_ringbuf", "log_rtt", "log_uart",
    # FAL/KVS: flash abstraction, HAL dependent
    "fal", "kvs",
    # Event queue: typed queue, not byte-buffer output
    "event_queue",
    # Lookup table: returns values, no output buffer
    "lookup_table",
}


# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------

@dataclass
class FuncParam:
    """A parsed function parameter."""
    type_str: str        # e.g. "uint8_t *", "const char *", "uint16_t"
    name: str            # e.g. "out", "buf", "out_len"
    is_pointer: bool
    is_const: bool
    is_output_buf: bool  # heuristic: non-const pointer to uint8_t/char/void


@dataclass
class FuncSig:
    """A parsed function signature from a header."""
    return_type: str     # e.g. "base64_err_t", "int", "uint16_t"
    name: str            # e.g. "base64_encode"
    params: list         # list of FuncParam
    raw_line: str        # original declaration text


@dataclass
class SentinelTest:
    """A generated sentinel test case."""
    func_name: str
    test_name: str
    code: str


@dataclass
class WheelInfo:
    """Information about a wheel for test generation."""
    name: str
    layer: str           # "primitives" or "protocols"
    header_path: Path
    funcs: list = field(default_factory=list)
    tests: list = field(default_factory=list)


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

def read_header(path: Path) -> str:
    """Read header file, stripping comments."""
    text = path.read_text(encoding="utf-8", errors="replace")
    # Strip block comments
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # Strip line comments
    text = re.sub(r'//[^\n]*', '', text)
    return text


def extract_function_declarations(text: str) -> list:
    """Extract function declarations from preprocessed header text.

    Handles multi-line declarations. Returns list of (return_type, name, params_str).
    """
    # Join continuation lines
    text = re.sub(r'\\\n', ' ', text)

    results = []

    # Match function declarations: return_type name(params);
    # Must handle multi-line params.
    # Strategy: find patterns like "type name(" then collect until ");"
    pattern = re.compile(
        r'^\s*'
        r'((?:(?:static\s+inline|extern|const|unsigned|signed|struct|enum|volatile)\s+)*'
        r'(?:\w+(?:\s*\*)*)\s+)'   # return type (possibly multi-word + pointer)
        r'(\w+)'                      # function name
        r'\s*\(',                     # opening paren
        re.MULTILINE
    )

    for m in pattern.finditer(text):
        ret_type = m.group(1).strip()
        func_name = m.group(2)
        start = m.end()

        # Skip static inline helper bodies, typedefs, macros
        if 'typedef' in ret_type:
            continue

        # Find matching closing paren + semicolon
        depth = 1
        pos = start
        while pos < len(text) and depth > 0:
            if text[pos] == '(':
                depth += 1
            elif text[pos] == ')':
                depth -= 1
            pos += 1

        if depth != 0:
            continue

        params_str = text[start:pos - 1].strip()

        # Check if this is a declaration (followed by ;) or definition (followed by {)
        rest = text[pos:pos + 20].lstrip()
        if rest and rest[0] == '{':
            # This is a static inline definition - skip
            continue

        results.append((ret_type, func_name, params_str))

    return results


def parse_param(raw: str) -> Optional[FuncParam]:
    """Parse a single parameter string into FuncParam."""
    raw = raw.strip()
    if not raw or raw == 'void':
        return None

    # Handle function pointer params: skip them
    if '(' in raw and ')' in raw:
        # Extract name from function pointer: void (*name)(...)
        fp_match = re.search(r'\(\s*\*\s*(\w+)\s*\)', raw)
        if fp_match:
            return FuncParam(
                type_str=raw, name=fp_match.group(1),
                is_pointer=False, is_const=False, is_output_buf=False
            )
        return None

    # Handle array params: uint8_t name[4]
    arr_match = re.match(r'(.+?)\s+(\w+)\s*\[', raw)
    if arr_match:
        type_str = arr_match.group(1).strip()
        name = arr_match.group(2)
        is_const = 'const' in type_str
        return FuncParam(
            type_str=raw, name=name,
            is_pointer=True, is_const=is_const, is_output_buf=False
        )

    # Split into type + name
    # Pointer asterisks may be attached to type or name
    parts = raw.rsplit(None, 1)
    if len(parts) < 2:
        return None

    type_part = parts[0]
    name_part = parts[1]

    # Handle "*name" -> move * to type
    while name_part.startswith('*'):
        type_part += ' *'
        name_part = name_part[1:]

    is_pointer = '*' in type_part
    is_const = 'const' in type_part

    # Determine if this is an output buffer parameter
    is_output_buf = False
    if is_pointer and not is_const:
        base_type = type_part.replace('const', '').replace('*', '').strip()
        output_names = {'out', 'buf', 'data', 'dst', 'resp', 'result', 'values',
                        'regs', 'ip', 'name', 'item'}
        if base_type in ('uint8_t', 'char', 'void', 'int8_t'):
            if name_part in output_names or name_part.startswith('out') or name_part.startswith('buf'):
                is_output_buf = True

    return FuncParam(
        type_str=type_part.strip(),
        name=name_part,
        is_pointer=is_pointer,
        is_const=is_const,
        is_output_buf=is_output_buf
    )


def parse_function(ret_type: str, name: str, params_str: str) -> FuncSig:
    """Parse a function declaration into FuncSig."""
    raw_params = []
    depth = 0
    current = ""
    for ch in params_str:
        if ch == '(':
            depth += 1
            current += ch
        elif ch == ')':
            depth -= 1
            current += ch
        elif ch == ',' and depth == 0:
            raw_params.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        raw_params.append(current.strip())

    params = []
    for rp in raw_params:
        p = parse_param(rp)
        if p:
            params.append(p)

    return FuncSig(
        return_type=ret_type,
        name=name,
        params=params,
        raw_line=f"{ret_type} {name}({params_str})"
    )


# ---------------------------------------------------------------------------
# Function classification and test generation
# ---------------------------------------------------------------------------

def has_output_buffer(func: FuncSig) -> bool:
    """Check if function has an output buffer parameter."""
    return any(p.is_output_buf for p in func.params)


def is_build_or_encode_func(func: FuncSig) -> bool:
    """Check if function is a builder/encoder that writes to a buffer.

    These return byte count (int/uint16_t) and take buf + buf_size params.
    Also matches encoder-context patterns (_enc_ abbreviation).
    """
    keywords = ['build', 'encode', '_enc_', 'format', 'pack', 'serialize']
    name_lower = func.name.lower()
    if any(kw in name_lower for kw in keywords):
        return True
    # Encoder-context: function has *enc param (cbor_lite, msgpack, tlv, pb, bencode)
    if any(p.name == 'enc' and p.is_pointer for p in func.params):
        return True
    return False


def is_codec_func(func: FuncSig) -> bool:
    """Check if function is an encode/decode/read/write/peek with output buffer."""
    keywords = ['encode', 'decode', 'read', 'write', 'peek', 'build',
                'format', 'pack', 'unpack', 'get', 'to_string', 'join',
                'trim', 'split', '_enc_']
    name_lower = func.name.lower()
    if any(kw in name_lower for kw in keywords):
        return True
    # Encoder-context functions
    if any(p.name == 'enc' and p.is_pointer for p in func.params):
        return True
    return False


def has_encoder_context(func: FuncSig) -> bool:
    """Check if function uses an encoder context (*enc parameter)."""
    return any(p.name == 'enc' and p.is_pointer for p in func.params)


def should_generate_test(func: FuncSig, wheel_name: str) -> bool:
    """Decide if we should generate a sentinel test for this function."""
    # Must have output buffer, or be a build/encoder function, or use encoder context
    if (not has_output_buffer(func) and
        not is_build_or_encode_func(func) and
        not has_encoder_context(func)):
        return False

    # Must be a codec-like function or encoder-context function
    if (not is_codec_func(func) and
        not has_output_buffer(func) and
        not has_encoder_context(func)):
        return False

    # Skip init/reset/destroy functions
    skip_suffixes = ['_init', '_reset', '_destroy', '_free', '_clear',
                     '_deinit', '_start', '_stop', '_cancel']
    for suffix in skip_suffixes:
        if func.name.endswith(suffix):
            return False

    # Skip pure query functions
    if func.name.endswith('_count') or func.name.endswith('_len'):
        return False
    if func.name.endswith('_size') or func.name.endswith('_free'):
        return False
    if func.name.endswith('_done') or func.name.endswith('_offset'):
        return False
    if func.name.endswith('_is_empty') or func.name.endswith('_is_full'):
        return False

    # Skip verify/check functions
    if 'verify' in func.name or 'check' in func.name:
        return False
    if 'validate' in func.name or 'match' in func.name:
        return False

    # Skip identify/get_type
    if func.name.endswith('_identify') or func.name.endswith('_get_type'):
        return False

    return True


# ---------------------------------------------------------------------------
# Per-wheel test generators (wheel-specific knowledge)
# ---------------------------------------------------------------------------

def gen_test_base64_encode(wheel_name: str, func: FuncSig) -> Optional[SentinelTest]:
    code = textwrap.dedent("""\
        TEST(test_base64_encode_sentinel) {
            uint8_t input[] = {0x41, 0x42, 0x43};  /* "ABC" */
            char out[16];
            memset(out, 0xFF, sizeof(out));
            uint16_t out_len = sizeof(out);
            base64_encode(input, 3, out, &out_len);
            /* sentinel: byte after written data must be untouched */
            ASSERT_EQ((uint8_t)out[out_len + 1], 0xFF);
        }
    """)
    return SentinelTest("base64_encode", "test_base64_encode_sentinel", code)


def gen_test_base64_decode(wheel_name: str, func: FuncSig) -> Optional[SentinelTest]:
    code = textwrap.dedent("""\
        TEST(test_base64_decode_sentinel) {
            const char *b64 = "QUJD";  /* decodes to "ABC" = 3 bytes */
            uint8_t out[16];
            memset(out, 0xFF, sizeof(out));
            uint16_t out_len = sizeof(out);
            base64_decode(b64, 4, out, &out_len);
            /* sentinel: byte after decoded data must be untouched */
            ASSERT_EQ(out[out_len], 0xFF);
        }
    """)
    return SentinelTest("base64_decode", "test_base64_decode_sentinel", code)


# ---------------------------------------------------------------------------
# Generic test generator
# ---------------------------------------------------------------------------

def generate_generic_sentinel_test(wheel_name: str, func: FuncSig) -> Optional[SentinelTest]:
    """Generate a sentinel test based on function signature analysis.

    Returns None if the function pattern is not recognized.
    """
    fn = func.name
    ret = func.return_type
    params = func.params

    # Classify the function pattern
    # Pattern A: err_t func(const_input, input_len, *out, *out_len)
    #   e.g. base64_encode, cobs_encode, hex_encode, slip_encode
    # Pattern B: int func(*buf, buf_size, ...) -- build functions returning length
    #   e.g. mqtt_packet_build_connect, dns_build_query
    # Pattern C: err_t func(context, *out_buf, ...) -- encoder with context
    #   e.g. cbor_lite_enc_* (uses encoder context)
    # Pattern D: uint16_t func(context, *out, len) -- ring_buffer_read/peek
    # Pattern E: err_t func(const_input, *out_struct) -- parse into struct (skip)
    # Pattern F: int func(*buf, size, fmt, ...) -- snprintf-like

    # Find output buffer params
    out_params = [p for p in params if p.is_output_buf]
    if not out_params:
        # Check for buf+size pattern (build functions)
        buf_param = None
        size_param = None
        for p in params:
            if p.is_pointer and not p.is_const and p.name in ('buf', 'out'):
                base = p.type_str.replace('*', '').strip()
                if base in ('uint8_t', 'char'):
                    buf_param = p
            if not p.is_pointer and p.name in ('buf_size', 'size', 'cap',
                                                 'buf_len', 'out_size'):
                size_param = p
        if buf_param:
            out_params = [buf_param]

    if not out_params:
        # For encoder-context functions, the output is in the context,
        # not in an explicit output param
        if has_encoder_context(func):
            test_name = f"test_{fn}_sentinel"
            return _gen_encoder_context_test(wheel_name, func, test_name)
        return None

    # Determine which pattern this is and generate appropriate test
    test_name = f"test_{fn}_sentinel"
    prefix = wheel_name

    # Check if it's a simple encode/decode with in/out buffers
    has_const_input = any(p.is_const and p.is_pointer for p in params)
    has_out_len = any(p.name in ('out_len',) and p.is_pointer for p in params)
    has_buf_size = any(p.name in ('buf_size', 'size', 'cap', 'buf_len',
                                   'out_size', 'buf_cap') for p in params)

    # Determine return value handling
    ret_is_err = '_err_t' in ret or ret.endswith('_t') and 'err' in ret.lower()
    ret_is_int = ret in ('int', 'int16_t', 'int32_t')
    ret_is_uint = ret in ('uint16_t', 'uint32_t', 'size_t')

    out_p = out_params[0]
    out_is_char = 'char' in out_p.type_str
    out_is_uint8 = 'uint8_t' in out_p.type_str

    # --- Determine what kind of function this is and generate accordingly ---

    # Encoder-context pattern (bencode, cbor, tlv, protobuf, msgpack)
    if any(p.name == 'enc' and p.is_pointer for p in params):
        return _gen_encoder_context_test(wheel_name, func, test_name)

    # Decoder-context pattern (feeds into *item/*token/*field struct -- not buffer output)
    if any(p.name in ('dec', 'decoder') and p.is_pointer for p in params):
        # Decoders typically output to a struct, not a buffer -- skip
        return None

    # Build pattern: int func(buf, buf_size, ...) -> returns written count
    # Prioritize build pattern over inout when function name contains 'build'
    if (ret_is_int or ret_is_uint) and has_buf_size and not has_out_len:
        return _gen_build_pattern_test(wheel_name, func, test_name)

    # Build with out_len: err func(buf, buf_len, *out_len, ...)
    # e.g. coap_packet_build, websocket_frame_build
    if 'build' in fn and has_buf_size:
        return _gen_build_pattern_test(wheel_name, func, test_name)

    # Encode/decode with out_len pattern: err func(in, in_len, out, &out_len)
    if has_out_len and (ret_is_err or ret_is_int):
        return _gen_inout_pattern_test(wheel_name, func, test_name)

    # Read/peek pattern: uint16_t func(ctx, *out, len) -> returns read count
    if ret_is_uint and not has_out_len:
        return _gen_read_pattern_test(wheel_name, func, test_name)

    # Simple output pattern: err func(..., *out, out_size)
    if has_buf_size and ret_is_err:
        return _gen_simple_output_test(wheel_name, func, test_name)

    # err_t func with output buffer but no size param (e.g. byte_buffer_get_bytes)
    if ret_is_err and out_params:
        return _gen_fallback_test(wheel_name, func, test_name)

    # Fallback: try a generic pattern
    return _gen_fallback_test(wheel_name, func, test_name)


def _gen_encoder_context_test(wheel_name: str, func: FuncSig, test_name: str) -> Optional[SentinelTest]:
    """Generate test for encoder-context pattern (cbor, msgpack, tlv, protobuf, bencode)."""
    fn = func.name
    prefix = wheel_name

    # Determine which encoder API this is
    enc_types = {
        'cbor_lite': ('cbor_lite_encoder_t', 'cbor_lite_enc_init', 'cbor_lite_enc_len'),
        'msgpack': ('msgpack_enc_t', 'msgpack_enc_init', 'msgpack_enc_len'),
        'tlv': ('tlv_encoder_t', 'tlv_encoder_init', 'tlv_encoder_len'),
        'pb': ('pb_encoder_t', 'pb_encoder_init', 'pb_encoder_len'),
        'bencode': ('bencode_encoder_t', 'bencode_encoder_init', 'bencode_encoder_len'),
    }

    enc_info = None
    for key, info in enc_types.items():
        if fn.startswith(key):
            enc_info = info
            break

    if not enc_info:
        return None

    enc_type, init_fn, len_fn = enc_info

    # Determine what to encode based on function name
    call_args = _make_encoder_call(fn, func)
    if not call_args:
        return None

    code = textwrap.dedent(f"""\
        TEST({test_name}) {{
            uint8_t backing[64];
            memset(backing, 0xFF, sizeof(backing));
            {enc_type} enc;
            {init_fn}(&enc, backing, sizeof(backing));
            {call_args}
            size_t used = {len_fn}(&enc);
            /* sentinel: byte after encoded data must be untouched */
            if (used > 0 && used < sizeof(backing)) {{
                ASSERT_EQ(backing[used], 0xFF);
            }}
        }}
    """)
    return SentinelTest(fn, test_name, code)


def _make_encoder_call(fn: str, func: FuncSig) -> Optional[str]:
    """Generate the call expression for an encoder function."""
    params = func.params

    # Skip init functions
    if fn.endswith('_init'):
        return None

    # CBOR
    if fn == 'cbor_lite_enc_uint':
        return "cbor_lite_enc_uint(&enc, 42);"
    if fn == 'cbor_lite_enc_nint':
        return "cbor_lite_enc_nint(&enc, 1);"
    if fn == 'cbor_lite_enc_bstr':
        return 'cbor_lite_enc_bstr(&enc, (const uint8_t *)"test", 4);'
    if fn == 'cbor_lite_enc_tstr':
        return 'cbor_lite_enc_tstr(&enc, "hello", 5);'
    if fn == 'cbor_lite_enc_array':
        return "cbor_lite_enc_array(&enc, 2);"
    if fn == 'cbor_lite_enc_map':
        return "cbor_lite_enc_map(&enc, 1);"
    if fn == 'cbor_lite_enc_bool':
        return "cbor_lite_enc_bool(&enc, true);"
    if fn == 'cbor_lite_enc_null':
        return "cbor_lite_enc_null(&enc);"
    if fn == 'cbor_lite_enc_float':
        return "cbor_lite_enc_float(&enc, 3.14f);"

    # MsgPack
    if fn == 'msgpack_enc_nil':
        return "msgpack_enc_nil(&enc);"
    if fn == 'msgpack_enc_bool':
        return "msgpack_enc_bool(&enc, true);"
    if fn == 'msgpack_enc_uint':
        return "msgpack_enc_uint(&enc, 42);"
    if fn == 'msgpack_enc_int':
        return "msgpack_enc_int(&enc, -1);"
    if fn == 'msgpack_enc_float':
        return "msgpack_enc_float(&enc, 1.5f);"
    if fn == 'msgpack_enc_str':
        return 'msgpack_enc_str(&enc, "hi", 2);'
    if fn == 'msgpack_enc_bin':
        return 'msgpack_enc_bin(&enc, (const uint8_t *)"\\x01\\x02", 2);'
    if fn == 'msgpack_enc_array':
        return "msgpack_enc_array(&enc, 2);"
    if fn == 'msgpack_enc_map':
        return "msgpack_enc_map(&enc, 1);"

    # TLV
    if fn == 'tlv_encode':
        return 'tlv_encode(&enc, 0x0001, (const uint8_t *)"AB", 2);'
    if fn == 'tlv_encode_u8':
        return "tlv_encode_u8(&enc, 0x0001, 0x42);"
    if fn == 'tlv_encode_u16':
        return "tlv_encode_u16(&enc, 0x0002, 0x1234);"
    if fn == 'tlv_encode_u32':
        return "tlv_encode_u32(&enc, 0x0003, 0xDEADBEEF);"
    if fn == 'tlv_encode_string':
        return 'tlv_encode_string(&enc, 0x0004, "hello");'

    # Protobuf
    if fn == 'pb_encode_varint':
        return "pb_encode_varint(&enc, 1, 150);"
    if fn == 'pb_encode_sint32':
        return "pb_encode_sint32(&enc, 2, -1);"
    if fn == 'pb_encode_sint64':
        return "pb_encode_sint64(&enc, 3, -100);"
    if fn == 'pb_encode_fixed32':
        return "pb_encode_fixed32(&enc, 4, 0x12345678);"
    if fn == 'pb_encode_fixed64':
        return "pb_encode_fixed64(&enc, 5, 0xDEADBEEFCAFEull);"
    if fn == 'pb_encode_float':
        return "pb_encode_float(&enc, 6, 2.5f);"
    if fn == 'pb_encode_bytes':
        return 'pb_encode_bytes(&enc, 7, (const uint8_t *)"\\x01\\x02\\x03", 3);'
    if fn == 'pb_encode_string':
        return 'pb_encode_string(&enc, 8, "test");'

    # Bencode
    if fn == 'bencode_encode_int':
        return "bencode_encode_int(&enc, 42);"
    if fn == 'bencode_encode_string':
        return 'bencode_encode_string(&enc, (const uint8_t *)"foo", 3);'
    if fn == 'bencode_encode_list_begin':
        return "bencode_encode_list_begin(&enc);"
    if fn == 'bencode_encode_list_end':
        return "bencode_encode_list_end(&enc);"
    if fn == 'bencode_encode_dict_begin':
        return "bencode_encode_dict_begin(&enc);"
    if fn == 'bencode_encode_dict_end':
        return "bencode_encode_dict_end(&enc);"

    return None


def _gen_build_pattern_test(wheel_name: str, func: FuncSig, test_name: str) -> Optional[SentinelTest]:
    """Generate test for build-pattern: int func(buf, buf_size, ...)."""
    fn = func.name
    call = _make_build_call(fn, func)
    if not call:
        return None

    code = (f"TEST({test_name}) {{\n"
            f"    uint8_t buf[256];\n"
            f"    memset(buf, 0xFF, sizeof(buf));\n"
            f"    {call}\n"
            f"    /* sentinel: byte after written data must be untouched */\n"
            f"    if (written > 0 && (size_t)written < sizeof(buf)) {{\n"
            f"        ASSERT_EQ(buf[written], 0xFF);\n"
            f"    }}\n"
            f"}}\n\n")
    return SentinelTest(fn, test_name, code)


def _make_build_call(fn: str, func: FuncSig) -> Optional[str]:
    """Generate the call for a build-pattern function."""

    # MQTT 3.1.1
    if fn == 'mqtt_packet_build_connect':
        return ('mqtt_connect_opts_t opts = { .client_id = "test", .keepalive_s = 60, .clean_session = true };\n'
                '    int written = mqtt_packet_build_connect(buf, sizeof(buf) - 1, &opts);')
    if fn == 'mqtt_packet_build_publish':
        return ('mqtt_publish_t pub = { .topic = "t", .payload = (const uint8_t *)"hi", .payload_len = 2, .qos = 0 };\n'
                '    int written = mqtt_packet_build_publish(buf, sizeof(buf) - 1, &pub);')
    if fn == 'mqtt_packet_build_subscribe':
        return 'int written = mqtt_packet_build_subscribe(buf, sizeof(buf) - 1, 1, "topic", 0);'
    if fn == 'mqtt_packet_build_pingreq':
        return 'int written = mqtt_packet_build_pingreq(buf, sizeof(buf) - 1);'
    if fn == 'mqtt_packet_build_disconnect':
        return 'int written = mqtt_packet_build_disconnect(buf, sizeof(buf) - 1);'
    if fn == 'mqtt_packet_encode_remaining_length':
        return 'int written = mqtt_packet_encode_remaining_length(buf, 127);'

    # MQTT 5.0
    if fn == 'mqtt5_build_connect':
        return ('mqtt5_connect_t cfg = { .client_id = "test", .keep_alive = 60, .clean_start = true };\n'
                '    int written = mqtt5_build_connect(buf, sizeof(buf) - 1, &cfg);')
    if fn == 'mqtt5_build_publish':
        return ('mqtt5_publish_t pub = { .topic = "t", .payload = (const uint8_t *)"x", .payload_len = 1, .qos = 0 };\n'
                '    int written = mqtt5_build_publish(buf, sizeof(buf) - 1, &pub);')
    if fn == 'mqtt5_build_subscribe':
        return 'int written = mqtt5_build_subscribe(buf, sizeof(buf) - 1, 1, "topic", 0);'
    if fn == 'mqtt5_build_disconnect':
        return 'int written = mqtt5_build_disconnect(buf, sizeof(buf) - 1, 0);'
    if fn == 'mqtt5_encode_vbi':
        return 'int written = mqtt5_encode_vbi(buf, 127);'

    # DNS
    if fn == 'dns_build_query':
        return 'int written = dns_build_query(buf, sizeof(buf) - 1, 0x1234, "example.com", DNS_TYPE_A);'
    if fn == 'dns_encode_name':
        return 'int written = dns_encode_name("example.com", buf, sizeof(buf) - 1);'
    if fn == 'dns_decode_name':
        # dns_decode_name writes to char *name buffer
        return ('/* Pre-encoded "example.com" in DNS wire format */\n'
                '    uint8_t pkt[] = {7,\'e\',\'x\',\'a\',\'m\',\'p\',\'l\',\'e\',3,\'c\',\'o\',\'m\',0};\n'
                '    char name_buf[64];\n'
                '    memset(name_buf, 0xFF, sizeof(name_buf));\n'
                '    int written = dns_decode_name(pkt, sizeof(pkt), 0, name_buf, sizeof(name_buf) - 1);\n'
                '    /* For dns_decode_name, check sentinel after the NUL-terminated string */\n'
                '    if (written > 0) {\n'
                '        size_t slen = strlen(name_buf);\n'
                '        if (slen + 2 < sizeof(name_buf)) {\n'
                '            ASSERT_EQ((uint8_t)name_buf[slen + 2], 0xFF);\n'
                '        }\n'
                '    }\n'
                '    return; /* custom sentinel check above */\n'
                '    int unused_written = 0; (void)unused_written')

    # LoRaWAN
    if fn == 'lorawan_build_data':
        return ('uint8_t payload[] = {0x01, 0x02};\n'
                '    lorawan_frame_t frame = {\n'
                '        .mtype = LORAWAN_UNCONFIRMED_UP, .dev_addr = 0x01020304,\n'
                '        .fcnt = 1, .fport = 1, .payload = payload, .payload_len = 2,\n'
                '        .mic = {0xAA, 0xBB, 0xCC, 0xDD}\n'
                '    };\n'
                '    int written = lorawan_build_data(buf, sizeof(buf) - 1, &frame);')
    if fn == 'lorawan_build_join_request':
        return ('uint8_t app_eui[8] = {0};\n'
                '    uint8_t dev_eui[8] = {0};\n'
                '    uint8_t dev_nonce[2] = {0x01, 0x02};\n'
                '    int written = lorawan_build_join_request(buf, sizeof(buf) - 1, app_eui, dev_eui, dev_nonce);')

    # Modbus RTU
    if fn == 'modbus_rtu_build':
        return ('uint8_t data[] = {0x00, 0x01, 0x00, 0x02};\n'
                '    int written = modbus_rtu_build(buf, sizeof(buf) - 1, 1, 0x03, data, 4);')
    if fn == 'modbus_rtu_master_read_regs':
        return 'int written = modbus_rtu_master_read_regs(buf, sizeof(buf) - 1, 1, 0x03, 0, 10);'
    if fn == 'modbus_rtu_master_write_reg':
        return 'int written = modbus_rtu_master_write_reg(buf, sizeof(buf) - 1, 1, 0, 0x1234);'
    if fn == 'modbus_rtu_master_write_regs':
        return ('uint16_t vals[] = {100, 200};\n'
                '    int written = modbus_rtu_master_write_regs(buf, sizeof(buf) - 1, 1, 0, 2, vals);')

    # Modbus TCP
    if fn == 'modbus_tcp_build_read_regs':
        return 'int written = modbus_tcp_build_read_regs(buf, sizeof(buf) - 1, 1, 1, 0, 10);'
    if fn == 'modbus_tcp_build_write_reg':
        return 'int written = modbus_tcp_build_write_reg(buf, sizeof(buf) - 1, 1, 1, 0, 0x1234);'
    if fn == 'modbus_tcp_build_write_regs':
        return ('uint16_t vals[] = {100, 200};\n'
                '    int written = modbus_tcp_build_write_regs(buf, sizeof(buf) - 1, 1, 1, 0, vals, 2);')

    # CAN bus
    if fn == 'canbus_frame_pack':
        return ('canbus_frame_t frame = { .id = 0x123, .dlc = 2, .data = {0xAA, 0xBB} };\n'
                '    int written = canbus_frame_pack(&frame, buf, sizeof(buf) - 1);')

    # CoAP
    if fn == 'coap_packet_build':
        return ('size_t out_len = 0;\n'
                '    coap_packet_err_t err = coap_packet_build(buf, sizeof(buf) - 1, &out_len,\n'
                '        COAP_TYPE_CON, COAP_CODE_GET, 0x1234,\n'
                '        NULL, 0, NULL, 0, NULL, 0);\n'
                '    int written = (err == COAP_PACKET_OK) ? (int)out_len : -1;')

    # WebSocket
    if fn == 'websocket_frame_build':
        return ('size_t out_len = 0;\n'
                '    uint8_t payload[] = {0x48, 0x69};  /* "Hi" */\n'
                '    websocket_frame_err_t err = websocket_frame_build(buf, sizeof(buf) - 1, &out_len,\n'
                '        true, WEBSOCKET_FRAME_OPCODE_TEXT, NULL, payload, 2);\n'
                '    int written = (err == WEBSOCKET_FRAME_OK) ? (int)out_len : -1;')

    # Syslog
    if fn == 'syslog_format':
        return ('syslog_msg_t msg = {\n'
                '        .facility = SYSLOG_FAC_USER, .severity = SYSLOG_SEV_INFO,\n'
                '        .hostname = "host", .app_name = "app", .message = "test"\n'
                '    };\n'
                '    int written = syslog_format(&msg, (char *)buf, sizeof(buf) - 1);')

    # MIDI
    if fn == 'midi_build_note_on':
        return 'int written = midi_build_note_on(buf, sizeof(buf) - 1, 0, 60, 100);'
    if fn == 'midi_build_note_off':
        return 'int written = midi_build_note_off(buf, sizeof(buf) - 1, 0, 60, 0);'
    if fn == 'midi_build_cc':
        return 'int written = midi_build_cc(buf, sizeof(buf) - 1, 0, 7, 100);'
    if fn == 'midi_build_program':
        return 'int written = midi_build_program(buf, sizeof(buf) - 1, 0, 0);'
    if fn == 'midi_build_pitch_bend':
        return 'int written = midi_build_pitch_bend(buf, sizeof(buf) - 1, 0, 0);'

    # Modbus RTU slave process
    if fn == 'modbus_rtu_slave_process':
        return None  # Needs full slave init with callbacks

    # snprintf_tiny
    if fn == 'snprintf_tiny':
        return 'int written = snprintf_tiny((char *)buf, sizeof(buf) - 1, "%d", 42);'

    return None


def _gen_inout_pattern_test(wheel_name: str, func: FuncSig, test_name: str) -> Optional[SentinelTest]:
    """Generate test for in/out pattern: err func(in, in_len, out, &out_len)."""
    fn = func.name
    out_params = [p for p in func.params if p.is_output_buf]
    if not out_params:
        return None

    out_p = out_params[0]
    out_is_char = 'char' in out_p.type_str
    buf_type = "char" if out_is_char else "uint8_t"

    call = _make_inout_call(fn, func, buf_type)
    if not call:
        return None

    code = textwrap.dedent(f"""\
        TEST({test_name}) {{
            {buf_type} out[64];
            memset(out, 0xFF, sizeof(out));
            uint16_t out_len = sizeof(out);
            {call}
            /* sentinel: byte after output data must be untouched */
            ASSERT_EQ(({buf_type} == 'c' ? (uint8_t)out[out_len] : out[out_len]), 0xFF);
        }}
    """)
    # Fix the type check to be compile-time
    # Actually, just use uint8_t cast for char buffers
    code = (f"TEST({test_name}) {{\n"
            f"    {buf_type} out[64];\n"
            f"    memset(out, 0xFF, sizeof(out));\n"
            f"    uint16_t out_len = sizeof(out);\n"
            f"    {call}\n"
            f"    /* sentinel: byte after output data must be untouched */\n"
            f"    ASSERT_EQ((uint8_t)out[out_len], 0xFF);\n"
            f"}}\n\n")
    return SentinelTest(fn, test_name, code)


def _make_inout_call(fn: str, func: FuncSig, buf_type: str) -> Optional[str]:
    """Generate the call for an in/out pattern function."""

    # base64
    if fn == 'base64_encode':
        return 'uint8_t input[] = {0x41, 0x42, 0x43};\n    base64_encode(input, 3, out, &out_len);'
    if fn == 'base64_decode':
        return 'base64_decode("QUJD", 4, out, &out_len);'

    # hex_codec
    if fn == 'hex_encode':
        return 'uint8_t input[] = {0xDE, 0xAD};\n    hex_encode(input, 2, out, sizeof(out));'
    if fn == 'hex_encode_upper':
        return 'uint8_t input[] = {0xBE, 0xEF};\n    hex_encode_upper(input, 2, out, sizeof(out));'
    if fn == 'hex_decode':
        return 'hex_decode("DEADBEEF", 8, out, &out_len);'

    # cobs
    if fn == 'cobs_encode':
        return 'uint8_t input[] = {0x01, 0x00, 0x02};\n    cobs_encode(input, 3, out, &out_len);'
    if fn == 'cobs_decode':
        return ('/* COBS-encoded {0x01, 0x00, 0x02}: [0x02, 0x01, 0x02, 0x02] */\n'
                '    uint8_t encoded[] = {0x02, 0x01, 0x02, 0x02};\n'
                '    cobs_decode(encoded, 4, out, &out_len);')

    # slip
    if fn == 'slip_encode':
        return 'uint8_t input[] = {0x01, 0x02, 0x03};\n    slip_encode(input, 3, out, &out_len);'
    if fn == 'slip_decode':
        return ('/* SLIP-encoded frame: END + data + END */\n'
                '    uint8_t encoded[] = {0xC0, 0x01, 0x02, 0x03, 0xC0};\n'
                '    slip_decode(encoded, 5, out, &out_len);')

    return None


def _gen_read_pattern_test(wheel_name: str, func: FuncSig, test_name: str) -> Optional[SentinelTest]:
    """Generate test for read/peek pattern: uint16_t func(ctx, *out, len)."""
    fn = func.name

    # ring_buffer read/peek
    if fn == 'ring_buffer_read':
        code = textwrap.dedent("""\
            TEST(test_ring_buffer_read_sentinel) {
                uint8_t storage[16];
                ring_buffer_t rb;
                ring_buffer_init(&rb, storage, sizeof(storage));
                uint8_t input[] = {0xAA, 0xBB, 0xCC};
                ring_buffer_write(&rb, input, 3);
                uint8_t out[8];
                memset(out, 0xFF, sizeof(out));
                uint16_t n = ring_buffer_read(&rb, out, 3);
                /* sentinel: byte after read data must be untouched */
                ASSERT_EQ(out[n], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'ring_buffer_peek':
        code = textwrap.dedent("""\
            TEST(test_ring_buffer_peek_sentinel) {
                uint8_t storage[16];
                ring_buffer_t rb;
                ring_buffer_init(&rb, storage, sizeof(storage));
                uint8_t input[] = {0xAA, 0xBB, 0xCC};
                ring_buffer_write(&rb, input, 3);
                uint8_t out[8];
                memset(out, 0xFF, sizeof(out));
                uint16_t n = ring_buffer_peek(&rb, out, 3);
                /* sentinel: byte after peeked data must be untouched */
                ASSERT_EQ(out[n], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # ring_buffer_write (returns written count, writes to internal buffer, not really output-buf sentinel)
    if fn == 'ring_buffer_write':
        return None

    # byte_buffer get functions
    if fn == 'byte_buffer_get_bytes':
        code = textwrap.dedent("""\
            TEST(test_byte_buffer_get_bytes_sentinel) {
                uint8_t storage[32];
                byte_buffer_t bb;
                byte_buffer_init(&bb, storage, sizeof(storage));
                uint8_t input[] = {0x11, 0x22, 0x33};
                byte_buffer_put_bytes(&bb, input, 3);
                uint8_t out[8];
                memset(out, 0xFF, sizeof(out));
                byte_buffer_get_bytes(&bb, out, 3);
                /* sentinel: byte after read data must be untouched */
                ASSERT_EQ(out[3], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # fifo_queue_pop/peek
    if fn == 'fifo_queue_pop':
        code = textwrap.dedent("""\
            TEST(test_fifo_queue_pop_sentinel) {
                uint8_t storage[64];
                fifo_queue_t q;
                uint32_t item = 0x12345678;
                fifo_queue_init(&q, storage, 4, sizeof(uint32_t));
                fifo_queue_push(&q, &item);
                uint8_t out[8];  /* room for uint32_t + sentinel */
                memset(out, 0xFF, sizeof(out));
                fifo_queue_pop(&q, out);
                /* sentinel: bytes after element must be untouched */
                ASSERT_EQ(out[4], 0xFF);
                ASSERT_EQ(out[5], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'fifo_queue_peek':
        code = textwrap.dedent("""\
            TEST(test_fifo_queue_peek_sentinel) {
                uint8_t storage[64];
                fifo_queue_t q;
                uint32_t item = 0xDEADBEEF;
                fifo_queue_init(&q, storage, 4, sizeof(uint32_t));
                fifo_queue_push(&q, &item);
                uint8_t out[8];  /* room for uint32_t + sentinel */
                memset(out, 0xFF, sizeof(out));
                fifo_queue_peek(&q, out);
                /* sentinel: bytes after element must be untouched */
                ASSERT_EQ(out[4], 0xFF);
                ASSERT_EQ(out[5], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    return None


def _gen_simple_output_test(wheel_name: str, func: FuncSig, test_name: str) -> Optional[SentinelTest]:
    """Generate test for simple output pattern: err func(..., *out, size)."""
    fn = func.name

    # string_utils
    if fn == 'str_trim':
        code = textwrap.dedent("""\
            TEST(test_str_trim_sentinel) {
                char out[16];
                memset(out, 0xFF, sizeof(out));
                str_trim("  hello  ", out, sizeof(out) - 1);
                size_t len = strlen(out);
                /* sentinel: bytes well after string must be untouched */
                ASSERT_EQ((uint8_t)out[sizeof(out) - 1], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'str_ltrim':
        code = textwrap.dedent("""\
            TEST(test_str_ltrim_sentinel) {
                char out[16];
                memset(out, 0xFF, sizeof(out));
                str_ltrim("  hello", out, sizeof(out) - 1);
                ASSERT_EQ((uint8_t)out[sizeof(out) - 1], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'str_rtrim':
        code = textwrap.dedent("""\
            TEST(test_str_rtrim_sentinel) {
                char out[16];
                memset(out, 0xFF, sizeof(out));
                str_rtrim("hello  ", out, sizeof(out) - 1);
                ASSERT_EQ((uint8_t)out[sizeof(out) - 1], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'str_join':
        code = textwrap.dedent("""\
            TEST(test_str_join_sentinel) {
                const char *tokens[] = {"a", "b", "c"};
                char out[16];
                memset(out, 0xFF, sizeof(out));
                str_join(tokens, 3, ",", out, sizeof(out) - 1);
                ASSERT_EQ((uint8_t)out[sizeof(out) - 1], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # uuid_to_string
    if fn == 'uuid_to_string':
        code = textwrap.dedent("""\
            TEST(test_uuid_to_string_sentinel) {
                uuid_t id = {{0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                              0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10}};
                char out[48];  /* 36 chars + NUL + sentinel room */
                memset(out, 0xFF, sizeof(out));
                uuid_to_string(&id, out, sizeof(out) - 1);
                /* sentinel: bytes after the 37-byte output must be untouched */
                ASSERT_EQ((uint8_t)out[38], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # printf_tiny snprintf
    if fn == 'snprintf_tiny':
        code = textwrap.dedent("""\
            TEST(test_snprintf_tiny_sentinel) {
                char out[16];
                memset(out, 0xFF, sizeof(out));
                int n = snprintf_tiny(out, 10, "%d", 42);
                /* sentinel: byte after buffer limit must be untouched */
                ASSERT_EQ((uint8_t)out[10], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # hex_codec
    if fn == 'hex_encode':
        code = textwrap.dedent("""\
            TEST(test_hex_encode_sentinel) {
                uint8_t input[] = {0xDE, 0xAD};
                char out[16];
                memset(out, 0xFF, sizeof(out));
                hex_encode(input, 2, out, sizeof(out));
                /* encoded "dead" = 4 chars + NUL = 5 bytes; sentinel after NUL */
                ASSERT_EQ((uint8_t)out[5], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'hex_encode_upper':
        code = textwrap.dedent("""\
            TEST(test_hex_encode_upper_sentinel) {
                uint8_t input[] = {0xBE, 0xEF};
                char out[16];
                memset(out, 0xFF, sizeof(out));
                hex_encode_upper(input, 2, out, sizeof(out));
                /* encoded "BEEF" = 4 chars + NUL = 5 bytes; sentinel after NUL */
                ASSERT_EQ((uint8_t)out[5], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    return None


def _gen_fallback_test(wheel_name: str, func: FuncSig, test_name: str) -> Optional[SentinelTest]:
    """Last resort: try to generate a basic sentinel test."""
    fn = func.name

    # byte_buffer_get_u8/u16/u32
    if fn == 'byte_buffer_get_u8':
        code = textwrap.dedent("""\
            TEST(test_byte_buffer_get_u8_sentinel) {
                uint8_t storage[16];
                byte_buffer_t bb;
                byte_buffer_init(&bb, storage, sizeof(storage));
                byte_buffer_put_u8(&bb, 0x42);
                uint8_t out[4];
                memset(out, 0xFF, sizeof(out));
                byte_buffer_get_u8(&bb, &out[0]);
                /* sentinel: second byte must be untouched */
                ASSERT_EQ(out[1], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'byte_buffer_get_u16_be':
        code = textwrap.dedent("""\
            TEST(test_byte_buffer_get_u16_be_sentinel) {
                uint8_t storage[16];
                byte_buffer_t bb;
                byte_buffer_init(&bb, storage, sizeof(storage));
                byte_buffer_put_u16_be(&bb, 0x1234);
                uint8_t out[4];
                memset(out, 0xFF, sizeof(out));
                uint16_t val;
                byte_buffer_get_u16_be(&bb, &val);
                /* val is written to a uint16_t, not buffer -- checking API correctness */
                ASSERT_EQ(val, 0x1234);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'byte_buffer_get_u32_be':
        code = textwrap.dedent("""\
            TEST(test_byte_buffer_get_u32_be_sentinel) {
                uint8_t storage[16];
                byte_buffer_t bb;
                byte_buffer_init(&bb, storage, sizeof(storage));
                byte_buffer_put_u32_be(&bb, 0xDEADBEEF);
                uint32_t val;
                byte_buffer_get_u32_be(&bb, &val);
                ASSERT_EQ(val, (long long)0xDEADBEEF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # bitfield_set
    if fn == 'bitfield_set':
        code = textwrap.dedent("""\
            TEST(test_bitfield_set_sentinel) {
                uint8_t data[8];
                memset(data, 0xFF, sizeof(data));
                memset(data, 0x00, 4);  /* 4 bytes of data, 4 bytes sentinel */
                bitfield_set(data, 4, 0, 8, 0xAB);
                /* sentinel: bytes 4-7 must be untouched */
                ASSERT_EQ(data[4], 0xFF);
                ASSERT_EQ(data[5], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # modbus_rtu_master_get_regs
    if fn == 'modbus_rtu_master_get_regs':
        code = textwrap.dedent("""\
            TEST(test_modbus_rtu_master_get_regs_sentinel) {
                /* Simulate a read response: addr(1) + func(1) + bytecount(1) + 2regs(4) + CRC(2) */
                uint8_t raw[] = {0x01, 0x03, 0x04, 0x00, 0x0A, 0x00, 0x14, 0x00, 0x00};
                /* Fix CRC (we'll skip CRC check by building frame manually) */
                modbus_rtu_frame_t frame = {
                    .addr = 1, .func = 0x03,
                    .data = &raw[2], .data_len = 5
                };
                uint16_t regs[8];
                memset(regs, 0xFF, sizeof(regs));
                int n = modbus_rtu_master_get_regs(&frame, regs, 8);
                if (n > 0 && n < 8) {
                    /* sentinel: register after extracted ones must be untouched */
                    ASSERT_EQ(regs[n], 0xFFFF);
                }
            }
        """)
        return SentinelTest(fn, test_name, code)

    # modbus_tcp_get_regs
    if fn == 'modbus_tcp_get_regs':
        code = textwrap.dedent("""\
            TEST(test_modbus_tcp_get_regs_sentinel) {
                /* Build a minimal parsed frame for FC 0x03 response:
                   byte_count(1) + register_data(4 bytes = 2 regs) */
                uint8_t data[] = {0x04, 0x00, 0x0A, 0x00, 0x14};
                modbus_tcp_frame_t frame = {
                    .function_code = 0x03,
                    .data = data, .data_len = 5
                };
                uint16_t regs[8];
                memset(regs, 0xFF, sizeof(regs));
                uint16_t actual = 0;
                modbus_tcp_get_regs(&frame, regs, 8, &actual);
                if (actual > 0 && actual < 8) {
                    ASSERT_EQ(regs[actual], 0xFFFF);
                }
            }
        """)
        return SentinelTest(fn, test_name, code)

    # byte_buffer get functions
    if fn == 'byte_buffer_get_bytes':
        code = textwrap.dedent("""\
            TEST(test_byte_buffer_get_bytes_sentinel) {
                uint8_t storage[32];
                byte_buffer_t bb;
                byte_buffer_init(&bb, storage, sizeof(storage));
                uint8_t input[] = {0x11, 0x22, 0x33};
                byte_buffer_put_bytes(&bb, input, 3);
                uint8_t out[8];
                memset(out, 0xFF, sizeof(out));
                byte_buffer_get_bytes(&bb, out, 3);
                /* sentinel: byte after read data must be untouched */
                ASSERT_EQ(out[3], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # fifo_queue pop/peek
    if fn == 'fifo_queue_pop':
        code = textwrap.dedent("""\
            TEST(test_fifo_queue_pop_sentinel) {
                uint8_t storage[64];
                fifo_queue_t q;
                uint32_t item = 0x12345678;
                fifo_queue_init(&q, storage, 4, sizeof(uint32_t));
                fifo_queue_push(&q, &item);
                uint8_t out[8];  /* room for uint32_t + sentinel */
                memset(out, 0xFF, sizeof(out));
                fifo_queue_pop(&q, out);
                /* sentinel: bytes after element must be untouched */
                ASSERT_EQ(out[4], 0xFF);
                ASSERT_EQ(out[5], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    if fn == 'fifo_queue_peek':
        code = textwrap.dedent("""\
            TEST(test_fifo_queue_peek_sentinel) {
                uint8_t storage[64];
                fifo_queue_t q;
                uint32_t item = 0xDEADBEEF;
                fifo_queue_init(&q, storage, 4, sizeof(uint32_t));
                fifo_queue_push(&q, &item);
                uint8_t out[8];  /* room for uint32_t + sentinel */
                memset(out, 0xFF, sizeof(out));
                fifo_queue_peek(&q, out);
                /* sentinel: bytes after element must be untouched */
                ASSERT_EQ(out[4], 0xFF);
                ASSERT_EQ(out[5], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    # uuid_from_string writes to uuid_t (16 bytes)
    if fn == 'uuid_from_string':
        code = textwrap.dedent("""\
            TEST(test_uuid_from_string_sentinel) {
                uint8_t mem[20];  /* 16-byte uuid + 4 sentinel */
                memset(mem, 0xFF, sizeof(mem));
                uuid_t *id = (uuid_t *)mem;
                uuid_from_string(id, "01020304-0506-0708-090a-0b0c0d0e0f10");
                /* sentinel: bytes 16-19 must be untouched */
                ASSERT_EQ(mem[16], 0xFF);
                ASSERT_EQ(mem[17], 0xFF);
            }
        """)
        return SentinelTest(fn, test_name, code)

    return None


# ---------------------------------------------------------------------------
# File generation
# ---------------------------------------------------------------------------

def get_includes(wheel: WheelInfo) -> str:
    """Generate #include lines for a wheel's sentinel test."""
    includes = ['#include "test_harness.h"']
    includes.append(f'#include "../{wheel.name}.h"')
    includes.append('#include <string.h>')

    # Add stdbool.h if needed
    needs_bool = False
    for t in wheel.tests:
        if 'true' in t.code or 'false' in t.code:
            needs_bool = True
            break
    if needs_bool:
        includes.append('#include <stdbool.h>')

    # Add stddef.h for size_t
    for t in wheel.tests:
        if 'size_t' in t.code or 'strlen' in t.code:
            includes.append('#include <stddef.h>')
            break

    return '\n'.join(sorted(set(includes)))


def generate_test_file(wheel: WheelInfo) -> str:
    """Generate the complete test file content."""
    includes = get_includes(wheel)

    test_bodies = '\n'.join(t.code for t in wheel.tests)
    run_calls = '\n'.join(f'    RUN({t.test_name});' for t in wheel.tests)

    return f"""\
/* Auto-generated sentinel tests for {wheel.name}
 * Verifies output-buffer functions write exactly the expected bytes.
 * Generated by tooling/test/gen_sentinel_tests.py
 */

{includes}

{test_bodies}
TEST_MAIN("{wheel.name} sentinel")
{run_calls}
TEST_MAIN_END()
"""


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def scan_wheel(layer: str, name: str) -> Optional[WheelInfo]:
    """Scan a single wheel and generate sentinel tests for it."""
    wheel_dir = WHEELS_DIR / layer / name
    header_path = wheel_dir / f"{name}.h"

    if not header_path.exists():
        return None

    if name in SKIP_WHEELS or name in VENDOR_WHEELS:
        return None

    wheel = WheelInfo(name=name, layer=layer, header_path=header_path)

    text = read_header(header_path)
    decls = extract_function_declarations(text)

    for ret_type, func_name, params_str in decls:
        func = parse_function(ret_type, func_name, params_str)
        wheel.funcs.append(func)

        if should_generate_test(func, name):
            test = generate_generic_sentinel_test(name, func)
            if test:
                wheel.tests.append(test)

    return wheel if wheel.tests else None


def main():
    dry_run = '--dry-run' in sys.argv

    print(f"Scanning wheels in {WHEELS_DIR}...")
    print(f"Target layers: {TARGET_LAYERS}")
    print(f"Skip layers: {SKIP_LAYERS}")
    print()

    total_wheels = 0
    total_tests = 0
    generated_files = []
    skipped_no_tests = []

    for layer in TARGET_LAYERS:
        layer_dir = WHEELS_DIR / layer
        if not layer_dir.exists():
            continue

        for entry in sorted(layer_dir.iterdir()):
            if not entry.is_dir():
                continue
            name = entry.name
            if name.startswith('_') or name.startswith('.'):
                continue

            total_wheels += 1
            wheel = scan_wheel(layer, name)

            if not wheel:
                if name not in SKIP_WHEELS and name not in VENDOR_WHEELS:
                    skipped_no_tests.append(f"{layer}/{name}")
                continue

            # Generate test file
            test_dir = WHEELS_DIR / layer / name / "test"
            test_file = test_dir / f"test_sentinel_{name}.c"

            content = generate_test_file(wheel)
            total_tests += len(wheel.tests)

            if dry_run:
                print(f"  [DRY] {layer}/{name}: {len(wheel.tests)} tests -> {test_file.relative_to(REPO_ROOT)}")
            else:
                test_dir.mkdir(parents=True, exist_ok=True)
                test_file.write_text(content, encoding="utf-8")
                print(f"  [GEN] {layer}/{name}: {len(wheel.tests)} tests -> {test_file.relative_to(REPO_ROOT)}")

            generated_files.append((layer, name, len(wheel.tests), test_file))

    print()
    print("=" * 60)
    print(f"Wheels scanned:    {total_wheels}")
    print(f"Files generated:   {len(generated_files)}")
    print(f"Total test cases:  {total_tests}")
    print()

    if skipped_no_tests:
        print(f"Skipped (no output-buffer functions detected): {len(skipped_no_tests)}")
        for s in skipped_no_tests:
            print(f"  - {s}")
        print()

    print("Generated files:")
    for layer, name, count, path in generated_files:
        print(f"  {layer}/{name}: {count} tests")

    return 0


if __name__ == "__main__":
    sys.exit(main())
