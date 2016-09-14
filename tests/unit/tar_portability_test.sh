#!/bin/sh

if [ "$(uname)" != "Linux" ]
then
    # Skip.
    exit 77
fi

cd "$(dirname $0)/../.."

tar --exclude="tests/acceptance/workdir" --format=ustar -cf /dev/null *

exit $?
