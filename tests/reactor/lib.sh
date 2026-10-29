# Helpers sourced by the tests. Expects CF_REACTOR and LOG to be exported by
# run_test.sh.
#
# Only POSIX tools are used, as the tests are run on all the platforms (e.g.
# no timeout(1), grep -m or date +%s).

# Seconds to wait for something before giving up
WAIT_TIMEOUT=60

# number of log lines already consumed by wait_for_log
log_offset=0

# Run command $@ until it succeeds, for at most WAIT_TIMEOUT seconds, polling
# every 0.1 second, or every second where sleep only takes whole seconds.
# Fails if it never succeeded. Runs in a subshell without tracing, so that
# set -x doesn't log every poll.
wait_until() {
    (
        set +x
        if sleep 0.1 2>/dev/null; then
            interval=0.1 polls=$((WAIT_TIMEOUT * 10))
        else
            interval=1 polls=$WAIT_TIMEOUT
        fi
        until "$@"; do
            [ "$polls" -gt 0 ] || exit 1
            polls=$((polls - 1))
            sleep "$interval"
        done
    )
}

# Print the current time in seconds since the Epoch. date +%s is not POSIX;
# without it, srand() seeds with the time of day and returns the last seed.
now() {
    t=$(date +%s 2>/dev/null)
    case "$t" in
        ''|*[!0-9]*) awk 'BEGIN { srand(); print srand() }' ;;
        *) echo "$t" ;;
    esac
}

# Print the first line of the reactor log after line $1 that contains $2,
# prefixed with its number counted from line $1. Fails if there is none.
log_match() {
    tail -n "+$(($1 + 1))" "$LOG" 2>/dev/null | grep -n -F -- "$2" | head -n 1 | grep .
}

# Fail unless the reactor log contains $1.
log_contains() {
    grep -q -F -- "$1" "$LOG" 2>/dev/null
}

# Print the number of lines of file $1, 0 if it doesn't exist.
count_lines() {
    if [ -f "$1" ]; then
        # without the padding of some wc implementations
        echo $(( $(wc -l < "$1") ))
    else
        echo 0
    fi
}

# Fail unless file $1 has at least $2 lines.
has_lines() {
    [ "$(count_lines "$1")" -ge "$2" ]
}

# Wait until the reactor log contains $1 in a line after the last match, then
# skip past that line, so that the next wait only sees newer output.
wait_for_log() {
    if ! wait_until log_match "$log_offset" "$1" >/dev/null
    then
        echo "Timed out waiting for '$1' in reactor log:" >&2
        cat "$LOG" >&2
        return 1
    fi
    match=$(log_match "$log_offset" "$1")
    log_offset=$((log_offset + ${match%%:*}))
}

# Wait until cf-reactor has registered the watchers of the policy, $1 of them.
# The watcher thread may log before this line, so search the whole log and
# leave the offset of wait_for_log as is.
wait_for_watchers() {
    if ! wait_until log_contains "verbose: cf-reactor event watcher has $1 watcher(s)"
    then
        echo "Timed out waiting for $1 watcher(s) in reactor log:" >&2
        cat "$LOG" >&2
        return 1
    fi
}

# Wait until file $1 has at least $2 lines, then fail if it has more than $2.
wait_for_lines() {
    if ! wait_until has_lines "$1" "$2"
    then
        echo "Timed out waiting for $2 line(s) in '$1', reactor log:" >&2
        cat "$LOG" >&2
        return 1
    fi

    lines=$(count_lines "$1")
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
