#!/bin/sh
set -ex

# Usage: run_test.sh TEST
#
# Runs the reactor test TEST (a tests/*/*.sh or tests/*/unsafe/*.sh file) in a
# throwaway work dir, and exits with its result: 0 if it passed, 77 if it was
# skipped, anything else if it failed. Used as the test driver by "make check"
# and by run.sh.
#
# The output of the test, with its set -x trace, is only printed if it didn't
# pass, so that the output of the tests that pass doesn't bury it.
#
# Tests in an unsafe/ directory modify the system they are running on, so they
# are skipped unless UNSAFE_TESTS=1.
#
# The binaries are taken from $CF_REACTOR and $CF_PROMISES if they are set,
# otherwise from $CF_BINDIR if it is set, otherwise from the CFEngine build
# tree this directory is part of if they are built, otherwise from
# /var/cfengine/bin.

if [ $# -ne 1 ]; then
    echo "Usage: $0 TEST" >&2
    exit 2
fi

t=$1

case "$t" in
    */unsafe/*)
        if [ "$UNSAFE_TESTS" != 1 ]; then
            echo "Skipping unsafe test '$t', set UNSAFE_TESTS=1 to run it"
            exit 77
        fi
        ;;
esac

# directory of lib.cf and lib.sh
REACTOR_TESTS_DIR=$(cd "$(dirname "$0")" && pwd)

# Print the path of binary $1, see the usage above.
find_binary() {
    if [ -n "$CF_BINDIR" ]; then
        if [ -x "$CF_BINDIR/$1.exe" ]; then
            echo "$CF_BINDIR/$1.exe"
            return
        fi
        if [ -x "$CF_BINDIR/$1" ]; then
            echo "$CF_BINDIR/$1"
            return
        fi
        echo "Unable to find executable for $1 in $CF_BINDIR"
        exit 1
    elif [ -x "$REACTOR_TESTS_DIR/../../$1/$1" ]; then
        # in-tree build, may be a libtool wrapper script
        echo "$REACTOR_TESTS_DIR/../../$1/$1"
    else
        echo "/var/cfengine/bin/$1"
    fi
}

CF_REACTOR=${CF_REACTOR:-$(find_binary cf-reactor)}
CF_PROMISES=${CF_PROMISES:-$(find_binary cf-promises)}
export CF_REACTOR
export CF_PROMISES

for b in "$CF_REACTOR" "$CF_PROMISES"; do
    if [ ! -x "$b" ]; then
        echo "'$b' not found, build CFEngine or set CF_BINDIR" >&2
        exit 2
    fi
done

# Create the throwaway work dir, with mktemp -d where available
WORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/cf-reactor-test.XXXXXX" 2>/dev/null) || {
    WORKDIR="${TMPDIR:-/tmp}/cf-reactor-test.$$"
    (umask 077 && mkdir "$WORKDIR") || exit 2
}
WORKDIR=$(cd "$WORKDIR" && pwd)
trap 'rm -rf "$WORKDIR"' EXIT
mkdir -p "$WORKDIR/bin" "$WORKDIR/tmp"

ext=""
if echo $CF_PROMISES | grep -q exe; then
    ext=".exe";
fi
ln -s "$CF_PROMISES" "$WORKDIR/bin/cf-promises$ext"

if [ -f "$CF_BINDIR/cf.events.dll" ]; then
    ln -s "$CF_BINDIR/cf.events.dll" "$WORKDIR/bin/cf.events.dll"
fi
echo CRAIG DEBUG
ls -l "$WORKDIR"/bin


# Run a copy of the policy, keeping its path relative to lib.cf, as CFEngine
# refuses to read policy files writable by group or others, which those of a
# checkout made with a umask of 002 are
p="/$t"
rel="tests/${p##*/tests/}"
policy="$WORKDIR/policy/${rel%.sh}.cf"
mkdir -p "$(dirname "$policy")"
cp "$REACTOR_TESTS_DIR/lib.cf" "$WORKDIR/policy/"
cp "${t%.sh}.cf" "$policy"
chmod 600 "$WORKDIR/policy/lib.cf" "$policy"

CFENGINE_TEST_OVERRIDE_WORKDIR="$WORKDIR"
LOG="$WORKDIR/reactor.log"
PROMISE_NAME=$(basename "$t" .sh)
export WORKDIR CFENGINE_TEST_OVERRIDE_WORKDIR LOG PROMISE_NAME

echo "--- Validate test policy $policy ---"
"$CF_PROMISES" -c $policy

echo "--- Begin test $t ---"
bash -ex "$t" "$policy" 2>&1 | tee "$WORKDIR/test.log"
result=$?
if [ "$result" -ne 0 ]; then
    cat "$WORKDIR/test.log"
fi
echo "--- End test $t with exit code $result ---"
exit "$result"
