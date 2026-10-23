#!/bin/sh

# check what is a permissions change for a file_permissions_changed event,
# which looks at the permission bits of the path, including the setuid,
# setgid and sticky bits, without following it if it is a symlink:
#   A. a regular file: writing to it, touching it or setting the permissions
#      it already has is not a permissions change, changing them is
#   B. a directory: setting its sticky bit is a permissions change
#   C. a symlink: changing the permissions of its target is not a
#      permissions change

set -ex

. "$(dirname "$0")/../../lib.sh"

FILE_A="$WORKDIR/tmp/file_a"
FILE_B="$WORKDIR/tmp/dir_b"
TARGET_C="$WORKDIR/tmp/target_c"
FILE_C="$WORKDIR/tmp/link_c"
EVENTS_FILE="$WORKDIR/tmp/events"
export FILE_A FILE_B FILE_C EVENTS_FILE

# create initial files
touch "$FILE_A"
chmod 600 "$FILE_A"
mkdir "$FILE_B"
chmod 755 "$FILE_B"
touch "$TARGET_C"
chmod 600 "$TARGET_C"
ln -s "$TARGET_C" "$FILE_C"

start_reactor "$1"

wait_for_watchers 3

# not permissions changes
echo "modified" >> "$FILE_A"
touch "$FILE_A"
chmod 600 "$FILE_A"
chmod 640 "$TARGET_C"

# give the watchers a couple of checks to see them, in case they would fire.
# Proving that something doesn't happen needs a wait.
sleep 2
if [ -e "$EVENTS_FILE" ]
then
    echo "Fired on something that is not a permissions change:" >&2
    cat "$EVENTS_FILE" >&2
    exit 1
fi

# permissions changes
chmod 640 "$FILE_A"
chmod +t "$FILE_B"

# each fired once
wait_for_lines "$EVENTS_FILE" 2
[ "$(cut -d' ' -f3- "$EVENTS_FILE" | sort)" = "$(printf 'A permissions changed\nB permissions changed')" ]
