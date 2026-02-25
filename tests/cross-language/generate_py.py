#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Cross-language header generator (Python): create a .bin header from .json spec.

Usage: generate_py.py <json_file> <output_bin>
"""

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "test-vectors"))
from generate_vectors import generate_binary  # noqa: E402

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <json_file> <output_bin>", file=sys.stderr)
        sys.exit(2)

    spec = json.loads(Path(sys.argv[1]).read_text())
    data = generate_binary(spec)
    Path(sys.argv[2]).write_bytes(data)
    print(f"Generated (Python): {sys.argv[2]} ({len(data)} bytes, version {spec['version']})")
