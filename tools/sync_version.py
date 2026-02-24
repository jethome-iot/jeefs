#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or MIT)
"""Synchronize version from version.json to all packages.

Usage:
    python tools/sync_version.py [--check]

Without --check: updates all files in-place.
With --check: exits 1 if any file is out of sync (for CI).
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_JSON = ROOT / "version.json"


def load_version() -> str:
    """Load stable version from version.json."""
    with open(VERSION_JSON) as f:
        data = json.load(f)
    return data["stable"]


def parse_semver(version: str) -> tuple[int, int, int]:
    """Extract major.minor.patch (ignores pre-release)."""
    m = re.match(r"(\d+)\.(\d+)\.(\d+)", version)
    if not m:
        raise ValueError(f"Invalid version: {version}")
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


# --- Updaters ---


def _toml_expected(path: Path, version: str) -> str:
    """Return TOML content with version replaced.

    NOTE: replaces the *first* ``version = "..."`` line
    (count=1).  This assumes the package version appears
    in [project] or [package] before any dependency
    sections — which is the standard TOML convention.
    """
    text = path.read_text()
    return re.sub(
        r'^(version\s*=\s*")[^"]*(")',
        rf"\g<1>{version}\2",
        text,
        count=1,
        flags=re.MULTILINE,
    )


def update_toml_version(path: Path, version: str) -> bool:
    """Update version = "..." in a TOML file."""
    text = path.read_text()
    new_text = _toml_expected(path, version)
    if new_text == text:
        return False
    path.write_text(new_text)
    return True


def check_toml_version(path: Path, version: str) -> bool:
    """Check TOML version (no mutation). True = mismatch."""
    return path.read_text() != _toml_expected(path, version)


def _c_version_header_content(version: str) -> str:
    """Return expected jeefs_version.h content."""
    major, minor, patch = parse_semver(version)
    return f"""\
// DO NOT EDIT — maintained by tools/sync_version.py from version.json
// SPDX-License-Identifier: (GPL-2.0+ or MIT)

#ifndef JEEFS_VERSION_H
#define JEEFS_VERSION_H

#define JEEFS_VERSION "{version}"
#define JEEFS_VERSION_MAJOR {major}
#define JEEFS_VERSION_MINOR {minor}
#define JEEFS_VERSION_PATCH {patch}

#ifdef __cplusplus
extern "C" {{
#endif

static inline const char *jeefs_version(void) {{ return JEEFS_VERSION; }}

#ifdef __cplusplus
}}
#endif

#endif // JEEFS_VERSION_H
"""


def update_c_version_header(path: Path, version: str) -> bool:
    """Regenerate include/jeefs_version.h."""
    content = _c_version_header_content(version)
    old = path.read_text() if path.exists() else ""
    if old == content:
        return False
    path.write_text(content)
    return True


def check_c_version_header(path: Path, version: str) -> bool:
    """Check C header (no mutation). True = mismatch."""
    old = path.read_text() if path.exists() else ""
    return old != _c_version_header_content(version)


# --- Main ---

_PY_TOML = ROOT / "python" / "pyproject.toml"
_RS_TOML = ROOT / "rust" / "jeefs-header" / "Cargo.toml"
_C_HDR = ROOT / "include" / "jeefs_version.h"

TARGETS = [
    ("python/pyproject.toml", _PY_TOML,
     update_toml_version, check_toml_version),
    ("rust/jeefs-header/Cargo.toml", _RS_TOML,
     update_toml_version, check_toml_version),
    ("include/jeefs_version.h", _C_HDR,
     update_c_version_header, check_c_version_header),
]


def main() -> int:
    check_only = "--check" in sys.argv

    version = load_version()
    print(f"version.json -> stable: {version}")

    errors = 0
    for label, path, updater, checker in TARGETS:
        if not path.exists():
            print(f"  SKIP {label} (not found)")
            continue

        if check_only:
            mismatch = checker(path, version)
            if mismatch:
                print(f"  MISMATCH {label}")
                errors += 1
            else:
                print(f"  OK {label}")
        else:
            changed = updater(path, version)
            status = "updated" if changed else "ok"
            print(f"  {status}: {label}")

    if check_only and errors:
        print(
            f"\n{errors} file(s) out of sync. "
            "Run: python tools/sync_version.py"
        )
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
