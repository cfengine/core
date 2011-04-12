#!/bin/sh

srcdir=$(dirname $0)
test -z "$srcdir" && srcdir=.

ORIGDIR=$(pwd)
cd $srcdir

if [ -z "$NO_SUBPROJECTS" ]; then
  #
  # Include nova/constellation
  #

  if [ -d ${srcdir}/../nova ]; then
    ln -sTf ${srcdir}/../nova ${srcdir}/nova
  fi

  if [ -d ${srcdir}/../constellation ]; then
    ln -sTf ${srcdir}/../constellation ${srcdir}/constellation
  fi
else
  #
  # Clean up just in case
  #
  rm -f ${srcdir}/nova
  rm -f ${srcdir}/constellation
fi

autoreconf --force -v --install -I m4 || exit 1
cd $ORIGDIR || exit $?

if [ -z "$NO_CONFIGURE" ]; then
  $srcdir/configure --enable-maintainer-mode "$@"
fi
