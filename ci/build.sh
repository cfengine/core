#!/usr/bin/env bash
# build.sh runs autogen/configure and then builds CFEngine core
# the script should take into account the operating system environment and adjust, such as --without-pam on termux, BSDs and such
set -ex
thisdir="$(dirname "$0")"
cd "$thisdir"/..
OPTS="--enable-debug"

if [ -n "$TERMUX_VERSION" ]; then
  OPTS="$OPTS --without-pam"
fi

./autogen.sh $OPTS
make
