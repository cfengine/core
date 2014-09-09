#!/bin/sh
try_exec() {
  type "$1" > /dev/null 2>&1 && exec "$@"
}

unset foo
(: ${foo%%bar}) 2> /dev/null
T1="$?"

if test "$T1" != 0; then
  try_exec /usr/xpg4/bin/sh "$0" "$@"
  echo "No compatible shell script interpreter found."
  echo "Please find a POSIX shell for your system."
  exit 42
fi

#
# Do not set -e before switching to POSIX shell, as it will break the test
# above.
#
set -e

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

autoreconf -Wno-portability --force --install -I m4 || exit 1
cd $ORIGDIR || exit $?

if [ -z "$NO_CONFIGURE" ]; then
  $srcdir/configure --enable-maintainer-mode "$@"
fi
