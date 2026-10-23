#!/bin/sh

# check what is a content modification for a file_content_modified event,
# which looks at the hash of the content of regular files, without following
# symlinks:
#   A. a regular file: touching it or replacing it with a file of the same
#      content is not a content modification, appending to it is
#   B. an empty regular file: writing to it is a content modification
#   C. a symlink: writing to its target is not a content modification
#   D. a directory: creating a file in it is not a content modification

set -ex

. "$(dirname "$0")/../../lib.sh"

FILE_A="$WORKDIR/tmp/file_a"
FILE_B="$WORKDIR/tmp/file_b"
TARGET_C="$WORKDIR/tmp/target_c"
FILE_C="$WORKDIR/tmp/link_c"
FILE_D="$WORKDIR/tmp/dir_d"
EVENTS_FILE="$WORKDIR/tmp/events"
export FILE_A FILE_B FILE_C FILE_D EVENTS_FILE

# create initial files
echo "content" > "$FILE_A"
touch "$FILE_B"
touch "$TARGET_C"
ln -s "$TARGET_C" "$FILE_C"
mkdir "$FILE_D"

start_reactor "$1"

wait_for_watchers 4

# not content modifications. The same content is moved over A, so that no
# check sees it truncated.
touch "$FILE_A"
echo "content" > "$WORKDIR/tmp/same_content"
mv "$WORKDIR/tmp/same_content" "$FILE_A"
echo "modified" >> "$TARGET_C"
touch "$FILE_D/file"

# give the watchers a couple of checks to see them, in case they would fire.
# Proving that something doesn't happen needs a wait.
sleep 2
if [ -e "$EVENTS_FILE" ]
then
    echo "Fired on something that is not a content modification:" >&2
    cat "$EVENTS_FILE" >&2
    exit 1
fi

# content modifications
echo "modified" >> "$FILE_A"
echo "modified" >> "$FILE_B"

# each fired once
wait_for_lines "$EVENTS_FILE" 2
[ "$(cut -d' ' -f3- "$EVENTS_FILE" | sort)" = "$(printf 'A content modified\nB content modified')" ]
