#!/bin/sh

srcdir=$(dirname $0)
test -z "$srcdir" && srcdir=.

ORIGDIR=$(pwd)
cd $srcdir

autoreconf --force -v --install || exit 1
cd $ORIGDIR || exit $?

if [ -z "$NO_CONFIGURE" ]; then
  $srcdir/configure --enable-maintainer-mode "$@"
fi
