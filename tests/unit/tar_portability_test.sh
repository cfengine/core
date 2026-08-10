#!/bin/sh

if [ "$(uname)" != "Linux" ]
then
    # Skip.
    exit 77
fi

cd "$(dirname "$0")"/../.. || exit 1

# Clear TAR_OPTIONS: Makefile.am sets it for "make dist", and the --pax-option
# in there only works on posix archives, not on the ustar one made here.
TAR_OPTIONS='' tar --exclude="tests/acceptance/workdir" --format=ustar -cf /dev/null *

exit $?
