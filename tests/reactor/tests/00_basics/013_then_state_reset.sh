#!/bin/sh

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE_A="$WORKDIR/tmp/events_A"
EVENTS_FILE_B="$WORKDIR/tmp/events_B"
MARKER_FILE="$WORKDIR/tmp/marker"
VALUE_FILE="$WORKDIR/tmp/value"
export TESTFILE EVENTS_FILE_A EVENTS_FILE_B MARKER_FILE VALUE_FILE

# create initial file
touch "$TESTFILE"
printf 'first' > "$VALUE_FILE"

start_reactor "$POLICY_FILE"

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE_A" 1
check_event "$EVENTS_FILE_A" 1 "$TESTFILE was deleted"
wait_for_lines "$EVENTS_FILE_B" 1
check_event "$EVENTS_FILE_B" 1 "first"

# recreate the file and wait for the watcher to see it before deleting it again
touch "$TESTFILE"
touch "$MARKER_FILE"
printf 'second' > "$VALUE_FILE"

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
# run_B, which runs after run_A, recorded the value read on this event, not
# the one of the previous event
wait_for_lines "$EVENTS_FILE_B" 2
check_event "$EVENTS_FILE_B" 2 "second"
# so run_A has run too, and didn't record the event since MARKER_FILE exists
wait_for_lines "$EVENTS_FILE_A" 1
