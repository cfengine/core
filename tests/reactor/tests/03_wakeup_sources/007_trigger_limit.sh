#!/bin/sh

# check that a watcher firing more than 10 times a minute is delayed, its
# further events coalesced into one run once the delay is over

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

touch "$TESTFILE"

start_reactor "$POLICY_FILE"
wait_for_watchers 1

# Each modification seen as its own event: wait for each run before the next
i=0
while [ $i -lt 10 ]; do
    i=$((i + 1))
    touch "$TESTFILE"
    wait_for_lines "$EVENTS_FILE" $i
done

# Beyond the limit, a burst of modifications only runs the bundles once more,
# after the delay
i=0
while [ $i -lt 5 ]; do
    i=$((i + 1))
    touch "$TESTFILE"
    sleep 1.1 2>/dev/null || sleep 2
done
wait_for_log "fired more than 10 times in 60 seconds, delaying its next run by 10 seconds"
wait_for_lines "$EVENTS_FILE" 11
sleep 3
wait_for_lines "$EVENTS_FILE" 11
