#!/bin/sh

set -ex

. "$(dirname "$0")/../../lib.sh"

TESTFILE_A="$WORKDIR/tmp/test_A"
TESTFILE_B="$WORKDIR/tmp/test_B"

EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE_A TESTFILE_B EVENTS_FILE

# create initial file
touch "$TESTFILE_A"
touch "$TESTFILE_B"

start_reactor "$1"

wait_for_watchers 2

wait_for_log "File '$TESTFILE_A' is present, watching for its deletion"
rm "$TESTFILE_A"
rm "$TESTFILE_B"
wait_for_lines "$EVENTS_FILE" 2

# recreate the file and wait for the watcher to see it before deleting it again
touch "$TESTFILE_A"
touch "$TESTFILE_B"
wait_for_log "File '$TESTFILE_A' is present, watching for its deletion"
rm "$TESTFILE_A"
rm "$TESTFILE_B"
wait_for_lines "$EVENTS_FILE" 2
