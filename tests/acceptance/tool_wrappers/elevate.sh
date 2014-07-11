#!/bin/sh

# Launch a command with elevated privileges, but use sh to do it.
# Requires "elevate.exe" from enterprise.
# Be careful what you pass into this script. It cannot handle any redirection
# or spaces in pathnames.

touch output.$$

$(dirname $0)/../elevate.exe -wait "$(dirname $0)\template.bat" "cd '`pwd`'; export PATH='$PATH'; $(dirname $0)/preserve-output-and-status.sh $$ $@" &

trap '$0 kill `cat pid.$$`' INT
trap '$0 kill `cat pid.$$`' TERM
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
