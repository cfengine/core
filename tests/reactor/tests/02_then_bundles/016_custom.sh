#!/bin/sh

# check that custom promises in a "then" bundle are evaluated again on every
# event, with a promise module started for the event and terminated after it

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
OUTPUT_FILE="$WORKDIR/tmp/output"
MODULE_LOG="$WORKDIR/tmp/module_log"
export TESTFILE EVENTS_FILE OUTPUT_FILE MODULE_LOG

# A promise module appending the message attribute to the promiser file, and
# its start and termination to $MODULE_LOG
mkdir -p "$WORKDIR/modules/promises"
cat > "$WORKDIR/modules/promises/reactor_test.sh" <<'MODULE'
echo "started" >> "$MODULE_LOG"

# skip the header of the agent, up to the empty line
while read -r line && [ -n "$line" ]; do :; done
echo "reactor_test 0.0.1 v1 line_based"
echo ""

while true; do
    operation=""
    promiser=""
    message=""
    while read -r line && [ -n "$line" ]; do
        case "$line" in
            operation=*) operation="${line#*=}" ;;
            promiser=*) promiser="${line#*=}" ;;
            attribute_message=*) message="${line#*=}" ;;
        esac
    done

    case "$operation" in
        validate_promise)
            result="valid" ;;
        evaluate_promise)
            if echo "$message" >> "$promiser"; then
                result="repaired"
            else
                result="not_kept"
            fi ;;
        terminate)
            echo "terminated" >> "$MODULE_LOG"
            printf 'operation=terminate\nresult=success\n\n'
            exit 0 ;;
        *)
            result="error" ;;
    esac
    printf 'operation=%s\nresult=%s\n\n' "$operation" "$result"
done
MODULE

touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1

# first event
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "custom repaired"
[ "$(cat "$OUTPUT_FILE")" = "ran" ]
[ "$(cat "$MODULE_LOG")" = "$(printf 'started\nterminated')" ]

# second event: recreate the file and wait for the watcher to see it before
# deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 2 "custom repaired"
[ "$(cat "$OUTPUT_FILE")" = "$(printf 'ran\nran')" ]
[ "$(cat "$MODULE_LOG")" = "$(printf 'started\nterminated\nstarted\nterminated')" ]
