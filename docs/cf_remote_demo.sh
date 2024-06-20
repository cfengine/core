#!/usr/bin/env bash

set -e
set -x

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cf-remote spawn --platform ubuntu-22-04-x64 --count 1 --name hub --role hub
cf-remote spawn --platform ubuntu-22-04-x64 --count 1 --name ubuntu-22 --role client
cf-remote spawn --platform ubuntu-20-04-x64 --count 1 --name ubuntu-20 --role client
cf-remote spawn --platform debian-12-x64 --count 1 --name debian-12 --role client
cf-remote spawn --platform rhel-8-x64 --count 1 --name rhel-8 --role client

cf-remote --version master install --demo --bootstrap hub --hub hub

# cf-remote deploy --hub hub "$SCRIPTPATH/../../masterfiles"

cf-remote --version master install --demo --bootstrap hub --clients ubuntu-22
cf-remote --version master install --demo --bootstrap hub --clients ubuntu-20
cf-remote --version master install --demo --bootstrap hub --clients debian-12
cf-remote --version master install --demo --bootstrap hub --clients rhel-8

cf-remote info -H hub
