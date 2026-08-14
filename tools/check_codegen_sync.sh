#!/bin/sh
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
# Verify that the committed generated files match docs/format/*.md.
# Used by the prek local hook; CI runs the same checks in codegen-check.
set -e
cd "$(dirname "$0")"
SPECS="../docs/format/header-common.md ../docs/format/header-v1.md \
../docs/format/header-v2.md ../docs/format/header-v3.md \
../docs/format/header-v4.md ../docs/format/device-identity-v1.md \
../docs/format/filesystem-v1.md"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
# shellcheck disable=SC2086
python3 -m jeefs_codegen --specs $SPECS \
    --c-output "$TMP/jeefs_generated.h" \
    --py-output "$TMP/constants_generated.py" \
    --rs-output "$TMP/generated.rs" >/dev/null
diff -u ../include/jeefs_generated.h "$TMP/jeefs_generated.h"
diff -u ../python/jeefs/constants_generated.py "$TMP/constants_generated.py"
diff -u ../rust/jeefs-header/src/generated.rs "$TMP/generated.rs"
echo "generated files are in sync with docs/format/*.md"
