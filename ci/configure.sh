#!/usr/bin/env bash
# configure.sh runs autotools/configure as appropriate for the current environment
# the script should take into account the operating system environment and adjust, such as --without-pam on termux, BSDs and such
set -ex
thisdir="$(dirname "$0")"
cd "$thisdir"/..
OPTS="--enable-debug"

if [ -f /etc/os-release ]; then
  source /etc/os-release
  if [ "$ID" = "alpine" ]; then
      OPTS="" # we don't want --enable-debug so that libpromises/dbm_test_api is not built due to lack of srand48_r() and friends on alpine linux libmusl
  fi
fi

if [ -n "$TERMUX_VERSION" ] || [ "$ID" = "alpine" ]; then
    OPTS="$OPTS --without-pam"
fi
if [ -n "$TERMUX_VERSION" ]; then
    export LDFLAGS+=" -landroid-glob"
    OPTS="$OPTS --prefix=$PREFIX\
      --with-workdir=$PREFIX/var/lib/cfengine \
      --without-selinux-policy \
      --without-systemd-service"
fi

./autogen.sh $OPTS
