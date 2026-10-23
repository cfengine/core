#!/bin/sh

fail=0

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

CF_BINDIR=${CF_BINDIR:-/var/cfengine/bin}
export CF_BINDIR

for t in "$PWD"/tests/*/*.sh; do
    banner "$t"

    # create throwaway work dir
    WORKDIR=$(mktemp -d)
    mkdir -p "$WORKDIR/bin" "$WORKDIR/tmp"
    ln -s "$CF_BINDIR/cf-promises" "$WORKDIR/bin/"
    CFENGINE_TEST_OVERRIDE_WORKDIR="$WORKDIR"
    LOG="$WORKDIR/reactor.log"
    PROMISE_NAME=$(basename "$t" .sh)
    export WORKDIR CFENGINE_TEST_OVERRIDE_WORKDIR LOG PROMISE_NAME

    sh "$t" "${t%.sh}.cf" || error "$t"

    rm -rf "$WORKDIR"
done

exit $fail
