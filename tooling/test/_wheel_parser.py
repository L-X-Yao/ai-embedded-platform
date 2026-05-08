"""_wheel_parser.py -- shared C header parsing primitives for wheel test generators.

Provides the common parsing layer used by gen_null_tests.py and
gen_boundary_tests.py: comment stripping, multiline-declaration joining,
parameter splitting/parsing, prefix detection, function discovery, and return-
type classification.

Do not run this module directly; import it from a generator script.
"""

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Repository layout
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
WHEELS_DIR = REPO_ROOT / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules", "_draft"]

# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------


@dataclass
class Param:
    full_text: str      # e.g. "const uint8_t *data"
    type_text: str      # e.g. "const uint8_t *"
    name: str           # e.g. "data"
    is_pointer: bool
    is_func_ptr: bool   # function pointer param (skip NULL test)
    # Optional fields populated by parse_param when compute_extra=True
    is_integer: bool = False    # True when type is a plain integer (not pointer)
    base_type: str = ""         # cleaned type without const/volatile/*


@dataclass
class FuncDecl:
    return_type: str    # e.g. "ring_buffer_err_t"
    name: str           # e.g. "ring_buffer_init"
    params: list        # list of Param
    raw_line: str       # original declaration text


@dataclass
class WheelInfo:
    name: str
    layer: str
    path: Path
    header_path: Path
    funcs: list = field(default_factory=list)


# ---------------------------------------------------------------------------
# Compiled regexes
# ---------------------------------------------------------------------------

# Matches a C function declaration: <return_type> <func_name>(<params>);
# Works on multi-line declarations after they've been joined into one line.
FUNC_RE = re.compile(
    r'^'
    r'([\w\s\*]+?)'           # return type (greedy but minimal)
    r'\s+'
    r'(\w+)'                   # function name
    r'\s*\('
    r'([^)]*)'                 # parameter list
    r'\)\s*;',
    re.MULTILINE
)

# Detect typedef'd function pointer params like: soft_timer_cb_t cb
# These are names ending in _cb_t, _fn_t, _fn, _handler_t, etc.
FUNC_PTR_TYPEDEF_RE = re.compile(r'_(?:cb|fn|handler|callback)_t$|_fn$')

# Integer base types recognised for boundary testing
INTEGER_TYPES = frozenset({
    'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
    'int8_t', 'int16_t', 'int32_t', 'int64_t',
    'int', 'unsigned', 'unsigned int', 'size_t',
})

# ---------------------------------------------------------------------------
# Header reading
# ---------------------------------------------------------------------------


def read_header(header_path: Path) -> str:
    """Read header content, stripping block and line comments."""
    text = header_path.read_text(encoding="utf-8", errors="replace")
    # Remove block comments (including multi-line)
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.DOTALL)
    # Remove line comments
    text = re.sub(r'//[^\n]*', '', text)
    return text


# ---------------------------------------------------------------------------
# Multiline declaration joining
# ---------------------------------------------------------------------------


def join_multiline_decls(text: str) -> str:
    """Join lines that are part of a single function declaration.

    A declaration starts with a return type and ends with );  Lines that don't
    terminate with ; { or } are continuation lines and are merged into the
    previous buffer.  Preprocessor lines flush the buffer immediately.
    """
    lines = text.split('\n')
    joined = []
    buf = ""
    for line in lines:
        stripped = line.strip()
        if not stripped:
            if buf:
                buf += " "
            continue
        # Preprocessor directives: flush current buffer first
        if stripped.startswith('#'):
            if buf:
                joined.append(buf)
                buf = ""
            continue
        buf += " " + stripped if buf else stripped
        # Semicolon closes a declaration
        if stripped.endswith(';'):
            joined.append(buf)
            buf = ""
        elif stripped.endswith('{') or stripped.endswith('}'):
            joined.append(buf)
            buf = ""
    if buf:
        joined.append(buf)
    return '\n'.join(joined)


# ---------------------------------------------------------------------------
# Parameter parsing
# ---------------------------------------------------------------------------


def split_params(params_str: str) -> list:
    """Split a parameter list on commas, respecting nested parentheses."""
    if not params_str or params_str.strip() == 'void':
        return []

    parts = []
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
            parts.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        parts.append(current.strip())
    return parts


def parse_param(param_text: str, compute_extra: bool = False) -> Optional[Param]:
    """Parse a single C parameter into a Param dataclass.

    Args:
        param_text: Raw parameter text, e.g. ``"const uint8_t *data"``.
        compute_extra: When True, populate ``is_integer`` and ``base_type``
            (needed by gen_boundary_tests.py).  Defaults to False so that
            gen_null_tests.py keeps the same behaviour as before extraction.
    """
    param_text = param_text.strip()
    if not param_text or param_text == 'void':
        return None

    # Variadic
    if param_text == '...' or param_text.startswith('...'):
        return None

    # Explicit function pointer: e.g. "bool (*write_reg)(void *, ...)"
    fp_match = re.match(r'.*\(\s*\*\s*(\w+)\s*\)\s*\(', param_text)
    if fp_match:
        return Param(
            full_text=param_text,
            type_text=param_text,
            name=fp_match.group(1),
            is_pointer=True,
            is_func_ptr=True,
            is_integer=False,
            base_type='',
        )

    # Track whether the original text had array brackets before stripping them
    has_array_brackets = bool(re.search(r'\[', param_text))

    # Remove array brackets: "uint8_t buf[16]" -> "uint8_t buf"
    param_clean = re.sub(r'\[[^\]]*\]', '', param_text).strip()

    # Split off the last word as name; the rest is the type
    parts = param_clean.rsplit(None, 1)
    if len(parts) < 2:
        bt = re.sub(r'\s*\*\s*', '', param_clean).replace('const', '').replace('volatile', '').strip()
        is_ptr = '*' in param_clean or has_array_brackets
        return Param(
            full_text=param_text,
            type_text=param_clean,
            name="",
            is_pointer=is_ptr,
            is_func_ptr=False,
            is_integer=(bt in INTEGER_TYPES and not is_ptr) if compute_extra else False,
            base_type=bt if compute_extra else '',
        )

    type_text, name = parts

    # Leading * on the name belongs to the type
    while name.startswith('*'):
        type_text += ' *'
        name = name[1:]

    is_pointer = '*' in type_text or has_array_brackets
    base_type = re.sub(r'\s*\*\s*', '', type_text).replace('const', '').replace('volatile', '').strip()
    is_func_ptr_typedef = bool(FUNC_PTR_TYPEDEF_RE.search(base_type))

    return Param(
        full_text=param_text,
        type_text=type_text.strip(),
        name=name.strip(),
        is_pointer=is_pointer,
        is_func_ptr=is_func_ptr_typedef,
        is_integer=(base_type in INTEGER_TYPES and not is_pointer) if compute_extra else False,
        base_type=base_type if compute_extra else '',
    )


# ---------------------------------------------------------------------------
# Prefix detection (LCP algorithm)
# ---------------------------------------------------------------------------


def detect_prefix(header_text: str, wheel_name: str) -> str:
    """Detect the common function-name prefix declared in *header_text*.

    Many wheels use abbreviated prefixes (hex_codec -> hex_,
    fixed_point -> fp_, string_utils -> str_).  Priority order:

    1. Full wheel name + '_' (e.g. ring_buffer_)
    2. Error-enum prefix (searches for <PREFIX>_ERR_PARAM in the header)
    3. Longest common prefix (LCP) of all candidate function names
    4. Empty string (accept all functions)
    """
    text = join_multiline_decls(header_text)

    # Collect candidate non-static, non-typedef function names
    candidates = []
    for m in FUNC_RE.finditer(text):
        ret = m.group(1).strip()
        name = m.group(2).strip()
        if 'static' in ret or 'typedef' in ret:
            continue
        candidates.append(name)

    if not candidates:
        return wheel_name + '_'

    # 1) Full wheel_name prefix
    full_prefix = wheel_name + '_'
    if any(c.startswith(full_prefix) for c in candidates):
        return full_prefix

    # 2) Error-enum prefix
    err_re = re.compile(r'(\w+)_ERR_PARAM')
    for em in err_re.finditer(header_text):
        prefix = em.group(1).lower() + '_'
        if any(c.startswith(prefix) for c in candidates):
            return prefix

    # 3) LCP among all candidates
    if len(candidates) >= 2:
        lcp = candidates[0]
        for c in candidates[1:]:
            while not c.startswith(lcp):
                lcp = lcp[:-1]
                if not lcp:
                    break
            if not lcp:
                break
        # Trim to last underscore for a clean boundary
        if '_' in lcp:
            lcp = lcp[:lcp.rfind('_') + 1]
        if lcp and len(lcp) >= 2:
            return lcp

    # 4) Fallback: accept everything
    return ''


# ---------------------------------------------------------------------------
# Function declaration parsing
# ---------------------------------------------------------------------------


def parse_functions(header_text: str, wheel_name: str,
                    compute_extra: bool = False) -> list:
    """Parse all public function declarations from *header_text*.

    Args:
        header_text: Preprocessed (comment-stripped) header content.
        wheel_name: Used for prefix detection.
        compute_extra: Passed through to :func:`parse_param`; when True the
            ``is_integer`` / ``base_type`` fields on each :class:`Param` are
            populated (required by gen_boundary_tests.py).

    Returns:
        List of :class:`FuncDecl`.
    """
    text = join_multiline_decls(header_text)
    prefix = detect_prefix(header_text, wheel_name)
    funcs = []

    for m in FUNC_RE.finditer(text):
        ret_type = m.group(1).strip()
        func_name = m.group(2).strip()
        params_str = m.group(3).strip()

        # Filter by detected prefix
        if prefix and not func_name.startswith(prefix):
            continue

        # Skip static helpers and typedef'd function types
        if 'static' in ret_type or 'typedef' in ret_type:
            continue

        params = split_params(params_str)
        parsed_params = []
        for p in params:
            pp = parse_param(p, compute_extra=compute_extra)
            if pp:
                parsed_params.append(pp)

        funcs.append(FuncDecl(
            return_type=ret_type,
            name=func_name,
            params=parsed_params,
            raw_line=m.group(0),
        ))

    return funcs


# ---------------------------------------------------------------------------
# Return-type classification
# ---------------------------------------------------------------------------


def classify_return_type(ret_type: str, wheel_name: str = "") -> str:
    """Classify a C return type for assertion-strategy selection.

    Returns one of: ``'err_t'``, ``'int_return'``, ``'unsigned'``,
    ``'bool'``, ``'void'``, ``'pointer'``, ``'other'``.
    """
    rt = ret_type.strip()

    if '_err_t' in rt:
        return 'err_t'

    if rt == 'int':
        return 'int_return'

    if rt in ('uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
              'size_t', 'unsigned', 'unsigned int'):
        return 'unsigned'

    # Signed integer returns (not plain int): treat as "return 0"
    if rt in ('int8_t', 'int16_t', 'int32_t', 'int64_t'):
        return 'unsigned'

    # State/event enum types
    if rt.startswith('sm_state_t') or rt.startswith('sm_event_t'):
        return 'unsigned'

    if rt == 'bool':
        return 'bool'

    if rt == 'void':
        return 'void'

    if '*' in rt:
        return 'pointer'

    return 'other'


# ---------------------------------------------------------------------------
# Default scalar value
# ---------------------------------------------------------------------------


def scalar_default(type_text: str) -> str:
    """Return a safe C literal for a non-pointer scalar parameter."""
    t = type_text.replace('const ', '').strip()
    if t == 'bool':
        return 'false'
    if t == 'float':
        return '0.0f'
    if t == 'double':
        return '0.0'
    if t in ('char', 'int8_t', 'uint8_t'):
        return '0'
    if t in ('size_t', 'uint16_t', 'uint32_t', 'uint64_t',
             'int16_t', 'int32_t', 'int64_t', 'int', 'unsigned int',
             'unsigned', 'sm_state_t', 'sm_event_t'):
        return '0'
    # Enum types: 0 is almost always a valid sentinel
    if '_t' in t:
        return '0'
    return '0'


# ---------------------------------------------------------------------------
# Wheel / vendor helpers
# ---------------------------------------------------------------------------


def is_vendor_wheel(wheel_path: Path) -> bool:
    """Return True if *wheel_path* has a ``vendor/`` subdirectory."""
    return (wheel_path / "vendor").is_dir()


def discover_wheels(skip_layers: Optional[set] = None,
                    skip_wheels: Optional[set] = None) -> list:
    """Discover all non-vendor wheels that have a matching public header.

    Args:
        skip_layers: Optional set of layer names to exclude entirely
            (e.g. ``{"drivers"}``).
        skip_wheels: Optional set of wheel names to exclude
            (e.g. ``{"telemetry", "cli_shell"}``).

    Returns:
        List of :class:`WheelInfo` sorted by layer then name.
    """
    skip_layers = skip_layers or set()
    skip_wheels = skip_wheels or set()
    wheels = []

    for layer in LAYERS:
        if layer in skip_layers:
            continue
        layer_dir = WHEELS_DIR / layer
        if not layer_dir.is_dir():
            continue

        for entry in sorted(layer_dir.iterdir()):
            if not entry.is_dir():
                continue
            if entry.name.startswith('_') or entry.name.startswith('.'):
                continue
            if is_vendor_wheel(entry):
                continue
            if entry.name in skip_wheels:
                continue

            header = entry / f"{entry.name}.h"
            if not header.is_file():
                continue

            wheels.append(WheelInfo(
                name=entry.name,
                layer=layer,
                path=entry,
                header_path=header,
            ))

    return wheels
