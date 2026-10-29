#!/bin/sh

# check that a watcher whose methods are all unavailable (Windows ones, here)
# and that doesn't accept polling isn't registered

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
export TESTFILE EVENTS_FILE

case "$(uname -s)" in
    *MINGW*|*MSYS*|*CYGWIN*) exit 0 ;;
esac

touch "$TESTFILE"

start_reactor "$POLICY_FILE"
wait_for_log "Reactor events promise '$PROMISE_NAME': none of its watch methods is available on this host"
wait_for_watchers 0
