#!/usr/bin/env bash

set -e
set -x

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cf-remote spawn --platform ubuntu-18-04-x64 --count 1 --name hub --role hub
cf-remote spawn --platform ubuntu-18-04-x64 --count 1 --name ubuntu18 --role client
cf-remote spawn --platform ubuntu-16-04-x64 --count 1 --name ubuntu16 --role client
cf-remote spawn --platform centos-7-x64 --count 1 --name centos7 --role client
cf-remote spawn --platform debian-9-x64 --count 1 --name debian9 --role client
cf-remote spawn --platform rhel-7-x64 --count 1 --name rhel7 --role client

cf-remote --version master install --demo --bootstrap hub --hub hub

# cf-remote deploy --hub hub "$SCRIPTPATH/../../masterfiles"

cf-remote --version master install --demo --bootstrap hub --clients ubuntu18
cf-remote --version master install --demo --bootstrap hub --clients ubuntu16
cf-remote --version master install --demo --bootstrap hub --clients centos7
cf-remote --version master install --demo --bootstrap hub --clients debian9
cf-remote --version master install --demo --bootstrap hub --clients rhel7

cf-remote info -H hub
