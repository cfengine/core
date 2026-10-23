#!/bin/sh

# check what is a creation for a file_created event, which only looks at
# whether the path exists, without following it if it is a symlink:
#   A. a regular file: creating it is a creation
#   B. a directory: creating it is a creation
#   C. a symlink: creating it is a creation, even if its target doesn't exist
#   D. a file present from the start: modifying or deleting it is not a
#      creation

set -ex

. "$(dirname "$0")/../../lib.sh"

FILE_A="$WORKDIR/tmp/file_a"
FILE_B="$WORKDIR/tmp/dir_b"
FILE_C="$WORKDIR/tmp/link_c"
FILE_D="$WORKDIR/tmp/file_d"
EVENTS_FILE="$WORKDIR/tmp/events"
export FILE_A FILE_B FILE_C FILE_D EVENTS_FILE

# create initial files
touch "$FILE_D"

start_reactor "$1"

wait_for_watchers 4

# not creations
echo "modified" >> "$FILE_D"
rm "$FILE_D"

# give the watchers a couple of checks to see them, in case they would fire.
# Proving that something doesn't happen needs a wait.
sleep 2
if [ -e "$EVENTS_FILE" ]
then
    echo "Fired on something that is not a creation:" >&2
    cat "$EVENTS_FILE" >&2
    exit 1
fi

# creations
touch "$FILE_A"
mkdir "$FILE_B"
ln -s "$WORKDIR/tmp/no_such_file" "$FILE_C"

# each fired once
wait_for_lines "$EVENTS_FILE" 3
[ "$(cut -d' ' -f3- "$EVENTS_FILE" | sort)" = "$(printf 'A created\nB created\nC created')" ]
