#!/bin/sh

# Run a command and store its output and return code.

PID=$1
shift
CWD=`pwd`
$@ >& output.$PID
RETURN_CODE=$?
cd $CWD
echo $RETURN_CODE > return-code.$PID
