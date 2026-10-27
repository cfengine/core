#!/bin/sh

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

# create initial file
touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 3

# recreate the file and wait for the watcher to see it before deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 6

# the bundles are run in the order of the list
check_event "$EVENTS_FILE" 1 "$TESTFILE first"
check_event "$EVENTS_FILE" 2 "$TESTFILE second"
check_event "$EVENTS_FILE" 3 "$TESTFILE third"
check_event "$EVENTS_FILE" 4 "$TESTFILE first"
check_event "$EVENTS_FILE" 5 "$TESTFILE second"
check_event "$EVENTS_FILE" 6 "$TESTFILE third"
