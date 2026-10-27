#!/bin/sh

# check that processes promises in a "then" bundle are evaluated again on
# every event, with the process table of the event: the process started
# after the first one is killed by the second one

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
PID_FILE="$WORKDIR/tmp/pid"
export TESTFILE EVENTS_FILE PID_FILE

# Start a process to be matched by the processes promise, detached from this
# shell so that it doesn't stay a zombie of it once killed
start_process() {
    ( sleep 978654 & echo $! > "$PID_FILE" )
}

wait_for_process_killed() {
    if ! timeout 60 sh -c 'while kill -0 "$1" 2>/dev/null; do sleep 0.1; done' _ "$(cat "$PID_FILE")"
    then
        echo "Process $(cat "$PID_FILE") not killed, reactor log:" >&2
        cat "$LOG" >&2
        return 1
    fi
}

start_process
touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1

# first event
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
# like in cf-agent, killing the process defines both classes
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 1 "processes kept"
check_event "$EVENTS_FILE" 2 "processes repaired"
wait_for_process_killed

# a new process, not in the process table of the first event
start_process

# second event: recreate the file and wait for the watcher to see it before
# deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 4
check_event "$EVENTS_FILE" 3 "processes kept"
check_event "$EVENTS_FILE" 4 "processes repaired"
wait_for_process_killed
