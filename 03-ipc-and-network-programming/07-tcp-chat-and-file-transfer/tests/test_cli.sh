#!/bin/sh

set -eu

client=$1
server=$2

expect_success() {
    if ! "$@" >/dev/null 2>&1; then
        echo "Expected success: $*" >&2
        exit 1
    fi
}

expect_failure() {
    if "$@" >/dev/null 2>&1; then
        echo "Expected failure: $*" >&2
        exit 1
    fi
}

expect_failure "$client"
expect_failure "$client" ""
expect_failure "$client" Alice invalid
expect_failure "$client" Alice 127.0.0.1 0
expect_failure "$client" Alice 127.0.0.1 65536
expect_failure "$client" Alice 127.0.0.1 not-a-port

expect_failure "$server" invalid
expect_failure "$server" 127.0.0.1 0
expect_failure "$server" 127.0.0.1 65536
expect_failure "$server" 127.0.0.1 not-a-port

echo "CLI tests passed"
