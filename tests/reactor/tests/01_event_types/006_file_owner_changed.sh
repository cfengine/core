#!/bin/sh

# check what is an owner change for a file_owner_changed event, which looks
# at the owning user and group of the path, without following it if it is a
# symlink:
#   A. a regular file: writing to it or changing its permissions is not an
#      owner change, changing its group is
#   B. a directory: changing its group is an owner change
#   C. a symlink: changing the group of its target is not an owner change
#
# Changing the owning user needs root, so only the group is changed, to
# another group of the user running the test.

set -ex

. "$(dirname "$0")/../../lib.sh"

OTHER_GROUP=$(id -G | tr ' ' '\n' | grep -v -x "$(id -g)" | head -n 1)
if [ -z "$OTHER_GROUP" ]
then
    echo "Skipping, the user running the test has no group other than its primary group"
    exit 0
fi

FILE_A="$WORKDIR/tmp/file_a"
FILE_B="$WORKDIR/tmp/dir_b"
TARGET_C="$WORKDIR/tmp/target_c"
FILE_C="$WORKDIR/tmp/link_c"
EVENTS_FILE="$WORKDIR/tmp/events"
export FILE_A FILE_B FILE_C EVENTS_FILE

# create initial files
touch "$FILE_A"
mkdir "$FILE_B"
touch "$TARGET_C"
ln -s "$TARGET_C" "$FILE_C"

start_reactor "$1"

wait_for_watchers 3

# not owner changes
echo "modified" >> "$FILE_A"
chmod 600 "$FILE_A"
chgrp "$OTHER_GROUP" "$TARGET_C"

# give the watchers a couple of checks to see them, in case they would fire.
# Proving that something doesn't happen needs a wait.
sleep 2
if [ -e "$EVENTS_FILE" ]
then
    echo "Fired on something that is not an owner change:" >&2
    cat "$EVENTS_FILE" >&2
    exit 1
fi

# owner changes
chgrp "$OTHER_GROUP" "$FILE_A"
chgrp "$OTHER_GROUP" "$FILE_B"

# each fired once
wait_for_lines "$EVENTS_FILE" 2
[ "$(cut -d' ' -f3- "$EVENTS_FILE" | sort)" = "$(printf 'A owner changed\nB owner changed')" ]
