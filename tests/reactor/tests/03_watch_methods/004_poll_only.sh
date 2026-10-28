#!/bin/sh

# check that a watcher limited to polling uses it, and still fires

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

touch "$TESTFILE"

start_reactor "$POLICY_FILE"
wait_for_watchers 1
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
wait_until log_contains "Reactor watcher '$PROMISE_NAME' watches with 'poll'"

rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "$TESTFILE was deleted"
