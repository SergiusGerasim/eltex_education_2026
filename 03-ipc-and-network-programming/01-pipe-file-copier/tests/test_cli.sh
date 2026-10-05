#!/bin/sh

set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PROGRAM="$PROJECT_DIR/build/copy_file"
TEST_DIR=$(mktemp -d)

trap 'rm -rf "$TEST_DIR"' EXIT HUP INT TERM

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

if "$PROGRAM" >"$TEST_DIR/no_args.out" 2>"$TEST_DIR/no_args.err"; then
    fail "launch without arguments succeeded"
fi

if "$PROGRAM" -p "$TEST_DIR/incomplete_fifo" >"$TEST_DIR/incomplete_fifo.out" 2>"$TEST_DIR/incomplete_fifo.err"; then
    fail "launch with -p but without files succeeded"
fi

printf 'first test file\nsecond line\n' >"$TEST_DIR/first.txt"
: >"$TEST_DIR/empty.txt"
dd if=/dev/urandom of="$TEST_DIR/binary.bin" bs=1024 count=12 status=none

"$PROGRAM" "$TEST_DIR/first.txt" "$TEST_DIR/empty.txt" "$TEST_DIR/binary.bin" >"$TEST_DIR/unnamed.out"
cmp "$TEST_DIR/first.txt" "$TEST_DIR/first.txt.copy"
cmp "$TEST_DIR/empty.txt" "$TEST_DIR/empty.txt.copy"
cmp "$TEST_DIR/binary.bin" "$TEST_DIR/binary.bin.copy"

if "$PROGRAM" "$TEST_DIR/first.txt" "$TEST_DIR/missing.txt" "$TEST_DIR/empty.txt" >"$TEST_DIR/missing.out" 2>"$TEST_DIR/missing.err"; then
    fail "launch with a missing file returned success"
fi

cmp "$TEST_DIR/first.txt" "$TEST_DIR/first.txt.copy"
cmp "$TEST_DIR/empty.txt" "$TEST_DIR/empty.txt.copy"
grep -q "$TEST_DIR/missing.txt" "$TEST_DIR/missing.err" || fail "missing-file diagnostic was not written to stderr"

FIFO_BASE="$TEST_DIR/channel"
"$PROGRAM" -p "$FIFO_BASE" "$TEST_DIR/first.txt" "$TEST_DIR/binary.bin" >"$TEST_DIR/fifo.out"
cmp "$TEST_DIR/first.txt" "$TEST_DIR/first.txt.copy"
cmp "$TEST_DIR/binary.bin" "$TEST_DIR/binary.bin.copy"

if [ -e "$FIFO_BASE.data" ] || [ -e "$FIFO_BASE.ready" ]; then
    fail "FIFO files were not removed"
fi

"$PROGRAM" -p "$FIFO_BASE" "$TEST_DIR/empty.txt" >"$TEST_DIR/fifo_repeat.out"
cmp "$TEST_DIR/empty.txt" "$TEST_DIR/empty.txt.copy"

if [ -e "$FIFO_BASE.data" ] || [ -e "$FIFO_BASE.ready" ]; then
    fail "FIFO files were not removed after repeated launch"
fi

printf 'All tests passed\n'
