#!/bin/sh

# cf-reactor defaults ifelapsed to 0, but it can be overridden

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE_DEFAULT="$WORKDIR/tmp/events_default"
EVENTS_FILE_IFELAPSED="$WORKDIR/tmp/events_ifelapsed"
export TESTFILE EVENTS_FILE_DEFAULT EVENTS_FILE_IFELAPSED

# create initial file
touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1

wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE_DEFAULT" 1
wait_for_lines "$EVENTS_FILE_IFELAPSED" 1
# the reports are written without a timestamp
[ "$(cat "$EVENTS_FILE_DEFAULT")" = "$TESTFILE was deleted" ]
[ "$(cat "$EVENTS_FILE_IFELAPSED")" = "$TESTFILE was deleted" ]

# recreate the file and wait for the watcher to see it before deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"

# with the default ifelapsed of 0, the report is made again, but not the one
# with an ifelapsed of 60 minutes
wait_for_lines "$EVENTS_FILE_DEFAULT" 2
wait_for_lines "$EVENTS_FILE_IFELAPSED" 1
[ "$(sed -n 2p "$EVENTS_FILE_DEFAULT")" = "$TESTFILE was deleted" ]
