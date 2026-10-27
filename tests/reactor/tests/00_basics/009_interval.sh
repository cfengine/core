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

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "$TESTFILE was deleted"
t1=$(date +%s)

# recreate the file and wait for the watcher to see it before deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 2 "$TESTFILE was deleted"
t2=$(date +%s)

# checking that the interval is at minimum 10 seconds
delta=$((t2 - t1))
if [ "$delta" -lt 10 ]; then
  echo "FAIL: expected >= 10s, got ${delta}s" >&2
  exit 1
fi
