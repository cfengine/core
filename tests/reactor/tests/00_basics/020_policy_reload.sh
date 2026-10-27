#!/bin/sh

# check that a watcher keeps the state of its event (whether the file existed
# at its last check) over a policy reload, i.e. that what the first check after
# the reload sees is compared to the state from before the reload, so that an
# event is neither missed nor reported twice:
#   1. last check: exists,  reload, file deleted   -> next check: event
#   2. last check: missing, reload, still missing  -> next check: no event,
#      the deletion was already reported
#   3. last check: missing, reload, file created   -> next check: no event

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
RELOAD_MARKER="$WORKDIR/tmp/reload_marker"
export TESTFILE EVENTS_FILE RELOAD_MARKER

# Reload the policy and wait until its watchers are set aside, which they stay
# for 3 seconds, see the "slow" vars promise. The file can't be checked until
# then, so changing it now is only seen by the watcher of the reloaded policy.
reload_and_wait_for_paused_watchers() {
    # created when the policy was last evaluated
    rm "$RELOAD_MARKER"

    kill -HUP "$reactor_pid"
    wait_for_log "Rereading policy file"

    if ! timeout 60 sh -c 'until [ -f "$1" ]; do sleep 0.1; done' _ "$RELOAD_MARKER"
    then
        echo "Timed out waiting for the policy to be reloaded, reactor log:" >&2
        cat "$LOG" >&2
        exit 1
    fi
}

# create initial file
touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1
wait_for_log "File '$TESTFILE' is present, watching for its deletion"

# 1. last check: exists. Without the state from before the reload, the first
# check after it would only find the file missing, which is not an event.
reload_and_wait_for_paused_watchers
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "$TESTFILE was deleted"

# 2. last check: missing (the deletion above, already reported). Without the
# state from before the reload, a watcher assuming the file existed would
# report the deletion again.
rm "$RELOAD_MARKER"
kill -HUP "$reactor_pid"
wait_for_log "Rereading policy file"
# the watchers of the reloaded policy are registered and checked again
wait_for_log "cf-reactor event watcher has 1 watcher(s)"

# Proving that something doesn't happen needs a wait, give it a couple of
# checks
sleep 2
wait_for_lines "$EVENTS_FILE" 1

# 3. last check: missing. The file appearing is not an event, the watcher only
# starts watching for its deletion again.
reload_and_wait_for_paused_watchers
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"

# An event would have been queued by the check that found the file present,
# give it a couple of checks to be handled
sleep 2
wait_for_lines "$EVENTS_FILE" 1

rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 2 "$TESTFILE was deleted"
