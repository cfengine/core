#!/bin/sh

LIBPROMISES=`dirname $0`/../../libpromises

if ! $LIBPROMISES/text2cstring.pl $LIBPROMISES/failsafe.cf | diff $LIBPROMISES/bootstrap.inc - >/dev/null; then
    echo "You forgot to update bootstrap.inc after modifying failsafe.cf!"
    exit 1
fi

exit 0
