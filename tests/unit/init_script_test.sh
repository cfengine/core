#!/bin/sh
################################################################################
#
#   Copyright 2017 Northern.tech AS
#
#   This file is part of CFEngine 3 - written and maintained by CFEngine AS.
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; version 3.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
#  To the extent this program is licensed as part of the Enterprise
#  versions of CFEngine, the applicable Commercial Open Source License
#  (COSL) may apply to this file if you as a licensee so wish it. See
#  included file COSL.txt.
#
################################################################################

################################################################################
# Test whether the init script correctly starts and kills processes.
################################################################################

#
# Detect and replace non-POSIX shell
#
try_exec() {
    type "$1" > /dev/null 2>&1 && exec "$@"
}

broken_posix_shell()
{
    unset foo
    local foo=1
    test "$foo" != "1"
}

if broken_posix_shell >/dev/null 2>&1; then
    try_exec /usr/xpg4/bin/sh "$0" "$@"
    echo "No compatible shell script interpreter found."
    echo "Please find a POSIX shell for your system."
    exit 42
fi

################################################################################
# Preparation
################################################################################

export CFTEST_PREFIX=/tmp/init_script_test.sh.$$
# Use alternate binary names to avoid killing actual system processes.
export CFTEST_CFEXECD=$CFTEST_PREFIX/bin/cf-test-execd
export CFTEST_CFSERVD=$CFTEST_PREFIX/bin/cf-test-serverd
export CFTEST_CFMOND=$CFTEST_PREFIX/bin/cf-test-monitord
export CFTEST_CFAGENT=$CFTEST_PREFIX/bin/cf-test-agent

rm -rf $CFTEST_PREFIX
mkdir -p $CFTEST_PREFIX/bin
mkdir -p $CFTEST_PREFIX/inputs

if [ "$1" != "sub-invocation" ]; then
    # Redirect to log, and only print if there's an error.
    if ! "$0" sub-invocation > $CFTEST_PREFIX/test-output.log 2>&1; then
        echo "FAIL: Output from test:"
        cat $CFTEST_PREFIX/test-output.log
        exit 1
    fi
    exit 0
fi

# Fail on any error.
set -e

cp init_script_test_helper $CFTEST_PREFIX/bin/cf-test-execd
cp init_script_test_helper $CFTEST_PREFIX/bin/cf-test-serverd
cp init_script_test_helper $CFTEST_PREFIX/bin/cf-test-monitord
cp init_script_test_helper $CFTEST_PREFIX/bin/cf-test-agent

touch $CFTEST_PREFIX/inputs/promises.cf

if ps --help | egrep -e '--cols\b' > /dev/null; then
    # There is a bug in SUSE which means that ps output will be truncated even
    # when piped to grep, if the terminal size is small. However using --cols
    # will override it.
    PS_OPTIONS="--cols 200"
else
    PS_OPTIONS=
fi

################################################################################
# Functions
################################################################################

match_pid()
{
    if [ "$(ps $PS_OPTIONS -ef|grep -v grep|grep "$1"|awk -F' ' '{print $2}')" != "" ]
    then
        return 0
    else
        return 1
    fi
}

matching_pid_exists()
{
    if ! match_pid "$1"; then
        echo "FAIL: No such process: $1"
        return 1
    fi
}

no_matching_pid_exists()
{
    if match_pid "$1"; then
        echo "FAIL: Unexpected process: $1"
        return 1
    fi
}

# Shortcut to check that no daemons/agents are running.
verify_none_running()
{
    no_matching_pid_exists $CFTEST_PREFIX/bin/cf-test-execd
    no_matching_pid_exists $CFTEST_PREFIX/bin/cf-test-serverd
    no_matching_pid_exists $CFTEST_PREFIX/bin/cf-test-monitord
    no_matching_pid_exists $CFTEST_PREFIX/bin/cf-test-agent
}

# Simply a workaround for the fact that '!' disables error checking with "set
# -e". By using it inside this function, the caller preserves error checking.
negate()
{
    ! "$@"
}

################################################################################
# Test
################################################################################

# Enable verbose mode.
set -v

# Normal starting.
../../misc/init.d/cfengine3 start
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-execd
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-serverd
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-monitord

# Normal stopping.
../../misc/init.d/cfengine3 stop
verify_none_running

# Stopping when a daemon is missing.
$CFTEST_PREFIX/bin/cf-test-execd
../../misc/init.d/cfengine3 stop
verify_none_running
$CFTEST_PREFIX/bin/cf-test-monitord
../../misc/init.d/cfengine3 stop
verify_none_running

# Stopping daemons and agents
$CFTEST_PREFIX/bin/cf-test-execd --spawn-process $CFTEST_PREFIX/bin cf-test-agent
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-execd
../../misc/init.d/cfengine3 stop
verify_none_running

# Stopping daemons and an agent launched just at signal time
$CFTEST_PREFIX/bin/cf-test-execd --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-execd
no_matching_pid_exists $CFTEST_PREFIX/bin/cf-test-agent
../../misc/init.d/cfengine3 stop
verify_none_running

# Stopping a chain of daemons and agents each spawning each other.
$CFTEST_PREFIX/bin/cf-test-execd \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-execd
no_matching_pid_exists $CFTEST_PREFIX/bin/cf-test-agent
../../misc/init.d/cfengine3 stop
verify_none_running

# Stopping a ludicrously long chain of daemons and agents each spawning each
# other. This will test the SIGKILL capacity of the script, since normal killing
# won't be enough before the iteration count runs out.
$CFTEST_PREFIX/bin/cf-test-execd \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-execd
no_matching_pid_exists $CFTEST_PREFIX/bin/cf-test-agent
../../misc/init.d/cfengine3 stop
verify_none_running

# Stopping a daemon that just won't die.
$CFTEST_PREFIX/bin/cf-test-execd --refuse-to-die
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-execd
../../misc/init.d/cfengine3 stop
verify_none_running

# Most extreme case, a long chain of daemons and agents where they all refuse to
# die.
$CFTEST_PREFIX/bin/cf-test-execd \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-execd --pass-to-next-process \
    --refuse-to-die --spawn-process-on-signal $CFTEST_PREFIX/bin cf-test-agent
matching_pid_exists $CFTEST_PREFIX/bin/cf-test-execd
no_matching_pid_exists $CFTEST_PREFIX/bin/cf-test-agent
../../misc/init.d/cfengine3 stop
verify_none_running

# Verify that status of one daemon works.
verify_none_running
$CFTEST_PREFIX/bin/cf-test-execd
OUTPUT="$(../../misc/init.d/cfengine3 status)"
echo "$OUTPUT"
for i in cf-test-serverd cf-test-monitord; do
    echo "$OUTPUT" | grep "$i.*is not running"
done
echo "$OUTPUT" | egrep "cf-test-execd.*(\.|is )running"
echo "$OUTPUT" | negate grep -i "Warning"

# Verify that status of all daemons works.
$CFTEST_PREFIX/bin/cf-test-serverd
$CFTEST_PREFIX/bin/cf-test-monitord
OUTPUT="$(../../misc/init.d/cfengine3 status)"
echo "$OUTPUT"
for i in cf-test-execd cf-test-serverd cf-test-monitord; do
    echo "$OUTPUT" | egrep "$i.*(\.|is )running"
done
echo "$OUTPUT" | negate grep -i "is not running"
echo "$OUTPUT" | negate grep -i "Warning"

# Verify that status of no daemons works.
../../misc/init.d/cfengine3 stop
verify_none_running
OUTPUT="$(../../misc/init.d/cfengine3 status)"
echo "$OUTPUT"
for i in cf-test-execd cf-test-serverd cf-test-monitord; do
    echo "$OUTPUT" | grep "$i.*is not running"
done
echo "$OUTPUT" | negate egrep -i "(\.|is )running"
echo "$OUTPUT" | negate grep -i "Warning"

# Verify that we get warnings about multiple daemons.
verify_none_running
$CFTEST_PREFIX/bin/cf-test-execd
$CFTEST_PREFIX/bin/cf-test-execd
OUTPUT="$(../../misc/init.d/cfengine3 status)"
echo "$OUTPUT"
for i in cf-test-serverd cf-test-monitord; do
    echo "$OUTPUT" | grep "$i.*is not running"
done
echo "$OUTPUT" | egrep "cf-test-execd.*(\.|is )running"
echo "$OUTPUT" | grep -i "Warning.*multiple"

# Verify that we get warnings about wrong PID file.
../../misc/init.d/cfengine3 stop
verify_none_running
$CFTEST_PREFIX/bin/cf-test-execd
echo 9999999 > $CFTEST_PREFIX/cf-test-execd.pid
OUTPUT="$(../../misc/init.d/cfengine3 status)"
echo "$OUTPUT"
for i in cf-test-serverd cf-test-monitord; do
    echo "$OUTPUT" | grep "$i.*is not running"
done
echo "$OUTPUT" | egrep "cf-test-execd.*(\.|is )running"
echo "$OUTPUT" | grep -i "Warning.*$CFTEST_PREFIX/cf-test-execd.pid"

################################################################################
# Cleanup
################################################################################

../../misc/init.d/cfengine3 stop

rm -rf $CFTEST_PREFIX
