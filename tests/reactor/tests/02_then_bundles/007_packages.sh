#!/bin/sh

# check that packages promises in a "then" bundle are evaluated again on
# every event: the package removed after the first one is installed again

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TESTFILE="$WORKDIR/tmp/test"
EVENTS_FILE="$WORKDIR/tmp/events"
INSTALLED_FILE="$WORKDIR/tmp/installed"
export TESTFILE EVENTS_FILE INSTALLED_FILE

# A package module installing packages by adding them to the file given as
# option, see tests/acceptance/07_packages/package_module_path.cf.module
mkdir -p "$WORKDIR/modules/packages"
cat > "$WORKDIR/modules/packages/reactor_test" <<'MODULE'
#!/bin/sh

set -e

remove_prefix()
{
    echo "$1" | sed "s/$2//"
}

case "$1" in
    supports-api-version)
        echo 1
        ;;
    get-package-data)
        while read line; do
            case "$line" in
                File=*)
                    echo PackageType=repo
                    echo Name=`remove_prefix "${line}" "File="`
                    ;;
            esac
        done
        ;;
    list-installed)
        while read line; do
            case "$line" in
                options=*)
                    OUTPUT=`remove_prefix "${line}" "options="`
                    ;;
            esac
        done
        if [ -f "$OUTPUT" ]; then
            cat "$OUTPUT"
        fi
        ;;
    list-*)
        cat > /dev/null
        ;;
    repo-install)
        while read line; do
            case "$line" in
                options=*)
                    OUTPUT=`remove_prefix "${line}" "options="`
                    ;;
                Name=*)
                    NAME=`remove_prefix "${line}" "Name="`
                    ;;
            esac
        done
        echo "Name=$NAME" >> "$OUTPUT"
        echo "Version=1.0" >> "$OUTPUT"
        echo "Architecture=generic" >> "$OUTPUT"
        ;;
    *)
        exit 1
        ;;
esac
MODULE
chmod 700 "$WORKDIR/modules/packages/reactor_test"

touch "$TESTFILE"

start_reactor "$POLICY_FILE"

wait_for_watchers 1

# first event
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "packages repaired"
grep -x "Name=reactor_test_pkg" "$INSTALLED_FILE"

# remove the package
rm "$INSTALLED_FILE"

# second event: recreate the file and wait for the watcher to see it before
# deleting it again
touch "$TESTFILE"
wait_for_log "File '$TESTFILE' is present, watching for its deletion"
rm "$TESTFILE"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 2 "packages repaired"
grep -x "Name=reactor_test_pkg" "$INSTALLED_FILE"
