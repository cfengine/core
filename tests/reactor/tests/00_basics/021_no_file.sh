#!/bin/sh

# check that a watcher set up while its file doesn't exist doesn't fire, and
# fires once the file is created and then deleted

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

# no initial file

start_reactor "$POLICY_FILE"

# the watcher is set up, its check on registration found the file missing
wait_for_watchers 1

touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"

# the missing file at the first checks was not an event: those checks were
# made at least a second before the one which found the file present
if grep -F "Reactor watcher '$PROMISE_NAME' fired" "$LOG" || [ -e "$EVENTS_FILE" ]
then
    echo "Watcher fired while the file didn't exist yet, reactor log:" >&2
    cat "$LOG" >&2
    exit 1
fi

rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "$TESTFILE was deleted"
