#!/bin/sh

# check that stepping the wall clock back while cf-reactor is running doesn't
# postpone its checks by the size of the step, which is longer than the
# harness waits for an event. Steps the system clock with sudo.

set -ex

. "$(dirname "$0")/../../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

STEP_SECS=300

touch "$TESTFILE"

start_reactor "$POLICY_FILE"

restore_clock() {
    sudo date -s "@$(($(date +%s) + STEP_SECS))" >/dev/null
    stop_reactor
}
trap restore_clock EXIT

wait_for_log "File '$TESTFILE' is present, watching for its deletion"

# step back while the watcher thread is between two checks
sudo date -s "@$(($(date +%s) - STEP_SECS))" >/dev/null

rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "$TESTFILE was deleted"
