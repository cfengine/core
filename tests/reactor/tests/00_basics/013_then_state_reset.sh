#!/bin/sh

set -ex

. "$(dirname "$0")/../../lib.sh"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE_A="$WORKDIR/tmp/events_A"
EVENTS_FILE_B="$WORKDIR/tmp/events_B"
MARKER_FILE="$WORKDIR/tmp/marker"
export TESTFILE EVENTS_FILE_A EVENTS_FILE_B MARKER_FILE

# create initial file
touch "$TESTFILE"

start_reactor "$1"

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE_A" 1
wait_for_lines "$EVENTS_FILE_B" 1

# recreate the file and wait for the watcher to see it before deleting it again
touch "$TESTFILE"
touch "$MARKER_FILE"

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
# record_event should not have written to EVENTS_FILE_A since MARKER_FILE exists
wait_for_lines "$EVENTS_FILE_A" 1
wait_for_lines "$EVENTS_FILE_B" 2

if [ "$(sed -n 1p "$EVENTS_FILE_B")" = "$(sed -n 2p "$EVENTS_FILE_B")" ]; then
    echo "Lines in '$EVENTS_FILE_B' are identical:" >&2
    cat "$EVENTS_FILE_B" >&2
    exit 1
fi