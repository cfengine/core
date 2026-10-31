#!/bin/sh

# check that a 'then' bundle repairing the watched file runs once more, for
# its own change, and that this converges: the second run changes nothing, so
# there is no third. Changes made while the bundles run are never lost.

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

echo good > "$TESTFILE"

start_reactor "$POLICY_FILE"
wait_for_watchers 1

echo bad > "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1
wait_until grep -q -x good "$TESTFILE"

# the run for its own repair
wait_for_lines "$EVENTS_FILE" 2

# and no more, give it a few checks
sleep 3
wait_for_lines "$EVENTS_FILE" 2

# and the watcher still fires on a later change
echo bad again > "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 3
