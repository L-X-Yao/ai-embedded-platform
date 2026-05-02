#!/usr/bin/env python3
"""Phase 2: fix Instances field, Deps format, I2C delay_ms, HAL pointer storage,
unit suffixes in data structs. Run after fix_convention_v2.py."""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WHEELS = REPO / "wheels"
LAYERS = ["primitives", "drivers", "protocols", "modules"]

SINGLE_INSTANCE_WHEELS = {"log", "log_color", "log_dedup", "log_file", "log_filter",
                          "log_flash", "log_rate", "log_ringbuf", "log_rtt", "log_uart"}

UNIT_MAP = {
    "temperature": "temperature_c",
    "temp": "temperature_c",
    "pressure": "pressure_pa",
    "humidity": "humidity_pct",
    "altitude": "altitude_m",
    "voltage": "voltage_mv",
    "current": "current_ma",
    "power": "power_mw",
    "shunt_voltage": "shunt_voltage_uv",
    "bus_voltage": "bus_voltage_mv",
    "lux": "lux",
    "illuminance": "illuminance_lux",
    "heading": "heading_deg",
    "angle": "angle_deg",
    "weight": "weight_raw",
    "proximity": "proximity_raw",
    "distance": "distance_mm",
    "range": "range_mm",
}

stats = {"instances": 0, "deps_fmt": 0, "delay_ms": 0, "hal_ptr": 0, "units": 0}


def find_wheels():
    for layer in LAYERS:
        layer_dir = WHEELS / layer
        if not layer_dir.is_dir():
            continue
        for entry in sorted(layer_dir.iterdir()):
            if entry.is_dir() and entry.name not in {"test", "build", "__pycache__"}:
                if (entry / "vendor").is_dir():
                    continue
                yield layer, entry.name, entry


def fix_instances_field(wheel_dir, name):
    """Add Instances: multi/single to file headers."""
    inst = "single" if name in SINGLE_INSTANCE_WHEELS else "multi"
    changed = False
    for ext in (".h", ".c"):
        f = wheel_dir / f"{name}{ext}"
        if not f.exists():
            continue
        text = f.read_text(encoding="utf-8")
        if "Instances:" in text:
            continue
        # Find Deps: line and add Instances after it
        new_text = re.sub(
            r"(\n \* Deps:\s+.*)",
            rf"\1\n * Instances: {inst}",
            text,
            count=1,
        )
        if new_text != text:
            f.write_text(new_text, encoding="utf-8")
            changed = True
    return changed


def fix_deps_format(wheel_dir, name):
    """Normalize Deps: line format."""
    changed = False
    for ext in (".h", ".c"):
        f = wheel_dir / f"{name}{ext}"
        if not f.exists():
            continue
        text = f.read_text(encoding="utf-8")
        new_text = text
        # Fix "Deps:    i2c_hal" → "Deps:      i2c_hal (injected)"
        new_text = re.sub(
            r"(\* Deps:\s+)(i2c_hal|spi_hal|gpio_hal|onewire_hal|pwm_hal)(\s*\n)",
            r"\1\2 (injected)\3",
            new_text,
        )
        # Fix "Deps:    (injected)" → need to find the HAL name
        # Fix inconsistent spacing: normalize to single-space after "Deps:"
        new_text = re.sub(r"(\* Deps:)\s{2,}", r"\1      ", new_text)
        if new_text != text:
            f.write_text(new_text, encoding="utf-8")
            changed = True
    return changed


def fix_i2c_delay_ms(wheel_dir, name):
    """Add delay_ms callback to I2C HAL structs if missing."""
    h_file = wheel_dir / f"{name}.h"
    if not h_file.exists():
        return False
    text = h_file.read_text(encoding="utf-8")
    if "_i2c_t" not in text:
        return False
    if "delay_ms" in text:
        return False
    # Find the I2C HAL struct and add delay_ms before void *ctx
    new_text = re.sub(
        r"(bool \(\*read_regs?\)\(void \*ctx.*?\);)\n(\s+void\s+\*ctx;)",
        r"\1\n    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */\n\2",
        text,
        flags=re.DOTALL,
    )
    # Also handle command-based (Pattern B) where read is the last callback
    if new_text == text:
        new_text = re.sub(
            r"(bool \(\*read\)\(void \*ctx.*?\);)\n(\s+void\s+\*ctx;)",
            r"\1\n    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */\n\2",
            text,
            flags=re.DOTALL,
        )
    if new_text != text:
        h_file.write_text(new_text, encoding="utf-8")
        return True
    return False


def fix_hal_pointer_storage(wheel_dir, name):
    """Change HAL struct copy to const pointer in _t context."""
    h_file = wheel_dir / f"{name}.h"
    c_file = wheel_dir / f"{name}.c"
    if not h_file.exists():
        return False
    h_text = h_file.read_text(encoding="utf-8")

    # Detect copy-by-value patterns: <name>_gpio_t gpio; or <name>_ow_t ow; or <name>_pwm_t pwm;
    hal_patterns = [
        (rf"(\s+){name}_gpio_t\s+gpio;", "gpio", f"{name}_gpio_t"),
        (rf"(\s+){name}_ow_t\s+ow;", "ow", f"{name}_ow_t"),
        (rf"(\s+){name}_pwm_t\s+pwm;", "pwm", f"{name}_pwm_t"),
    ]

    found_hal = None
    for pat, field, type_name in hal_patterns:
        if re.search(pat, h_text):
            found_hal = (pat, field, type_name)
            break

    if not found_hal:
        return False

    pat, field, type_name = found_hal

    # Check if already a pointer
    if f"const {type_name} *{field}" in h_text:
        return False

    # Fix header: value → const pointer
    new_h = re.sub(
        rf"(\s+){type_name}\s+{field};(\s*/\*.*?\*/)?",
        rf"\1const {type_name} *{field};\2",
        h_text,
    )
    if new_h == h_text:
        return False

    h_file.write_text(new_h, encoding="utf-8")

    # Fix .c file
    if c_file.exists():
        c_text = c_file.read_text(encoding="utf-8")
        new_c = c_text
        # Change dev->gpio = *gpio; → dev->gpio = gpio;
        new_c = new_c.replace(f"dev->{field} = *{field};", f"dev->{field} = {field};")
        # Change dev->field.xxx( → dev->field->xxx(
        new_c = re.sub(rf"dev->{field}\.", f"dev->{field}->", new_c)
        if new_c != c_text:
            c_file.write_text(new_c, encoding="utf-8")

    # Fix test file
    test_c = wheel_dir / "test" / f"test_{name}.c"
    if test_c.exists():
        t_text = test_c.read_text(encoding="utf-8")
        new_t = re.sub(rf"dev\.{field}\.", f"dev.{field}->", t_text)
        if new_t != t_text:
            test_c.write_text(new_t, encoding="utf-8")

    return True


def fix_unit_suffixes(wheel_dir, name):
    """Add unit suffixes to _data_t struct fields."""
    h_file = wheel_dir / f"{name}.h"
    if not h_file.exists():
        return False
    text = h_file.read_text(encoding="utf-8")
    if "_data_t" not in text:
        return False

    # Extract the data struct
    m = re.search(rf"typedef struct \{{(.*?)\}} {name}_data_t;", text, re.DOTALL)
    if not m:
        return False

    struct_body = m.group(1)
    new_body = struct_body
    changed = False

    for old_name, new_name in UNIT_MAP.items():
        if old_name == new_name:
            continue
        # Match field declarations like "float temperature;" or "int32_t pressure;"
        pattern = rf"(\b(?:float|double|int(?:8|16|32)_t|uint(?:8|16|32)_t)\s+){old_name}(\s*;)"
        if re.search(pattern, new_body):
            # Don't replace if already has a unit suffix
            if not re.search(rf"\b{old_name}_[a-z]+\b", new_body):
                new_body = re.sub(pattern, rf"\1{new_name}\2", new_body)
                changed = True

    if not changed:
        return False

    new_text = text.replace(struct_body, new_body)

    # Also fix references in .c and test files
    for old_name, new_name in UNIT_MAP.items():
        if old_name == new_name:
            continue
        if old_name in struct_body and new_name in new_body:
            # Replace field access patterns: out->temperature → out->temperature_c
            new_text = re.sub(rf"->({old_name})(\s*[;=\)])", rf"->{new_name}\2", new_text)

    h_file.write_text(new_text, encoding="utf-8")

    # Fix .c file
    c_file = wheel_dir / f"{name}.c"
    if c_file.exists():
        c_text = c_file.read_text(encoding="utf-8")
        new_c = c_text
        for old_name, new_name in UNIT_MAP.items():
            if old_name == new_name or old_name not in struct_body:
                continue
            if new_name in new_body:
                new_c = re.sub(rf"->({old_name})(\s*[;=\)\+\-\*\/])", rf"->{new_name}\2", new_c)
                new_c = re.sub(rf"\.({old_name})(\s*[;=\)\+\-\*\/])", rf".{new_name}\2", new_c)
        if new_c != c_text:
            c_file.write_text(new_c, encoding="utf-8")

    # Fix test file
    test_c = wheel_dir / "test" / f"test_{name}.c"
    if test_c.exists():
        t_text = test_c.read_text(encoding="utf-8")
        new_t = t_text
        for old_name, new_name in UNIT_MAP.items():
            if old_name == new_name or old_name not in struct_body:
                continue
            if new_name in new_body:
                new_t = re.sub(rf"->({old_name})(\s*[;=\)\+\-\*\/\,])", rf"->{new_name}\2", new_t)
                new_t = re.sub(rf"\.({old_name})(\s*[;=\)\+\-\*\/\,\<\>])", rf".{new_name}\2", new_t)
        if new_t != t_text:
            test_c.write_text(new_t, encoding="utf-8")

    return True


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "all"

    for layer, name, wheel_dir in find_wheels():
        path = f"wheels/{layer}/{name}"

        if mode in ("all", "instances"):
            if fix_instances_field(wheel_dir, name):
                stats["instances"] += 1
                print(f"  INST    {path}")

        if mode in ("all", "deps"):
            if fix_deps_format(wheel_dir, name):
                stats["deps_fmt"] += 1
                print(f"  DEPS    {path}")

        if mode in ("all", "delay"):
            if fix_i2c_delay_ms(wheel_dir, name):
                stats["delay_ms"] += 1
                print(f"  DELAY   {path}")

        if mode in ("all", "ptr"):
            if fix_hal_pointer_storage(wheel_dir, name):
                stats["hal_ptr"] += 1
                print(f"  PTR     {path}")

        if mode in ("all", "units"):
            if fix_unit_suffixes(wheel_dir, name):
                stats["units"] += 1
                print(f"  UNITS   {path}")

    print(f"\n  INST: {stats['instances']}  DEPS: {stats['deps_fmt']}  "
          f"DELAY: {stats['delay_ms']}  PTR: {stats['hal_ptr']}  UNITS: {stats['units']}")


if __name__ == "__main__":
    main()
