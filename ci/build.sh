#!/usr/bin/env bash
# build.sh runs after dependencies and configure scripts and builds CFEngine core
# the script should take into account the operating system environment and adjust, such as --without-pam on termux, BSDs and such
set -ex
thisdir="$(dirname "$0")"
cd "$thisdir"/..

source /etc/os-release
CFLAGS="-Wall"
if [ "$ID" != "alpine" ]; then
  CFLAGS="$CFLAGS -Werror"
  # on alpine, with lib musl, there are not re-entrant random functions e.g. srand48_r, lrand48_r see CFE-4654
fi
export CFLAGS
make -j8
