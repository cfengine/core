#!/bin/sh

# check that package_installed and package_uninstalled fire once when a
# package is installed and uninstalled, with the default package module of
# body common control. Only the exact name matches: another package whose
# name starts with it stays installed.

set -ex

. "$(dirname "$0")/../../lib.sh"

POLICY_FILE="$1"

TEST_PACKAGE="reactor_test_pkg"
OTHER_PACKAGE="reactor_test_pkg-dev"
INSTALLED_FILE="$WORKDIR/tmp/installed"
EVENTS_FILE="$WORKDIR/tmp/events"
export TEST_PACKAGE INSTALLED_FILE EVENTS_FILE

create_package_module reactor_test
set_installed_packages "$INSTALLED_FILE" "$OTHER_PACKAGE"

start_reactor "$POLICY_FILE"

wait_for_watchers 2

# checked once the first listing is done
wait_for_log "Package '$TEST_PACKAGE' is not installed, watching for its installation"

set_installed_packages "$INSTALLED_FILE" "$OTHER_PACKAGE" "$TEST_PACKAGE"
wait_for_lines "$EVENTS_FILE" 1
check_event "$EVENTS_FILE" 1 "installed"

wait_for_log "Package '$TEST_PACKAGE' is installed, watching for its uninstallation"

set_installed_packages "$INSTALLED_FILE" "$OTHER_PACKAGE"
wait_for_lines "$EVENTS_FILE" 2
check_event "$EVENTS_FILE" 2 "uninstalled"
