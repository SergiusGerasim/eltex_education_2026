#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <chat_program>" >&2
    exit 1
fi

program=$1
test_port=$((50000 + $$ % 10000))
test_directory=$(mktemp -d)
alice_pid=
bob_pid=
charlie_pid=

cleanup() {
    if [ -n "$charlie_pid" ]; then kill "$charlie_pid" 2>/dev/null || true; fi
    if [ -n "$bob_pid" ]; then kill "$bob_pid" 2>/dev/null || true; fi
    if [ -n "$alice_pid" ]; then kill "$alice_pid" 2>/dev/null || true; fi
    rm -rf "$test_directory"
}

trap cleanup EXIT INT TERM

mkfifo "$test_directory/alice.in" "$test_directory/bob.in"
exec 3<>"$test_directory/alice.in"
exec 4<>"$test_directory/bob.in"

"$program" Alice 127.255.255.255 "$test_port" <"$test_directory/alice.in" >"$test_directory/alice.out" 2>&1 &
alice_pid=$!
sleep 0.2

"$program" Bob 127.255.255.255 "$test_port" <"$test_directory/bob.in" >"$test_directory/bob.out" 2>&1 &
bob_pid=$!
sleep 0.2

long_message=$(awk 'BEGIN { for (i = 0; i < 1024; ++i) printf "x" }')
printf '%s\n' "$long_message" >&4
sleep 0.2
printf '%s\n' "Hello from Bob" >&4
sleep 0.2
printf '%s\n' "/exit" >&4
wait "$bob_pid"
bob_pid=
sleep 0.2

"$program" Charlie 127.255.255.255 "$test_port" </dev/null >"$test_directory/charlie.out" 2>&1 &
charlie_pid=$!
wait "$charlie_pid"
charlie_pid=
sleep 0.2

printf '%s\n' "/exit" >&3
wait "$alice_pid"
alice_pid=

grep -Fqx "Bob joined the chat" "$test_directory/alice.out"
grep -Fqx "Bob: Hello from Bob" "$test_directory/alice.out"
grep -Fqx "Bob left the chat" "$test_directory/alice.out"
grep -Fqx "Charlie joined the chat" "$test_directory/alice.out"
grep -Fqx "Charlie left the chat" "$test_directory/alice.out"
grep -Fq "Message is too long" "$test_directory/bob.out"

if grep -Fq "Bob: $long_message" "$test_directory/alice.out"; then
    echo "Oversized message was unexpectedly delivered" >&2
    exit 1
fi

echo "Chat integration test passed"
