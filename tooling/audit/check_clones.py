#!/usr/bin/env python3
"""
check_clones.py — detect duplicate or near-duplicate code blocks across wheels.

Q26 audit dimension: copy-paste patterns that should be factored into shared
utilities. Scans all .c files under wheels/, extracts function bodies, normalises
code, hashes blocks, then reports matching hashes that span different wheels.

WHAT IS FLAGGED
  Normalised code blocks (>= N lines) present in 2+ different wheels where the
  block contains meaningful executable logic — not just preprocessor directives,
  file headers, or structural boilerplate.

  Normalisation: strip comments, collapse whitespace, replace all non-keyword
  identifiers/numbers with canonical placeholders so structurally identical
  algorithms are caught even when type or variable names differ.

WHAT IS SUPPRESSED
  - Preprocessor-heavy blocks (>#50% lines starting with #).
  - Blocks whose only diversity is ID/NUM tokens (all-placeholder vocabulary).
  - Init/deinit boilerplate (NULL check + assignment patterns).
  - Short error-return guards.
  - Blocks that score below a minimum "logic density" threshold.

Exit 0 always — informational, not a gate.

Usage:
  check_clones.py                      # human-readable report
  check_clones.py --min-lines 8        # larger minimum block
  check_clones.py --json               # machine-readable JSON to stdout
  check_clones.py --quiet              # summary line only
"""

import argparse
import hashlib
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Repository layout
# ---------------------------------------------------------------------------

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules", "_draft"]

# ---------------------------------------------------------------------------
# ANSI helpers (tty-aware)
# ---------------------------------------------------------------------------

_BOLD   = "\033[1m"
_YELLOW = "\033[33m"
_CYAN   = "\033[36m"
_RESET  = "\033[0m"


def _c(text: str, code: str) -> str:
    if not sys.stdout.isatty():
        return text
    return f"{code}{text}{_RESET}"


# ---------------------------------------------------------------------------
# C source normalisation
# ---------------------------------------------------------------------------

# Tokens that are NOT replaced by a placeholder: C keywords and exact-width
# types.  Everything else (user identifiers, numbers) becomes ID / NUM.
_C_KEYWORDS = frozenset({
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned",
    "void", "volatile", "while",
    # C99 / C11
    "_Bool", "_Complex", "_Imaginary", "bool", "true", "false",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "size_t", "ptrdiff_t", "uintptr_t", "intptr_t",
    "NULL", "EOF",
})

_RE_LINE_COMMENT  = re.compile(r"//[^\n]*")
_RE_BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)
_RE_STRING_LIT    = re.compile(r'"(?:[^"\\]|\\.)*"')
_RE_CHAR_LIT      = re.compile(r"'(?:[^'\\]|\\.)'")
_RE_NUMBER        = re.compile(r"\b0x[0-9a-fA-F]+[uUlL]*\b|\b\d+[uUlLfF]*\b")
_RE_IDENT         = re.compile(r"\b[a-zA-Z_]\w*\b")
_RE_WS            = re.compile(r"[ \t]+")
_RE_PUNCT_ONLY    = re.compile(r"^[{}\[\];,\s]*$")


def _strip_comments(src: str) -> str:
    src = _RE_BLOCK_COMMENT.sub(" ", src)
    src = _RE_LINE_COMMENT.sub("", src)
    return src


def _normalise_line(line: str) -> str:
    """Normalise a single source line for clone detection."""
    # Preserve the preprocessor # as a literal token so we can detect
    # preprocessor-heavy blocks.
    stripped = line.lstrip()
    is_pp = stripped.startswith("#")

    line = _RE_STRING_LIT.sub("SL", line)
    line = _RE_CHAR_LIT.sub("CL", line)
    line = _RE_NUMBER.sub("NUM", line)

    def _replace_ident(m: re.Match) -> str:
        word = m.group(0)
        return word if word in _C_KEYWORDS else "ID"

    line = _RE_IDENT.sub(_replace_ident, line)
    line = _RE_WS.sub(" ", line).strip()
    return line


def _normalise(src: str) -> list[str]:
    """Return list of non-empty normalised lines from *src*."""
    src = _strip_comments(src)
    result = []
    for raw in src.splitlines():
        stripped = raw.lstrip()
        # Skip blank / all-punctuation lines (braces-only) — these appear in
        # every function and would create false-positive sliding windows.
        if not stripped or _RE_PUNCT_ONLY.match(stripped):
            continue
        norm = _normalise_line(raw)
        if norm:
            result.append(norm)
    return result


# ---------------------------------------------------------------------------
# Block quality filters
# ---------------------------------------------------------------------------

_RE_PP_LINE = re.compile(r"^#")


def _is_preprocessor_heavy(lines: list[str]) -> bool:
    """True if > 50% of lines are preprocessor directives (#define, #include…)."""
    if not lines:
        return True
    pp = sum(1 for l in lines if _RE_PP_LINE.match(l))
    return pp / len(lines) > 0.50


# Patterns that, when ALL lines match, mark a block as pure boilerplate
_BOILERPLATE_RE = [
    re.compile(r"^if\s*\(\s*!\s*ID(\s*\|\|\s*!\s*ID)*\s*\)\s*\{?$"),
    re.compile(r"^if\s*\(\s*!\s*ID(\s*\|\|\s*!\s*ID)*\s*\)\s*return\s+"),
    re.compile(r"^return\s+(ID|NUM)\s*;$"),
    re.compile(r"^ID\s*=\s*(ID|NULL|NUM)\s*;$"),
    re.compile(r"^ID\s*->\s*ID\s*=\s*(ID|NULL|NUM)\s*;$"),
    re.compile(r"^memset\s*\("),
    re.compile(r"^\}\s*$"),
    re.compile(r"^\{$"),
]


def _is_all_boilerplate(lines: list[str]) -> bool:
    for line in lines:
        if not any(p.search(line) for p in _BOILERPLATE_RE):
            return False
    return True


# Vocabulary: a block where every token is ID/NUM/SL/CL or punctuation has
# no semantic information — it is pure scaffold structure.
_RE_VOCAB_TOKEN = re.compile(r"\b(ID|NUM|SL|CL)\b")
_RE_KEYWORD_TOKEN = re.compile(
    r"\b(if|for|while|return|static|void|bool|int|unsigned|const|struct|"
    r"uint8_t|uint16_t|uint32_t|uint64_t|int16_t|int32_t|sizeof|else)\b"
)


def _has_enough_vocabulary(lines: list[str]) -> bool:
    """Return True if the block contains real control-flow or operator tokens
    beyond pure placeholder substitutions.

    A block that normalises to only '#ID ID ID' lines (preprocessor) or only
    'ID = ID; return ID;' lines (trivial assignment chain) would fail this.
    We require at least 2 distinct keyword tokens OR at least 1 operator
    sequence that implies an algorithm (bit-ops, arithmetic, array indexing).
    """
    joined = " ".join(lines)
    kw_hits = set(_RE_KEYWORD_TOKEN.findall(joined))
    # Require at least 2 distinct control-flow / type keywords
    if len(kw_hits) >= 2:
        return True
    # Or at least one operator that implies computation
    if re.search(r"[<>|&^]{1,2}|<<|>>|\+=|-=|\*=|/=|%=", joined):
        return True
    return False


def _should_suppress(lines: list[str]) -> bool:
    """Return True if this block should not be reported as a clone."""
    if _is_preprocessor_heavy(lines):
        return True
    if _is_all_boilerplate(lines):
        return True
    if not _has_enough_vocabulary(lines):
        return True
    return False


# ---------------------------------------------------------------------------
# Sliding-window block extractor
# ---------------------------------------------------------------------------

def _sliding_blocks(norm_lines: list[str], min_lines: int) -> list[tuple[int, list[str]]]:
    n = len(norm_lines)
    blocks = []
    for start in range(n - min_lines + 1):
        block = norm_lines[start:start + min_lines]
        blocks.append((start, block))
    return blocks


def _block_hash(lines: list[str]) -> str:
    key = "\n".join(lines)
    return hashlib.sha256(key.encode()).hexdigest()[:16]


# ---------------------------------------------------------------------------
# Wheel discovery
# ---------------------------------------------------------------------------

def _find_c_files() -> list[tuple[str, str, Path]]:
    """Return [(wheel_key, layer, path)] for all wheel .c source files.

    wheel_key = "layer/name" — same for all .c files inside one wheel directory.
    """
    result = []
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for wheel_dir in sorted(layer_dir.iterdir()):
            if not wheel_dir.is_dir():
                continue
            wheel_key = f"{layer}/{wheel_dir.name}"
            for c_file in sorted(wheel_dir.rglob("*.c")):
                rel = c_file.relative_to(wheel_dir)
                parts = rel.parts
                if any(p in {"test", "build", "build_audit", "__pycache__", "vendor"}
                       for p in parts):
                    continue
                result.append((wheel_key, layer, c_file))
    return result


# ---------------------------------------------------------------------------
# Main analysis
# ---------------------------------------------------------------------------

def _analyse(min_lines: int) -> dict:
    """Run clone detection and return structured results."""
    c_files = _find_c_files()

    # hash -> list of (wheel_key, file_path, line_number_1based)
    hash_map: dict[str, list[tuple[str, Path, int]]] = defaultdict(list)
    suppressed = 0
    total_blocks = 0

    for wheel_key, _layer, c_path in c_files:
        try:
            src = c_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue

        norm_lines = _normalise(src)
        if len(norm_lines) < min_lines:
            continue

        # Build a line-number map: norm_index -> approximate original line no.
        # We map each normalised line to the raw line number where it came from.
        stripped_src = _strip_comments(src)
        norm_to_raw: list[int] = []
        for raw_idx, raw_line in enumerate(stripped_src.splitlines()):
            stripped = raw_line.lstrip()
            if not stripped or _RE_PUNCT_ONLY.match(stripped):
                continue
            norm = _normalise_line(raw_line)
            if norm:
                norm_to_raw.append(raw_idx + 1)  # 1-based

        for start, block in _sliding_blocks(norm_lines, min_lines):
            total_blocks += 1
            if _should_suppress(block):
                suppressed += 1
                continue
            h = _block_hash(block)
            orig_line = norm_to_raw[start] if start < len(norm_to_raw) else 0
            hash_map[h].append((wheel_key, c_path, orig_line))

    # Keep only hashes found in 2+ different wheels
    clone_groups = []
    for h, occurrences in hash_map.items():
        # One occurrence per wheel (keep first seen)
        seen_wheels: dict[str, tuple[Path, int]] = {}
        for wheel_key, fpath, lineno in occurrences:
            if wheel_key not in seen_wheels:
                seen_wheels[wheel_key] = (fpath, lineno)

        if len(seen_wheels) < 2:
            continue

        # Recover normalised sample from first occurrence
        first_wheel = next(iter(seen_wheels))
        first_path, first_line = seen_wheels[first_wheel]
        sample_lines = None
        try:
            src = first_path.read_text(encoding="utf-8", errors="replace")
            norm_lines = _normalise(src)
            for start, block in _sliding_blocks(norm_lines, min_lines):
                if _block_hash(block) == h:
                    sample_lines = block
                    break
        except OSError:
            sample_lines = ["<unavailable>"]

        occ_list = [
            {
                "wheel": wheel_key,
                "file": str(fpath.relative_to(REPO)),
                "line": lineno,
            }
            for wheel_key, (fpath, lineno) in sorted(seen_wheels.items())
        ]

        clone_groups.append({
            "hash": h,
            "line_count": min_lines,
            "normalized_sample": sample_lines or [],
            "occurrences": occ_list,
        })

    # Sort: more wheels first, then hash for stability
    clone_groups.sort(key=lambda g: (-len(g["occurrences"]), g["hash"]))

    wheel_keys = {wk for wk, _layer, _path in c_files}
    return {
        "scanned_files": len(c_files),
        "scanned_wheels": len(wheel_keys),
        "total_blocks_checked": total_blocks,
        "suppressed_boilerplate": suppressed,
        "clone_groups": clone_groups,
    }


# ---------------------------------------------------------------------------
# Refactor hints
# ---------------------------------------------------------------------------

_REFACTOR_HINTS = [
    # (regex on joined normalised sample, hint message)
    (re.compile(r"uint16_t.*buf.*<<.*\|"),
     "16-bit register unpacking → shared parse_le16() / parse_be16() helper"),
    (re.compile(r"uint16_t.*>>.*&"),
     "16-bit big-endian unpacking → shared parse_be16() helper"),
    (re.compile(r"\^\s*ID\s*\[ID\].*for.*uint8_t"),
     "bit-by-bit CRC loop → shared crc_bitwise_step() in crc_any wheel"),
    (re.compile(r"write_reg.*read_regs.*i2c"),
     "I2C register shim → shared i2c_reg_io wrapper"),
    (re.compile(r"SPI.*CS.*transfer"),
     "SPI register shim → shared spi_reg_io wrapper"),
    (re.compile(r"ID\s*\[\s*ID\s*\]\s*=.*>>\s*NUM.*ID\s*\[\s*ID\s*\]\s*=.*&"),
     "byte-packing helper → shared pack_be16() / pack_le16() utility"),
    (re.compile(r"for.*NUM.*ID\s*<<=\s*NUM.*\^="),
     "LFSR / CRC shift loop → shared bitwise CRC primitive"),
]


def _suggest_refactor(group: dict) -> str:
    sample = " ".join(group["normalized_sample"])
    for pat, hint in _REFACTOR_HINTS:
        if pat.search(sample):
            return hint
    return ""


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def _print_human(results: dict, quiet: bool) -> None:
    groups     = results["clone_groups"]
    n_files    = results["scanned_files"]
    n_wheels   = results["scanned_wheels"]
    suppressed = results["suppressed_boilerplate"]
    total      = results["total_blocks_checked"]

    if not quiet:
        print(_c("check_clones -- semantic duplicate detector", _BOLD))
        print(f"  scanned     : {n_files} .c files, {n_wheels} wheels")
        print(f"  blocks      : {total} evaluated, {suppressed} suppressed (boilerplate/preprocessor)")
        print()

    if not groups:
        msg = "No semantic clones detected."
        if quiet:
            print(msg)
        else:
            print(_c(msg, _BOLD))
        return

    print(_c(f"{len(groups)} clone group(s) found "
             f"(use --min-lines to adjust sensitivity):", _BOLD))
    print()

    for i, group in enumerate(groups, 1):
        occ = group["occurrences"]
        n_occ = len(occ)
        hint = _suggest_refactor(group)
        print(_c(
            f"[{i}] {n_occ} wheels -- {group['line_count']}-line block"
            f"  (hash {group['hash']})",
            _YELLOW
        ))
        if hint:
            print(_c(f"     Refactor hint: {hint}", _CYAN))
        print("     Wheels:")
        for o in occ:
            print(f"       {o['wheel']}  ({o['file']}:{o['line']})")
        if not quiet:
            print("     Normalised block:")
            for ln in group["normalized_sample"]:
                print(f"       {ln}")
        print()

    # Summary by layer pair
    if not quiet:
        pair_counts: dict[tuple, int] = defaultdict(int)
        for group in groups:
            layers = tuple(sorted({o["wheel"].split("/")[0]
                                    for o in group["occurrences"]}))
            pair_counts[layers] += 1

        print(_c("Clone distribution by layer:", _BOLD))
        for pair, count in sorted(pair_counts.items(), key=lambda x: -x[1]):
            print(f"  {' + '.join(pair):40s}  {count:3d}")
        print()


def _print_json(results: dict) -> None:
    slim = []
    for g in results["clone_groups"]:
        slim.append({
            "hash": g["hash"],
            "line_count": g["line_count"],
            "normalized_sample": g["normalized_sample"][:4],
            "occurrences": g["occurrences"],
        })
    out = {
        "scanned_files":          results["scanned_files"],
        "scanned_wheels":         results["scanned_wheels"],
        "clone_count":            len(results["clone_groups"]),
        "suppressed_boilerplate": results["suppressed_boilerplate"],
        "clone_groups":           slim,
    }
    json.dump(out, sys.stdout, indent=2)
    print()


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Detect semantic duplicate code blocks across wheels (Q26)."
    )
    parser.add_argument(
        "--min-lines", "-n",
        type=int, default=5, metavar="N",
        help="minimum block size to consider (default: 5)"
    )
    parser.add_argument(
        "--json", action="store_true",
        help="emit machine-readable JSON to stdout"
    )
    parser.add_argument(
        "--quiet", "-q", action="store_true",
        help="suppress normalised-block detail; show only clone locations"
    )
    args = parser.parse_args()

    if args.min_lines < 2:
        parser.error("--min-lines must be >= 2")

    results = _analyse(args.min_lines)

    if args.json:
        _print_json(results)
    else:
        _print_human(results, args.quiet)

    sys.exit(0)


if __name__ == "__main__":
    main()
