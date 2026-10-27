#!/bin/sh

# check that databases promises in a "then" bundle are evaluated on every
# event. Like in cf-agent, no outcome classes are defined when the server
# can't be connected to, so this looks for the error of each evaluation.

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"

export TESTFILE EVENTS_FILE

touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1

# first event
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_log "Could not connect to the sql_db server for 'reactor_test'"

# second event: recreate the file and wait for the watcher to see it before
# deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_log "Could not connect to the sql_db server for 'reactor_test'"
