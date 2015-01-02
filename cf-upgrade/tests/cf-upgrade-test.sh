#!/bin/sh

# This is the test suite for cf-upgrade. It is completely written in shell scripts
# and covers as much as possible.

# Some global variables
CF_UPGRADE=./cf-upgrade
GOOD_PACKAGE_MANAGER="tests/good.sh"
BAD_PACKAGE_MANAGER="tests/bad.sh"
DUMB_PACKAGE_MANAGER="tests/pm.sh"
GOOD_BACKUP="tests/good.sh"
BAD_BACKUP="tests/bad.sh"
BACKUP_FILE="tests/backup.tar.gz"
TEST_RESULT="notrun"
TEST_FOLDER=""
TEST_NAME=""

create_temporary_folder() {
# Create a folder to use as CFEngine folder.
  if [ -x /bin/mktemp ];
  then
    TEST_FOLDER=`mktemp --directory --tmpdir cf-upgrade-folder-XXX`
    touch $TEST_FOLDER/cf-upgrade-test
  else
# No GNU tools, let's do it the old way.
    TEST_FOLDER=/tmp/cf-upgrade-test$$
    mkdir -p $TEST_FOLDER
    touch $TEST_FOLDER/cf-upgrade-test
  fi
}

# First test, request help
TEST_NAME="Help argument";
CL="$CF_UPGRADE -h"
TEST_OUTPUT=`$CL`
if [  $? -ne 0 ];
then
    echo "$TEST_NAME: Failure";
    echo "*** Debug info below ***";
    echo "command line: $CL";
    echo "output:";
    echo "------- output start -------";
    echo "$TEST_OUTPUT"
    echo "-------- output end --------";
else
    echo "$TEST_NAME: Success";
fi
# Second test, version
TEST_NAME="Version argument";
CL="$CF_UPGRADE -v"
TEST_OUTPUT=`$CL`
if [  $? -ne 0 ];
then
    echo "$TEST_NAME: Failure";
    echo "*** Debug info below ***";
    echo "command line: $CL";
    echo "output:";
    echo "------- output start -------";
    echo "$TEST_OUTPUT"
    echo "-------- output end --------";
else
    echo "$TEST_NAME: Success";
fi          

# Third test, wrong parameter
TEST_NAME="Wrong parameter";
CL="$CF_UPGRADE -q"
TEST_OUTPUT=`$CL`
if [  $? -eq 0 ];
then
    echo "$TEST_NAME: Failure";
    echo "*** Debug info below ***";
    echo "command line: $CL";
    echo "output:";
    echo "------- output start -------";
    echo "$TEST_OUTPUT"
    echo "-------- output end --------";
else
    echo "$TEST_NAME: Success";
fi          

# Fourth test, all good
TEST_NAME="Normal command line";
CL="$CF_UPGRADE -b $GOOD_BACKUP -s $BACKUP_FILE -i $GOOD_PACKAGE_MANAGER xyz"
TEST_OUTPUT=`$CL`
if [  $? -ne 0 ];
then
    echo "$TEST_NAME: Failure";
    echo "*** Debug info below ***";
    echo "command line: $CL";
    echo "output:";
    echo "------- output start -------";
    echo "$TEST_OUTPUT"
    echo "-------- output end --------";
else
    echo "$TEST_NAME: Success";
fi

# Fifth test, try a normal upgrade from /tmp
TEST_NAME="Normal upgrade process"
create_temporary_folder
CL="$CF_UPGRADE -b $GOOD_BACKUP -s $BACKUP_FILE -i $DUMB_PACKAGE_MANAGER $TEST_FOLDER"
TEST_OUTPUT=`$CL`
if [  $? -ne 0 ];
then
    echo "$TEST_NAME: Failure";
    echo "*** Debug info below ***";
    echo "command line: $CL";
    echo "output:";
    echo "------- output start -------";
    echo "$TEST_OUTPUT"
    echo "-------- output end --------";
else
    if [ ! -f $TEST_FOLDER/cf-upgrade-done ];
    then
        echo "$TEST_NAME: Verification Failure";
        echo "*** Debug info below ***";
        echo "command line: $CL";
        echo "output:";
        echo "------- output start -------";
        echo "$TEST_OUTPUT"
        echo "-------- output end --------";
    else
        echo "$TEST_NAME: Success";
    fi
fi
