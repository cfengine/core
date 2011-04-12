#!/bin/sh
set -e

if [ -z "$FORCE_SVNVERSION" ]; then
  if grep AM_INIT_AUTOMAKE configure.ac | grep -q svnversion; then
    echo "Packge version in configure.ac contains SVN revision: "
    grep AM_INIT_AUTOMAKE configure.ac
    echo "Bailing out!"
    exit 1
  fi
fi

NO_SUBPROJECTS=1 ./autogen.sh --disable-maintainer-mode

make -j8
make dist
