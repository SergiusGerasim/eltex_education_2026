#!/bin/sh

set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$project_dir/build/subnet_membership"

test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT

cd "$test_dir"

fail()
{
    echo "FAIL: $1" >&2
    exit 1
}

expect_failure()
{
    description=$1
    shift

    if "$@" >/dev/null 2>&1; then
        fail "$description: program unexpectedly succeeded"
    fi
}

echo "Checking valid invocation..."

output=$("$binary" 192.168.1.1 255.255.255.0 100)

echo "$output" | grep -q "Processed packets: 100" ||
    fail "missing processed packet count"

echo "$output" | grep -q "Local subnet:" ||
    fail "missing local subnet statistics"

echo "$output" | grep -q "External networks:" ||
    fail "missing external network statistics"

echo "$output" |
    grep -q "Packet addresses written to: generated_packets.txt" ||
    fail "missing output file message"

[ -f generated_packets.txt ] ||
    fail "generated_packets.txt was not created"

line_count=$(wc -l < generated_packets.txt)

[ "$line_count" -eq 100 ] ||
    fail "expected 100 packet lines, got $line_count"

formatted_line_count=$(
    grep -Ec \
        '^[0-9]+: ([0-9]{1,3}\.){3}[0-9]{1,3} - (local subnet|external network)$' \
        generated_packets.txt
)

[ "$formatted_line_count" -eq 100 ] ||
    fail "some packet lines have an invalid format"

echo "Checking random address distribution..."

probability_packet_count=20000
"$binary" 192.168.1.1 128.0.0.0 "$probability_packet_count" \
    >/dev/null

local_count=$(
    awk '/ - local subnet$/ { count++ } END { print count + 0 }' \
        generated_packets.txt
)

minimum_local_count=$((probability_packet_count * 45 / 100))
maximum_local_count=$((probability_packet_count * 55 / 100))

if [ "$local_count" -lt "$minimum_local_count" ] ||
   [ "$local_count" -gt "$maximum_local_count" ]; then
    fail \
        "expected about 50% local packets for /1, got $local_count of $probability_packet_count"
fi

echo "Checking invalid invocations..."

expect_failure \
    "missing arguments" \
    "$binary"

expect_failure \
    "invalid gateway" \
    "$binary" invalid 255.255.255.0 100

expect_failure \
    "invalid mask syntax" \
    "$binary" 192.168.1.1 invalid 100

expect_failure \
    "noncontiguous mask" \
    "$binary" 192.168.1.1 255.0.255.0 100

expect_failure \
    "zero packet count" \
    "$binary" 192.168.1.1 255.255.255.0 0

expect_failure \
    "negative packet count" \
    "$binary" 192.168.1.1 255.255.255.0 -10

expect_failure \
    "nonnumeric packet count" \
    "$binary" 192.168.1.1 255.255.255.0 abc

expect_failure \
    "extra argument" \
    "$binary" 192.168.1.1 255.255.255.0 100 extra

echo "All CLI integration tests passed"
