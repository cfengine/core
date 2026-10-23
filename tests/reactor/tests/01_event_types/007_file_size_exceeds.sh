#!/bin/sh

# check what is a size exceeding the threshold (10 bytes) for a
# file_size_exceeds event, which fires when the size of the path grows above
# the threshold:
#   A. a regular file: growing up to the threshold is not exceeding it,
#      growing above it is, growing further above it is not, shrinking below
#      it and growing above it again is
#   B. a file created after the start: being created above the threshold is
#      exceeding it
#   C. a file above the threshold from the start: growing is not exceeding it

set -ex

. "$(dirname "$0")/../../lib.sh"

FILE_A="$WORKDIR/tmp/file_a"
FILE_B="$WORKDIR/tmp/file_b"
FILE_C="$WORKDIR/tmp/file_c"
EVENTS_FILE="$WORKDIR/tmp/events"
export FILE_A FILE_B FILE_C EVENTS_FILE

# create initial files
touch "$FILE_A"
printf '0123456789abcdef' > "$FILE_C"

start_reactor "$1"

wait_for_watchers 3

# not exceeding the threshold
printf '0123456789' >> "$FILE_A"
printf 'more' >> "$FILE_C"

# give the watchers a couple of checks to see them, in case they would fire.
# Proving that something doesn't happen needs a wait.
sleep 2
if [ -e "$EVENTS_FILE" ]
then
    echo "Fired on something that is not exceeding the threshold:" >&2
    cat "$EVENTS_FILE" >&2
    exit 1
fi

# exceeding the threshold. B is written next to its path and moved there, so
# that it is created above the threshold.
printf 'a' >> "$FILE_A"
printf '0123456789abcdef' > "$WORKDIR/tmp/new_b"
mv "$WORKDIR/tmp/new_b" "$FILE_B"

wait_for_lines "$EVENTS_FILE" 2
[ "$(cut -d' ' -f3- "$EVENTS_FILE" | sort)" = "$(printf 'A exceeds\nB exceeds')" ]

# growing further above the threshold is not exceeding it again
printf 'more' >> "$FILE_A"
sleep 2
wait_for_lines "$EVENTS_FILE" 2

# shrinking below the threshold, then growing above it again, once a check
# has seen it below
: > "$FILE_A"
sleep 2
printf '0123456789abcdef' >> "$FILE_A"

wait_for_lines "$EVENTS_FILE" 3
check_event "$EVENTS_FILE" 3 "A exceeds"
