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

wait_for_watchers 1

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1

# the meta variables also expanded in the arguments of the "then" bundle
check_event "$EVENTS_FILE" 1 "$TESTFILE was deleted"
