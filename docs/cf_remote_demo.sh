#!/usr/bin/env bash

set -e
set -x

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cf-remote spawn --platform ubuntu-24-04-x64 --count 1 --name hub --role hub
cf-remote spawn --platform ubuntu-24-04-x64 --count 1 --name ubuntu-24 --role client
cf-remote spawn --platform ubuntu-22-04-x64 --count 1 --name ubuntu-22 --role client
cf-remote spawn --platform debian-13-x64 --count 1 --name debian-13 --role client
cf-remote spawn --platform rhel-10-x64 --count 1 --name rhel-10 --role client

cf-remote --version master install --demo --bootstrap hub --hub hub

# cf-remote deploy --hub hub "$SCRIPTPATH/../../masterfiles"

cf-remote --version master install --demo --bootstrap hub --clients ubuntu-24
cf-remote --version master install --demo --bootstrap hub --clients ubuntu-22
cf-remote --version master install --demo --bootstrap hub --clients debian-13
cf-remote --version master install --demo --bootstrap hub --clients rhel-10

cf-remote info -H hub
