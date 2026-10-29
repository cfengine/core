#!/bin/sh

# check that a watcher whose 'watch_methods' changed over a policy reload still
# takes over the state of the watcher from before the reload, so that a
# deletion while reloading fires. With the methods part of the watcher's
# identity, it would start from scratch and only see the file missing.

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

METHODS_FILE="$WORKDIR/tmp/methods"
RELOAD_MARKER="$WORKDIR/tmp/reload_marker"
export METHODS_FILE RELOAD_MARKER

echo poll > "$METHODS_FILE"
touch "$TESTFILE"

start_reactor "$POLICY_FILE"
wait_for_watchers 1
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
wait_until log_contains "Reactor watcher '$PROMISE_NAME' watches with 'poll'"

# a different list, that still works everywhere
printf 'inotify\nrdcw\npoll\n' > "$METHODS_FILE"
rm "$RELOAD_MARKER"
kill -HUP "$reactor_pid"
wait_for_log "Rereading policy file"
if ! wait_until [ -f "$RELOAD_MARKER" ]; then
    echo "Timed out waiting for the policy to be reloaded, reactor log:" >&2
    cat "$LOG" >&2
    exit 1
fi
rm "$TESTFILE"

wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "$TESTFILE was deleted"
