#!/bin/sh

# Run a command and store its return code.

CWD=`pwd`
$@
RETURN_CODE=$?
cd $CWD
echo $RETURN_CODE > return-code.txt
