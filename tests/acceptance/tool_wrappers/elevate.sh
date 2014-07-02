#!/bin/sh

# Launch a command with elevated privileges, but use sh to do it.
# Requires "elevate.exe" from enterprise.
# Be careful what you pass into this script. It cannot handle any redirection
# or spaces in pathnames.

$(dirname $0)/../elevate.exe -wait "$(dirname $0)\template.bat" "cd '`pwd`'; export PATH='$PATH'; $(dirname $0)/store-return-code.sh $@"
# Elevate does not preserve return code, so use the return code we stored above.
return_code=`cat return-code.txt`
rm -f return-code.txt
exit $return_code
