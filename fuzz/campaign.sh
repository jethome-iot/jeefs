#!/usr/bin/env bash
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
#
# Run a fuzzing campaign longer than the CI smoke, keep what it learns, and
# write down what was run — freeze criterion 3 asks for accumulated time
# without findings, which means the time has to be recorded somewhere.
#
#   ./fuzz/campaign.sh [minutes] [target ...]
#
# Defaults to 30 minutes per target across all four.
#
# fuzz/corpus/ holds curated seeds: a handful of named inputs that say which
# paths matter, reviewed like any other file. What a campaign discovers goes
# to fuzz/corpus-grown/ instead — hundreds of hash-named files that would
# bury a diff, useful on the machine that produced them and rebuildable
# anywhere. Crashes land in fuzz/crashes/ and make the script exit non-zero.
set -euo pipefail

cd "$(dirname "$0")/.."
MINUTES="${1:-30}"
shift || true
TARGETS=("$@")
if [ ${#TARGETS[@]} -eq 0 ]; then
    TARGETS=(fuzz_fs fuzz_header fuzz_devid fuzz_fs_diff)
fi

# libFuzzer needs a runtime the vendor clang does not always ship — on macOS
# Apple's clang has none, while Homebrew's LLVM does. Pick a compiler that
# can actually link a fuzzer instead of failing at the link step.
pick_clang() {
    if [ -n "${CC:-}" ]; then
        echo "$CC"
        return
    fi
    for candidate in /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang clang; do
        command -v "$candidate" >/dev/null 2>&1 || continue
        resource="$("$candidate" -print-resource-dir 2>/dev/null)" || continue
        if ls "$resource"/lib/*/libclang_rt.fuzzer* >/dev/null 2>&1; then
            echo "$candidate"
            return
        fi
    done
    echo ""
}

BUILD="build-fuzz"
if [ ! -x "$BUILD/fuzz/fuzz_fs" ]; then
    CLANG="$(pick_clang)"
    if [ -z "$CLANG" ]; then
        echo "No clang with a libFuzzer runtime found." >&2
        echo "On macOS: brew install llvm, then re-run (or set CC to a suitable clang)." >&2
        echo "The cross-language mutation vectors still run without it: ctest -R fs_mutation" >&2
        exit 2
    fi
    echo "Building fuzzers into $BUILD with $CLANG"
    CC="$CLANG" CXX="${CLANG}++" cmake -B "$BUILD" -DJEEFS_BUILD_FUZZERS=ON -DJEEFS_FUZZ_DRIVER=OFF \
        -DJEEFS_BUILD_TESTS=OFF -DJEEFS_BUILD_EXAMPLES=OFF -DJEEFS_INSTALL=OFF >/dev/null
    cmake --build "$BUILD" >/dev/null
fi

mkdir -p fuzz/crashes
LOG="fuzz/campaign.log"
STARTED="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
failed=0

seed_corpus_for() {
    case "$1" in
        fuzz_fs) echo "fuzz/corpus/fs" ;;
        fuzz_fs_diff) echo "fuzz/corpus/fs_diff" ;;
        fuzz_header) echo "fuzz/corpus/header" ;;
        fuzz_devid) echo "fuzz/corpus/devid" ;;
        *) echo "fuzz/corpus/$1" ;;
    esac
}

grown_corpus_for() {
    echo "fuzz/corpus-grown/$1"
}

seed_dir_for() {
    # Committed vectors are the starting point for the header and record
    # targets; the filesystem targets start from their own corpora.
    case "$1" in
        fuzz_header) echo "test-vectors/vectors" ;;
        fuzz_devid) echo "test-vectors/vectors" ;;
        *) echo "" ;;
    esac
}

for target in "${TARGETS[@]}"; do
    bin="$BUILD/fuzz/$target"
    if [ ! -x "$bin" ]; then
        echo "skip $target: not built (cargo missing for the differential target?)"
        continue
    fi

    seed_corpus="$(seed_corpus_for "$target")"
    grown="$(grown_corpus_for "$target")"
    mkdir -p "$seed_corpus" "$grown"
    work="$(mktemp -d)"
    cp "$seed_corpus"/*.bin "$work"/ 2>/dev/null || true
    cp "$grown"/* "$work"/ 2>/dev/null || true
    seeds="$(seed_dir_for "$target")"
    [ -n "$seeds" ] && cp "$seeds"/*.bin "$work"/ 2>/dev/null || true

    echo "=== $target: ${MINUTES}m"
    if ! "$bin" -max_total_time=$((MINUTES * 60)) -artifact_prefix=fuzz/crashes/ "$work"; then
        echo "FINDING: $target produced an artifact in fuzz/crashes/"
        failed=1
    fi

    # Keep what the run learned, minimised, so the next campaign starts ahead.
    "$bin" -merge=1 "$grown" "$work" >/dev/null 2>&1 || true
    rm -rf "$work"
    echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) $target ${MINUTES}m grown=$(ls "$grown" | wc -l | tr -d ' ')" >> "$LOG"
done

echo "campaign started $STARTED, log in $LOG"
exit $failed
