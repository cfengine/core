#!/bin/sh

# check that a bundle in another namespace can be run in "then", by its name
# (then => "ns:my_bundle") and called with arguments (then => ns:my_bundle(...))

set -ex

. "$(dirname "$0")/../../lib.sh"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

# create initial file
touch "$TESTFILE"

start_reactor "$1"

wait_for_watchers 1

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 2

# both bundles ran, in the order of the "then" list
sed -n 1p "$EVENTS_FILE" | grep -F "test_ns:record_without_args ran"
sed -n 2p "$EVENTS_FILE" | grep -F "test_ns:record_with_args ran"
