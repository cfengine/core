#!/usr/bin/env bash
# configure.sh runs autotools/configure as appropriate for the current environment
# the script should take into account the operating system environment and adjust, such as --without-pam on termux, BSDs and such
set -ex
thisdir="$(dirname "$0")"
cd "$thisdir"/..
OPTS="--enable-debug"

if [ -n "$TERMUX_VERSION" ]; then
  OPTS="$OPTS --without-pam"
fi

./autogen.sh $OPTS
