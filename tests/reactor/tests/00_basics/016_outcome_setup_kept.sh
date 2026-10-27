#!/bin/sh

# when could set up watchers, promise is kept

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
CHECK_FILE="$WORKDIR/tmp/check"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE CHECK_FILE EVENTS_FILE

# create initial files
touch "$TESTFILE"
touch "$CHECK_FILE"

start_reactor "$POLICY_FILE"

# the watcher of the ok promise, and the one of the check promise, which is
# only set up if the ok promise was kept
wait_for_watchers 2

wait_for_log "File '$CHECK_FILE' is present, watching for its deletion"
rm "$CHECK_FILE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "$CHECK_FILE was deleted"
