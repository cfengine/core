#!/usr/bin/env bash
#
# Set the copyright year shown in the help output (`--help`).
#
# The year is a plain literal in configure.ac. This script rewrites that literal
# and nothing else -- committing it and opening a pull request is left to the
# caller, see .github/workflows/copyright_year.yml.
#
# The year it settled on is printed on stdout, everything else goes to stderr:
#
#   year=$(ci/update-copyright-year.sh)       # the current year
#   year=$(ci/update-copyright-year.sh 2027)  # a specific year

set -eu

year=${1:-$(date -u +%Y)}

case "$year" in
    [0-9][0-9][0-9][0-9]) ;;
    *)
        echo "$0: '$year' is not a four digit year" >&2
        exit 1
        ;;
esac

cd "$(dirname "$0")/.."

current=$(sed -nE 's/^AC_DEFINE\(CF_COPYRIGHT, "([0-9]{4}) Northern\.tech AS".*/\1/p' configure.ac)

# Fail loudly rather than silently doing nothing if the define is ever renamed
# or reformatted -- a silent no-op here is exactly how the year went three
# releases without being updated.
if [ -z "$current" ]; then
    echo "$0: no CF_COPYRIGHT define in the expected form in configure.ac, update this script" >&2
    exit 1
fi

if [ "$current" = "$year" ]; then
    echo "configure.ac: copyright year is already $year" >&2
else
    sed -i -E \
        "s/^(AC_DEFINE\(CF_COPYRIGHT, \")[0-9]{4}( Northern\.tech AS\")/\1$year\2/" \
        configure.ac
    echo "configure.ac: copyright year $current -> $year" >&2
fi

echo "$year"
