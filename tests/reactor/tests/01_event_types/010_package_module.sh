#!/bin/sh

# check the package_module attribute of package events: the named package
# module is used, and the events promise fails if there is no default package
# module or no package module body of that name

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TEST_PACKAGE="reactor_test_pkg"
INSTALLED_FILE="$WORKDIR/tmp/installed"
EVENTS_FILE="$WORKDIR/tmp/events"
export TEST_PACKAGE INSTALLED_FILE EVENTS_FILE

create_package_module reactor_test
set_installed_packages "$INSTALLED_FILE"

start_reactor "$POLICY_FILE"

# logged on registration, in the order of the promises
wait_for_log "No default package module to watch package '$TEST_PACKAGE' with"
wait_for_log "No package module body 'no_such_module' to watch package '$TEST_PACKAGE' with"

# only the watcher with the existing package module
wait_for_watchers 1

wait_for_log "Package '$TEST_PACKAGE' is not installed, watching for its installation"

set_installed_packages "$INSTALLED_FILE" "$TEST_PACKAGE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "installed"
