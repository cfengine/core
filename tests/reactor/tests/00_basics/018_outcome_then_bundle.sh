#!/bin/sh

# if set up and polls correctly, the outcome should be the outcome of the run bundle in "then"

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE_FAILING="$WORKDIR/tmp/test_failing"
TESTFILE_REPAIRING="$WORKDIR/tmp/test_repairing"
CREATED_FILE="$WORKDIR/tmp/created"
CHECK_FILE="$WORKDIR/tmp/check"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE_FAILING TESTFILE_REPAIRING CREATED_FILE CHECK_FILE EVENTS_FILE

# create initial files
touch "$TESTFILE_FAILING"
touch "$TESTFILE_REPAIRING"
touch "$CHECK_FILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 3

wait_for_log "File '$TESTFILE_FAILING' is present, watching for its deletion"
wait_for_log "File '$TESTFILE_REPAIRING' is present, watching for its deletion"
wait_for_log "File '$CHECK_FILE' is present, watching for its deletion"

# one event at a time, the events are handled in order
rm "$TESTFILE_FAILING"
wait_for_log "Reactor watcher '${PROMISE_NAME}_failing' fired"
rm "$TESTFILE_REPAIRING"
wait_for_log "Reactor watcher '${PROMISE_NAME}_repairing' fired"

# the outcomes of the previous events are only visible to the next event
rm "$CHECK_FILE"
wait_for_lines "$EVENTS_FILE" 2

[ -f "$CREATED_FILE" ]
[ "$(cut -d' ' -f3- "$EVENTS_FILE" | sort)" = "$(printf 'failing_failed\nrepairing_repaired')" ]
