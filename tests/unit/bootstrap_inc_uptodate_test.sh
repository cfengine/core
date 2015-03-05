#!/bin/sh

if [ "`uname`" != "Linux" ]; then
    # Skip (77) test on non-Linux platforms, since text2cstring.pl hasn't been
    # made portable. If the test passes on Linux, it's valid everywhere anyway.
    exit 77
fi

LIBPROMISES=`dirname $0`/../../libpromises

if ! $LIBPROMISES/text2cstring.pl $LIBPROMISES/failsafe.cf | diff $LIBPROMISES/bootstrap.inc - >/dev/null; then
    echo "You forgot to update bootstrap.inc after modifying failsafe.cf!"
    exit 1
fi

exit 0
