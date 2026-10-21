#!/bin/sh

# check that methods promises in a "then" bundle are evaluated again on every
# event, with the arguments of the event

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
VALUE_FILE="$WORKDIR/tmp/value"
export TESTFILE EVENTS_FILE VALUE_FILE

printf "first" > "$VALUE_FILE"
touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1

# first event
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 1 "methods first"
check_event "$EVENTS_FILE" 2 "methods kept"

printf "second" > "$VALUE_FILE"

# second event: recreate the file and wait for the watcher to see it before
# deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 4
check_event "$EVENTS_FILE" 3 "methods second"
check_event "$EVENTS_FILE" 4 "methods kept"
