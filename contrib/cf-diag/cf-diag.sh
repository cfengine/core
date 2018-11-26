#!/bin/sh
# Author: Mike Weilgart <mike.weilgart@verticalsysadmin.com>
#   (inspired by Aleksey Tsalolikhin)
# Date: 8 January 2018
# Purpose: Test the CFEngine health of a host
# and report on it using Test Anything Protocol
# (https://testanything.org/)

ok_versions='
-e 3.7.3
-e 3.10.2
' # to be fed to: grep -qFx $ok_versions

################################## FRAMEWORK ##################################

# You shouldn't need to modify anything in this section.
# Skip ahead to TESTS instead.

test_number=0

outcome() {
  # Accepts from one to three args:
  # First - outcome, should be 'ok' or 'not ok'
  # Second - optional description
  # Third - optional directive
  # (see TAP specification https://testanything.org/tap-specification.html)

  # (Third arg not currently used.)

  test_number="$((test_number+1))"
  printf '%s %d%s%s\n' "$1" "$test_number" ${2+" - $2"} ${3+" # $3"}
  # See https://unix.stackexchange.com/a/68488/135943
  # for the basis of the shell trick in use here
}

succeed() {
  [ -t 1 ] && printf '\e[0;32m'
  outcome ok "$@"
  [ -t 1 ] && printf '\e[0m'
}

fail() {
  [ -t 1 ] && printf '\e[0;31m'
  outcome 'not ok' "$@"
  [ -t 1 ] && printf '\e[0m'
}

bail() {
  printf 'Bail out!%s\n' ${1+" $1"}
  exit
}

trap 'printf "1..%d\n" "$test_number"' EXIT

#################################### TESTS ####################################

# I recommend custom checks be added only at the end.
# Skip ahead to CUSTOM TESTS.

# Show host key (digest)
printf '# '
cf-key -p /var/cfengine/ppkeys/localhost.pub 2>/dev/null || echo SHA not available

# VERSION CHECKS
# makes use of 'ok_versions' defined at top of script

version_data="$(cf-agent -V 2>/dev/null)"
if [ "$?" = 127 ]; then # Don't go further if cf-agent missing
  bail "$(cf-agent -V 2>&1 1>/dev/null)"
fi

# Beware literal newlines; don't try adding trailing comments!
core_version_line="${version_data%
*}" # e.g. 'CFEngine Core 3.7.3'
core_version="${core_version_line##* }" # e.g. '3.7.3'
enterprise_version_line="${version_data#*
}" # e.g. 'CFEngine Enterprise 3.7.3'
enterprise_version="${enterprise_version_line##* }" # e.g. '3.7.3'

desc="Expected Core version"
if printf '%s\n' "$core_version" | grep -qFx $ok_versions; then
  succeed "$desc"
else
  fail "$desc"
fi

desc="Expected Enterprise version"
if printf '%s\n' "$enterprise_version" | grep -qFx $ok_versions; then
  succeed "$desc"
else
  fail "$desc"
fi

desc="Core version matches Enterprise version"
# In pathological cases these may not match.
# The individual checks for Core and Enterprise versions ok
# may not detect these pathological cases if e.g. 3.7.3 and 3.10.2
# are both allowed ("okay") versions, so we test it separately.
if [ "$core_version" = "$enterprise_version" ]; then
  succeed "$desc"
else
  fail "$desc"
fi

# Show actual version data
# (Can be commented out if you only allow one version anyway.)
printf '# %s\n' "$core_version_line" "$enterprise_version_line"

# CFENGINE FUNCTIONING

# Check all three daemons as separate tests
for d in cf-execd cf-monitord cf-serverd; do
  desc="$d running"
  if pgrep "$d" >/dev/null 2>/dev/null; then
    succeed "$desc"
  else
    fail "$desc"
  fi
done

# Test for recent run:

# 15 minutes is three times the out-of-the-box default run interval (5 min)
# However, as noted by Neil Watson
# (in this ticket: https://tracker.mender.io/browse/CFE-769),
# most large CFEngine installations use longer (less frequent) run intervals.

# Note also THIS TEST MAY FAIL ON NON-LINUX SYSTEMS.
# There is no portable approach I can find for this 'stat' command.
# However, I consider this a minor (negligible) issue.
# (It will also fail with coreutils 8.6, but 8.5 and 8.7+ are fine:
# see https://unix.stackexchange.com/q/3348/135943.)  :)

desc="cf-agent ran in last 15 minutes"
if [ "$(($(date +%s)-$(stat -c %Y /var/cfengine/promise_summary.log)))" -lt 900 ]; then
  succeed "$desc"
else
  fail "$desc"
fi

# HUB CHECKS

desc="Bootstrapped to hub (policy_server.dat exists)"
if [ -f /var/cfengine/policy_server.dat ]; then
  succeed "$desc"
  hub_ip="$(cat /var/cfengine/policy_server.dat)"
  printf '# Hub IP is %s\n' "$hub_ip"
else
  fail "$desc"
  bail 'Not connected to any hub.'
fi

desc="Hub is connecting to host"
if cf-key -s | grep ^Incoming | grep -qF "$hub_ip"; then
  succeed "$desc"

  hub_lastseen="$(cf-key -s |grep -F "$hub_ip" |awk '/^Incoming/{print $4, $5, $6, $7, $8}')"
  hub_lastseen_in_seconds="$(date +%s -d "$hub_lastseen")"
  now="$(date +%s)"
  seconds_since_conn="$((now-hub_lastseen_in_seconds))"

  desc="Hub connected to host within last 15 minutes"
  if [ "$seconds_since_conn" -lt 900 ]; then
    succeed "$desc"
  else
    fail "$desc"
  fi
  printf '# Has been %d seconds since last connection\n' "$seconds_since_conn"

else
  # Hub connection fail
  fail "$desc"
fi

### SYSTEM CHECKS

disk_usage="$(df -P / | awk '/\//{print $5}')"

desc="Filesystem has free space"
if [ "$disk_usage" = '100%' ]; then
  fail "$desc"
  bail "Can't continue with full disk"
else
  succeed "$desc"
  printf '# Disk usage is %s\n' "$disk_usage"
fi

desc="Filesystem is writable"
if touch /tmp/cf_diag_test 2>/dev/null; then
  succeed "$desc"
  rm /tmp/cf_diag_test
else
  fail "$desc"
  bail "Can't continue with read-only filesystem"
fi

### POLICY CHECKS

for f in promises.cf update.cf; do
  desc="$f exists"
  if [ -f /var/cfengine/inputs/"$f" ]; then
    succeed "$desc"
  else
    fail "$desc"
    bail "No $f"
  fi
done

# The tests below this point may take a long time to run,
# depending on how efficiently your CFEngine policy is written.

for f in promises.cf update.cf; do
  desc="$f is runnable"
  if cf-agent -f "$f" >/dev/null 2>&1; then
    succeed "$desc"
  else
    fail "$desc"
    bail "$f not runnable"
  fi
done

desc="Update run fixes changes to policy"
# Running update policy (after deleting flag file)
# should repair alteration of promises.cf
echo '#cf-diag_comment' >> /var/cfengine/inputs/promises.cf
rm -f /var/cfengine/inputs/cf_promises_validated
cf-agent -K -f update.cf >/dev/null 2>&1
if grep -q cf-diag_comment /var/cfengine/inputs/promises.cf; then
  fail "$desc"
else
  succeed "$desc"
fi

############################### CUSTOM TESTS ##################################

# Add custom tests here (if you really need them).
# Use the general form:
#
#   desc="Name of test (descriptive string)"
#   if some-command-that-should-succeed; then
#     succeed "$desc"
#   else
#     fail "$desc"
#   fi
