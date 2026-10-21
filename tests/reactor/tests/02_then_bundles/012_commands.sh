#!/bin/sh

# check that commands promises in a "then" bundle are run again on every
# event, and that the variables of a module command are the ones of its
# output of the event

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
OUTPUT_FILE="$WORKDIR/tmp/output"
MODULE_FILE="$WORKDIR/tmp/module"
export TESTFILE EVENTS_FILE OUTPUT_FILE MODULE_FILE

printf "^context=reactor_test\n=value=first\n" > "$MODULE_FILE"
touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1

# first event
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 1 "commands module value first"
check_event "$EVENTS_FILE" 2 "commands repaired"
[ "$(cat "$OUTPUT_FILE")" = "ran" ]

printf "^context=reactor_test\n=value=second\n" > "$MODULE_FILE"

# second event: recreate the file and wait for the watcher to see it before
# deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 4
check_event "$EVENTS_FILE" 3 "commands module value second"
check_event "$EVENTS_FILE" 4 "commands repaired"
[ "$(cat "$OUTPUT_FILE")" = "$(printf 'ran\nran')" ]
