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
    if [ "$(id -u)" != "0" ]; then
        if ! command -v sudo >/dev/null; then
            echo "Sorry, run $0 as root or install and configure sudo."
            exit 1
        fi
        GAINROOT="sudo"
    fi
fi

$GAINROOT make install
CFKEY=$(command -v cf-key)
if ! command -v cf-key; then
  CFKEY=/var/cfengine/bin/cf-key
  if [ ! -f /var/cfengine/bin/cf-key ]; then
    echo "Can't find cf-key in PATH or at /var/cfengine/bin/cf-key. You will need to generate a hostkey yourself."
    exit 0
  fi
fi
echo Generating hostkey with $CFKEY
$GAINROOT $CFKEY
