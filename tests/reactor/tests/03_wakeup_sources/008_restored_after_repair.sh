#!/bin/sh

# check that writing the bad content back right after the 'then' bundle
# repaired it is repaired too: the check after the run must not compare it
# with the bad content seen before the run, and call it unchanged

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

echo good > "$TESTFILE"

start_reactor "$POLICY_FILE"
wait_for_watchers 1

for round in 1 2 3 4; do
    echo bad > "$TESTFILE.new"
    cp "$TESTFILE.new" "$TESTFILE"
    if ! wait_until grep -q -x good "$TESTFILE"; then
        echo "Round $round not repaired, reactor log:" >&2
        cat "$LOG" >&2
        exit 1
    fi
done
