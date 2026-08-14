#!/bin/sh
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
# The pure header API must not drag I/O headers: preprocess
# jeefs_header.h and fail if anything POSIX/eepromops shows up.
set -e
CC="$1"
INCDIR="$2"
OUT=$("$CC" -E -I"$INCDIR" -x c "$INCDIR/jeefs_header.h")
for bad in eepromops fcntl.h "sys/stat.h" unistd.h; do
    if printf '%s' "$OUT" | grep -q "$bad"; then
        echo "FAIL: jeefs_header.h include chain pulls in '$bad'"
        exit 1
    fi
done
echo "jeefs_header.h is I/O-free"
