#!/bin/sh

# when could set up watchers, but the polling thread is failing, return NOT KEPT

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTDIR="$WORKDIR/tmp/dir"
TESTFILE="$TESTDIR/test"
CHECK_FILE="$WORKDIR/tmp/check"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE CHECK_FILE EVENTS_FILE

# create initial files
mkdir "$TESTDIR"
touch "$TESTFILE"
touch "$CHECK_FILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 2

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
wait_for_log "File '$CHECK_FILE' is present, watching for its deletion"

# replace the directory of the watched file with a symlink to itself, so that
# checking the file fails (ELOOP, even as root) rather than seeing it deleted
rm -r "$TESTDIR"
ln -s "$TESTDIR" "$TESTDIR"
wait_for_log "Reactor watcher '${PROMISE_NAME}_failing' failed to check for its event"

# the outcome of the failing promise is only visible to the next event
rm "$CHECK_FILE"
wait_for_lines "$EVENTS_FILE" 1

# the "then" bundle of the failing promise didn't run, and it wasn't kept
check_event "$EVENTS_FILE" 1 "failing_failed"
