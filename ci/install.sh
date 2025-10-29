#!/usr/bin/env bash
# install.sh ensures dependencies are installed, builds core, then installs it
set -ex
thisdir=$(dirname $0)
"$thisdir"/dependencies.sh
"$thisdir"/configure.sh
"$thisdir"/build.sh
cd "$thisdir"/..
GAINROOT=""
if [ ! -n "$TERMUX_VERSION" ]; then
  GAINROOT="sudo"
fi

$GAINROOT make install
