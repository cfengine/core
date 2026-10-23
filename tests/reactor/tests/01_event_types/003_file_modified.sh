#!/bin/sh

# check what is a modification for a file_modified event, which looks at the
# modification time of the path, without following it if it is a symlink:
#   A. a regular file: changing its permissions is not a modification,
#      writing to it is
#   B. a regular file: touching it is a modification, even without writing
#   C. a symlink: writing to its target is not a modification
#   D. a directory: creating a file in it is a modification
#   E. a file created after the start: its creation is not a modification

set -ex

. "$(dirname "$0")/../../lib.sh"

FILE_A="$WORKDIR/tmp/file_a"
FILE_B="$WORKDIR/tmp/file_b"
TARGET_C="$WORKDIR/tmp/target_c"
FILE_C="$WORKDIR/tmp/link_c"
FILE_D="$WORKDIR/tmp/dir_d"
FILE_E="$WORKDIR/tmp/file_e"
EVENTS_FILE="$WORKDIR/tmp/events"
export FILE_A FILE_B FILE_C FILE_D FILE_E EVENTS_FILE

# create initial files
touch "$FILE_A"
touch "$FILE_B"
touch "$TARGET_C"
ln -s "$TARGET_C" "$FILE_C"
mkdir "$FILE_D"

start_reactor "$1"

wait_for_watchers 5

# not modifications
chmod 600 "$FILE_A"
echo "modified" >> "$TARGET_C"
touch "$FILE_E"

# give the watchers a couple of checks to see them, in case they would fire.
# Proving that something doesn't happen needs a wait.
sleep 2
if [ -e "$EVENTS_FILE" ]
then
    echo "Fired on something that is not a modification:" >&2
    cat "$EVENTS_FILE" >&2
    exit 1
fi

# modifications
echo "modified" >> "$FILE_A"
touch -m -d "2000-01-01 00:00:00" "$FILE_B"
touch "$FILE_D/file"

# each fired once
wait_for_lines "$EVENTS_FILE" 3
[ "$(cut -d' ' -f3- "$EVENTS_FILE" | sort)" = "$(printf 'A modified\nB modified\nD modified')" ]
