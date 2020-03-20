#!/usr/bin/env bash

# This test / example shows how to spawn (windows) instances using cf-remote
# can be useful for manual testing, or be converted to an automated test.

set -e
set -x

# Note, cf-remote uses private AMIs, you may have to create your own and add
# them to cloud_data.py

cf-remote destroy --all
cf-remote spawn --count 1 --platform ubuntu-18-04-x64 --role hub --name hub
cf-remote spawn --count 1 --platform windows-2012-x64 --role client --name twelve
cf-remote spawn --count 1 --platform windows-2016-x64 --role client --name sixteen
sleep 60
cf-remote --version master install --bootstrap hub --hub hub --clients sixteen,twelve
sleep 60
cf-remote sudo --raw -H hub "/var/cfengine/bin/psql -d cfdb -c 'SELECT * FROM __hosts;'"
cf-remote destroy --all
