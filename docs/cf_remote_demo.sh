#!/usr/bin/env bash

set -e
set -x

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cf-remote spawn --platform ubuntu-22-04-x64 --count 1 --name hub --role hub
cf-remote spawn --platform ubuntu-20-04-x64 --count 1 --name ubuntu-20 --role client
cf-remote spawn --platform ubuntu-18-04-x64 --count 1 --name ubuntu-18 --role client
cf-remote spawn --platform centos-7-x64 --count 1 --name centos-7 --role client
cf-remote spawn --platform debian-11-x64 --count 1 --name debian-11 --role client
cf-remote spawn --platform rhel-7-x64 --count 1 --name rhel-7 --role client

cf-remote --version master install --demo --bootstrap hub --hub hub

# cf-remote deploy --hub hub "$SCRIPTPATH/../../masterfiles"

cf-remote --version master install --demo --bootstrap hub --clients ubuntu-20
cf-remote --version master install --demo --bootstrap hub --clients ubuntu-18
cf-remote --version master install --demo --bootstrap hub --clients centos-7
cf-remote --version master install --demo --bootstrap hub --clients debian-11
cf-remote --version master install --demo --bootstrap hub --clients rhel-7

cf-remote info -H hub
