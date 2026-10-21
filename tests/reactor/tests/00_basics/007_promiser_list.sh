#!/bin/sh

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE_A="$WORKDIR/tmp/test_A"
TESTFILE_B="$WORKDIR/tmp/test_B"

EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE_A TESTFILE_B EVENTS_FILE

# create initial file
touch "$TESTFILE_A"
touch "$TESTFILE_B"

start_reactor "$POLICY_FILE"

wait_for_watchers 2

# checked in the order of the promises
wait_for_log "File '$TESTFILE_A' is present, watching for its deletion"
wait_for_log "File '$TESTFILE_B' is present, watching for its deletion"
rm "$TESTFILE_A"
rm "$TESTFILE_B"
wait_for_lines "$EVENTS_FILE" 2
check_events_unordered "$EVENTS_FILE" 1 2 \
    "$TESTFILE_A was deleted" "$TESTFILE_B was deleted"

# recreate the files and wait for the watchers to see them before deleting
# them again
touch "$TESTFILE_A"
touch "$TESTFILE_B"
wait_for_log "File '$TESTFILE_A' is present, watching for its deletion"
wait_for_log "File '$TESTFILE_B' is present, watching for its deletion"
rm "$TESTFILE_A"
rm "$TESTFILE_B"
wait_for_lines "$EVENTS_FILE" 4
check_events_unordered "$EVENTS_FILE" 3 4 \
    "$TESTFILE_A was deleted" "$TESTFILE_B was deleted"
