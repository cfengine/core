#!/bin/sh

srcdir=$(dirname $0)
test -z "$srcdir" && srcdir=.

ORIGDIR=$(pwd)
cd $srcdir

if [ -z "$NO_SUBPROJECTS" ]; then
  #
  # Include nova
  #
  for s in nova; do
    if [ -d ${srcdir}/../${s} ]; then
      if [ -h ${srcdir}/${s} ]; then
        rm -f ${srcdir}/${s}
      fi
      ln -sf ${srcdir}/../${s} ${srcdir}/${s}
    fi
  done
else
  #
  # Clean up just in case
  #
  rm -f ${srcdir}/nova
fi

autoreconf -Wno-portability --force --install -I m4 || exit 1
cd $ORIGDIR || exit $?

if [ -z "$NO_CONFIGURE" ]; then
  $srcdir/configure --enable-maintainer-mode "$@"
fi
