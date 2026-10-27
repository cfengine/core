# Helpers sourced by the tests. Expects CF_REACTOR and LOG to be exported by
# run.sh.

# number of log lines already consumed by wait_for_log
log_offset=0

# Wait until the reactor log contains $1 in a line after the last match, then
# skip past that line, so that the next wait only sees newer output.
wait_for_log() {
    if ! match=$(timeout 60 sh -c '
        until tail -n "+$(($2 + 1))" "$1" | grep -n -F -m 1 -- "$3"; do
            sleep 0.1
        done' _ "$LOG" "$log_offset" "$1")
    then
        echo "Timed out waiting for '$1' in reactor log:" >&2
        cat "$LOG" >&2
        return 1
    fi
    log_offset=$((log_offset + ${match%%:*}))
}

# Wait until cf-reactor has registered the watchers of the policy, $1 of them.
# The watcher thread may log before this line, so search the whole log and
# leave the offset of wait_for_log as is.
wait_for_watchers() {
    if ! timeout 60 sh -c '
        until grep -q -F -- "$2" "$1"; do
            sleep 0.1
        done' _ "$LOG" "verbose: cf-reactor event watcher has $1 watcher(s)"
    then
        echo "Timed out waiting for $1 watcher(s) in reactor log:" >&2
        cat "$LOG" >&2
        return 1
    fi
}

# Wait until file $1 has at least $2 lines, then fail if it has more than $2.
wait_for_lines() {
    if ! timeout 60 sh -c '
        until [ "$(cat "$1" 2>/dev/null | wc -l)" -ge "$2" ]; do
            sleep 0.1
        done' _ "$1" "$2"
    then
        echo "Timed out waiting for $2 line(s) in '$1', reactor log:" >&2
        cat "$LOG" >&2
        return 1
    fi

    lines=$(wc -l < "$1")
    if [ "$lines" -ne "$2" ]
    then
        echo "Expected $2 line(s) in '$1', got $lines" >&2
        return 1
    fi
}

# Fail unless line $2 of file $1, without the timestamp written by
# record_event, is $3.
check_event() {
    line=$(sed -n "$2p" "$1" | cut -d' ' -f3-)
    if [ "$line" != "$3" ]
    then
        echo "Expected line $2 of '$1' to be '$3', got '$line':" >&2
        cat "$1" >&2
        return 1
    fi
}

# Fail unless lines $2 to $3 of file $1, without the timestamps written by
# record_event, are the remaining arguments in any order, for events whose
# order is not defined.
check_events_unordered() {
    file=$1 first=$2 last=$3
    shift 3
    actual=$(sed -n "${first},${last}p" "$file" | cut -d' ' -f3- | sort)
    expected=$(printf '%s\n' "$@" | sort)
    if [ "$actual" != "$expected" ]
    then
        echo "Expected lines $first to $last of '$file' to be, in any order:" >&2
        printf '%s\n' "$@" >&2
        echo "got:" >&2
        cat "$file" >&2
        return 1
    fi
}

# Start cf-reactor in the background with policy file $1, logging to $LOG.
# It is stopped when the test exits.
start_reactor() {
    "$CF_REACTOR" --no-fork --verbose -f "$1" >>"$LOG" 2>&1 &
    reactor_pid=$!
    trap stop_reactor EXIT
}

stop_reactor() {
    echo "Exiting cf-reactor..."
    kill -TERM "$reactor_pid" 2>/dev/null || true
    wait "$reactor_pid" 2>/dev/null || true
}
