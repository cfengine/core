#!/bin/sh

# Usage: run.sh [--unsafe]
#
# Runs all the reactor tests, see run_test.sh, and prints a summary of their
# results. The output of a test is only printed if it failed. They are also
# run by "make check".
#
# Tests in an unsafe/ directory modify the system they are running on, so they
# are skipped unless run with --unsafe (or UNSAFE_TESTS=1).
#
# The binaries are taken from $CF_BINDIR if it is set, otherwise from the
# CFEngine build tree this directory is part of if they are built, otherwise
# from /var/cfengine/bin.

if [ "$1" = "--unsafe" ]; then
    UNSAFE_TESTS=1
fi
export UNSAFE_TESTS

dir=$(cd "$(dirname "$0")" && pwd)

# for now()
. "$dir/lib.sh"

passed=0
failed=0
skipped=0
total=0
failures=""
suite_start=$(now)

# Run test $1 and print its result, with its output if it failed
run_test() {
    name=${1#"$dir/tests/"}
    name=${name%.sh}
    total=$((total + 1))

    start=$(now)
    output=$(sh "$dir/run_test.sh" "$1" 2>&1)
    result=$?
    elapsed=$(($(now) - start))

    case $result in
        0)
            passed=$((passed + 1))
            echo "--- PASS: $name (${elapsed}s)"
            ;;
        77)
            skipped=$((skipped + 1))
            echo "--- SKIP: $name (${elapsed}s)"
            ;;
        *)
            failed=$((failed + 1))
            failures="$failures  $name
"
            echo "--- FAIL: $name (${elapsed}s)"
            echo "$output"
            echo "---"
            ;;
    esac
}

for t in "$dir"/tests/*/*.sh "$dir"/tests/*/unsafe/*.sh; do
    # skip the pattern itself if there are no unsafe tests
    [ -f "$t" ] || continue
    run_test "$t"
done

echo ""
echo "=============================="
echo "Test Results: $passed passed, $failed failed, $skipped skipped (total: $total, $(($(now) - suite_start))s)"
if [ "$failed" -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    printf '%s' "$failures"
    exit 1
fi
echo "=============================="
echo "All reactor tests passed"
