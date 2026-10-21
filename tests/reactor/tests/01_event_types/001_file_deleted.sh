#!/bin/sh

# check what is a deletion for a file_deleted event, which only looks at
# whether the path exists, without following it if it is a symlink:
#   A. a regular file: modifying it is not a deletion, removing it is
#   B. a file whose parent turns into a regular file (ENOTDIR) is deleted
#   C. a symlink: removing its target is not a deletion, removing it is
#   D. a directory: removing it is a deletion

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

FILE_A="$WORKDIR/tmp/file_a"
DIR_B="$WORKDIR/tmp/dir_b"
FILE_B="$DIR_B/file_b"
TARGET_C="$WORKDIR/tmp/target_c"
FILE_C="$WORKDIR/tmp/link_c"
FILE_D="$WORKDIR/tmp/dir_d"
EVENTS_FILE="$WORKDIR/tmp/events"
export FILE_A FILE_B FILE_C FILE_D EVENTS_FILE

# create initial files
touch "$FILE_A"
mkdir "$DIR_B"
touch "$FILE_B"
touch "$TARGET_C"
ln -s "$TARGET_C" "$FILE_C"
mkdir "$FILE_D"

start_reactor "$POLICY_FILE"

wait_for_watchers 4

# checked on registration, in the order of the promises
wait_for_log "File '$FILE_A' is present, watching for its deletion"
wait_for_log "File '$FILE_B' is present, watching for its deletion"
wait_for_log "File '$FILE_C' is present, watching for its deletion"
wait_for_log "File '$FILE_D' is present, watching for its deletion"

# not deletions
echo "modified" >> "$FILE_A"
touch "$FILE_A"
chmod 600 "$FILE_A"
rm "$TARGET_C"

# give the watchers a couple of checks to see them, in case they would fire.
# Proving that something doesn't happen needs a wait.
sleep 2
if [ -e "$EVENTS_FILE" ]
then
    echo "Fired on something that is not a deletion:" >&2
    cat "$EVENTS_FILE" >&2
    exit 1
fi

# deletions
rm "$FILE_A"

# make the parent of the file a regular file, so that its path has a regular
# file as directory (ENOTDIR). A check right between the two commands would
# see it missing (ENOENT) instead, which is also a deletion.
rm -r "$DIR_B"
touch "$DIR_B"

rm "$FILE_C"
rmdir "$FILE_D"

# each fired once
wait_for_lines "$EVENTS_FILE" 4
[ "$(cut -d' ' -f3- "$EVENTS_FILE" | sort)" = "$(printf 'A deleted\nB deleted\nC deleted\nD deleted')" ]
