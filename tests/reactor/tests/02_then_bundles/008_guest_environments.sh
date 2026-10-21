#!/bin/sh

# check that guest_environments promises in a "then" bundle are evaluated on
# every event. Without libvirt, they don't define the classes of their
# outcome, so this looks at the log instead.

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"

export TESTFILE

touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1

# The promise is evaluated, and logged, on every pass of the bundle: wait for
# the end of the bundle, so that the next event only sees the logs of its run
wait_for_evaluation() {
    wait_for_log "P: BEGIN guest_environments promise (test)"
    wait_for_log "A: Promise was KEPT"
    wait_for_log "B: END bundle run_guest_environments"
}

# first event
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_evaluation

# second event: recreate the file and wait for the watcher to see it before
# deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_evaluation
