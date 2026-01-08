#!/usr/bin/env python3
# scripts/increment_build.py
# Increment the BUILD number in src/version.h on each PlatformIO build.
# Safe: makes a .bak before writing and returns cleanly if file/pattern not found.

from pathlib import Path
import re


VERSION_H = Path("src/version.h")
BACKUP_SUFFIX = ".bak"


def increment_build():
    if not VERSION_H.exists():
        print(f"[increment_build] warning: {VERSION_H} not found, skipping build increment")
        return

    text = VERSION_H.read_text(encoding="utf-8")

    # Match: static constexpr uint16_t BUILD = 123;
    pattern = re.compile(r'(static\s+constexpr\s+uint16_t\s+BUILD\s*=\s*)(\d+)(\s*;)', re.M)

    m = pattern.search(text)
    if not m:
        print(f"[increment_build] warning: BUILD pattern not found in {VERSION_H}, skipping")
        return

    old = int(m.group(2))
    new = old + 1

    new_text = pattern.sub(lambda m: m.group(1) + str(new) + m.group(3), text, count=1)

    # Backup original file
    bak = VERSION_H.with_name(VERSION_H.name + BACKUP_SUFFIX)
    bak.write_text(text, encoding="utf-8")

    # Write updated file atomically
    tmp = VERSION_H.with_suffix(VERSION_H.suffix + ".tmp")
    tmp.write_text(new_text, encoding="utf-8")
    tmp.replace(VERSION_H)

    print(f"[increment_build] updated BUILD: {old} -> {new} ({VERSION_H})")


# When PlatformIO imports this script as an "extra_script" it executes the
# module. Call `increment_build()` at import time so the BUILD is updated
# before the project build proceeds. Avoid `sys.exit()` to not terminate the
# SCons/PlatformIO process.

increment_build()

