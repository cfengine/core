#!/bin/sh

# Usage: run.sh [--unsafe]
#
# Runs all the reactor tests, see run_test.sh. They are also run by
# "make check".
#
# Tests in an unsafe/ directory modify the system they are running on, so they
# are only run with --unsafe (or UNSAFE_TESTS=1).
#
# The binaries are taken from $CF_BINDIR if it is set, otherwise from the
# CFEngine build tree this directory is part of if they are built, otherwise
# from /var/cfengine/bin.

fail=0

if [ "$1" = "--unsafe" ]; then
    UNSAFE_TESTS=1
fi
export UNSAFE_TESTS

banner() {
    name=" $(basename "$(dirname "$1")")/$(basename "$1") "
    width=72
    left=$(( (width - ${#name}) / 2 ))
    right=$(( width - ${#name} - left ))
    [ "$left" -lt 0 ] && left=0
    [ "$right" -lt 0 ] && right=0
    printf '%s%s%s\n' \
        "$(printf "%${left}s" '' | tr ' ' '=')" \
        "$name" \
        "$(printf "%${right}s" '' | tr ' ' '=')"
}

error() {
    echo "test '$1' FAILED" >&2
    fail=1
}

dir=$(cd "$(dirname "$0")" && pwd)

for t in "$dir"/tests/*/*.sh "$dir"/tests/*/unsafe/*.sh; do
    # skip the pattern itself if there are no unsafe tests
    [ -f "$t" ] || continue
    case "$t" in
        */unsafe/*) [ "$UNSAFE_TESTS" = 1 ] || continue ;;
    esac

    banner "$t"

    sh "$dir/run_test.sh" "$t"
    case $? in
        0|77) ;;
        *) error "$t" ;;
    esac
done

exit $fail
