#!/bin/sh

# Usage: run.sh [--unsafe]
#
# Must be run from the tests/reactor directory.
#
# Tests in an unsafe/ directory modify the system they are running on, so they
# are only run with --unsafe.
#
# The binaries are taken from $CF_BINDIR if it is set, otherwise from the
# CFEngine build tree this directory is part of if they are built, otherwise
# from /var/cfengine/bin.

fail=0

unsafe=0
if [ "$1" = "--unsafe" ]; then
    unsafe=1
fi

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

# Print the path of binary $1, see the usage above.
find_binary() {
    if [ -n "$CF_BINDIR" ]; then
        echo "$CF_BINDIR/$1"
    elif [ -x "$PWD/../../$1/$1" ]; then
        # in-tree build, may be a libtool wrapper script
        echo "$PWD/../../$1/$1"
    else
        echo "/var/cfengine/bin/$1"
    fi
}

CF_REACTOR=$(find_binary cf-reactor)
CF_PROMISES=$(find_binary cf-promises)
export CF_REACTOR

for b in "$CF_REACTOR" "$CF_PROMISES"; do
    if [ ! -x "$b" ]; then
        echo "'$b' not found, build CFEngine or set CF_BINDIR" >&2
        exit 2
    fi
done
echo "Using $CF_REACTOR and $CF_PROMISES"

for t in "$PWD"/tests/*/*.sh "$PWD"/tests/*/unsafe/*.sh; do
    # skip the pattern itself if there are no unsafe tests
    [ -f "$t" ] || continue
    case "$t" in
        */unsafe/*) [ "$unsafe" = 1 ] || continue ;;
    esac

    banner "$t"

    # create throwaway work dir
    WORKDIR=$(mktemp -d)
    mkdir -p "$WORKDIR/bin" "$WORKDIR/tmp"
    ln -s "$CF_PROMISES" "$WORKDIR/bin/cf-promises"

    # Run a copy of the policy, keeping its path relative to lib.cf, as
    # CFEngine refuses to read policy files writable by group or others, which
    # those of a checkout made with a umask of 002 are
    rel=${t#"$PWD"/}
    policy="$WORKDIR/policy/${rel%.sh}.cf"
    mkdir -p "$(dirname "$policy")"
    cp lib.cf "$WORKDIR/policy/"
    cp "${t%.sh}.cf" "$policy"
    chmod 600 "$WORKDIR/policy/lib.cf" "$policy"

    CFENGINE_TEST_OVERRIDE_WORKDIR="$WORKDIR"
    LOG="$WORKDIR/reactor.log"
    PROMISE_NAME=$(basename "$t" .sh)
    export WORKDIR CFENGINE_TEST_OVERRIDE_WORKDIR LOG PROMISE_NAME

    sh "$t" "$policy" || error "$t"

    rm -rf "$WORKDIR"
done

exit $fail
