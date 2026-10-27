#!/bin/sh

# check that user_added and user_removed fire once when a user is added and
# removed. Adds and removes a user with sudo.

set -ex

. "$(dirname "$0")/../../../lib.sh"

POLICY_FILE="$1"

TEST_USER="cfreactortest"
EVENTS_FILE="$WORKDIR/tmp/events"
export TEST_USER EVENTS_FILE

# never touch an existing user
if id "$TEST_USER" >/dev/null 2>&1
then
    echo "User '$TEST_USER' already exists" >&2
    exit 1
fi

start_reactor "$POLICY_FILE"

# replaces the trap of start_reactor
cleanup() {
    stop_reactor
    sudo userdel "$TEST_USER" 2>/dev/null || true
}
trap cleanup EXIT

wait_for_watchers 2

# checked on registration
wait_for_log "User '$TEST_USER' is absent, watching for its addition"

sudo useradd -M "$TEST_USER"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "added"

wait_for_log "User '$TEST_USER' is present, watching for its removal"

sudo userdel "$TEST_USER"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 2 "removed"
