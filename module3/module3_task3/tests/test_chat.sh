#!/bin/sh

set -eu

chat_binary=${1:-./build/posix_chat}
test_directory=$(mktemp -d)
queue_name="/test_chat_$$"
first_pid=
second_pid=

cleanup() {
    if [ -n "$first_pid" ]; then kill "$first_pid" 2>/dev/null || true; fi
    if [ -n "$second_pid" ]; then kill "$second_pid" 2>/dev/null || true; fi
    rm -rf "$test_directory"
}

trap cleanup EXIT INT TERM

mkfifo "$test_directory/first.input" "$test_directory/second.input"
exec 3<>"$test_directory/first.input"
exec 4<>"$test_directory/second.input"

"$chat_binary" "$queue_name" <"$test_directory/first.input" >"$test_directory/first.output" 2>&1 &
first_pid=$!
"$chat_binary" "$queue_name" <"$test_directory/second.input" >"$test_directory/second.output" 2>&1 &
second_pid=$!

printf 'from first\n' >&3
printf 'from second\n' >&4

attempt=0
while [ "$attempt" -lt 50 ]; do
    if grep -q 'Peer: from second' "$test_directory/first.output" && grep -q 'Peer: from first' "$test_directory/second.output"; then break; fi
    sleep 0.02
    attempt=$((attempt + 1))
done

grep -q 'Peer: from second' "$test_directory/first.output"
grep -q 'Peer: from first' "$test_directory/second.output"

printf '/exit\n' >&3

attempt=0
while [ "$attempt" -lt 50 ]; do
    if ! kill -0 "$first_pid" 2>/dev/null && ! kill -0 "$second_pid" 2>/dev/null; then break; fi
    sleep 0.02
    attempt=$((attempt + 1))
done

if kill -0 "$first_pid" 2>/dev/null || kill -0 "$second_pid" 2>/dev/null; then
    echo "Chat processes did not stop" >&2
    exit 1
fi

wait "$first_pid"
wait "$second_pid"
first_pid=
second_pid=

echo "Chat integration test passed"
