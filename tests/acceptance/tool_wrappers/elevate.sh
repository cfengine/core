#!/bin/sh

# Launch a command with elevated privileges, but use sh to do it.
# Requires "elevate.exe" from enterprise.
# Be careful what you pass into this script. It cannot handle any redirection
# or spaces in pathnames.

kill_all_subprocesses() {
  SUB_PID="$(cat pid.$$)"
  # Extract sub process group ID.
  SUB_PGID="$(ps -W | egrep "^ *$SUB_PID " | sed -e 's/^ *[0-9]\+ \+[0-9]\+ \+\([0-9]\+\).*/\1/')"
  # Extract list of all processes with that PGID.
  # Note: We extract WINPID, not PID, for use in native Windows command.
  WINPID_LIST="$(ps -W | egrep "^ *[0-9]+ +[0-9]+ +$SUB_PGID " | sed -e 's/^ *[0-9]\+ \+[0-9]\+ \+[0-9]\+ \+\([0-9]\+\).*/\1/')"
  # Kill them all.
  for i in $WINPID_LIST; do
    $0 taskkill -f -t -pid $i
  done
}

touch output.$$

$(dirname $0)/../elevate.exe -wait "$(dirname $0)\template.bat" "cd '`pwd`'; export PATH='$PATH'; $(dirname $0)/preserve-output-and-status.sh $$ $@" &

trap kill_all_subprocesses INT
trap kill_all_subprocesses TERM
# Traps do not fire during commands, but *do* fire during wait.
tail -F output.$$ --pid=$! 2>/dev/null &
wait $!

# All these are written by "preserve-output-and-status.sh", because none of
# them can be preserved across elevate.exe.
rm -f pid.$$
rm -f output.$$
return_code=`cat return-code.$$`
rm -f return-code.$$
exit $return_code
