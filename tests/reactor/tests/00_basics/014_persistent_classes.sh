#!/bin/sh

# check that persistent class stay alive between "then" bundle runs

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
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "$TESTFILE was deleted"

# recreate the file and wait for the watcher to see it before deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"

# wait for the "then" bundle to have run, it doesn't record the event as the
# persistent class has_run is still defined
wait_for_log "B: END bundle run_if"
wait_for_lines "$EVENTS_FILE" 1

touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"

# sleep for the duration of the persistent class
sleep 60
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 2 "$TESTFILE was deleted"
