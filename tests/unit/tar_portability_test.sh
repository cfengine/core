#!/bin/sh

if [ "$(uname)" != "Linux" ]
then
    # Skip.
    exit 77
fi

cd "$(dirname $0)/../.."

tar --format=ustar -cf /dev/null *
exit $?
