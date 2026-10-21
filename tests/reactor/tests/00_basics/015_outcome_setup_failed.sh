#!/bin/sh

# events promise is NOT KEPT when couldn't set up watchers

set -ex

. "$(dirname "$0")/../../lib.sh"

TESTFILE="$WORKDIR/tmp/test"
CHECK_FILE="$WORKDIR/tmp/check"
EVENTS_FILE="$WORKDIR/tmp/events"
MISSING_BUNDLE="no_such_bundle"
export TESTFILE CHECK_FILE EVENTS_FILE MISSING_BUNDLE

# create initial files
touch "$TESTFILE"
touch "$CHECK_FILE"

start_reactor "$1"

wait_for_log "refers to unknown bundle '$MISSING_BUNDLE'"

# only the watcher of the check promise, which is only set up if the broken
# promise was not kept
wait_for_watchers 1

wait_for_log "File '$CHECK_FILE' is present, watching for its deletion"
rm "$CHECK_FILE"
wait_for_lines "$EVENTS_FILE" 1
