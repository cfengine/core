#!/bin/bash

# Helper script to give $GIT_USERNAME and $GIT_PASSWORD variables
# in reply to username and password requests from git

# Note that this file should be executed by bash, since not all versions of
# /bin/sh understand ${1:0:8}

# If everything is good, git should tell us what it wants.
# First argument is either "Username for https://<url>"
# or "Password for https://<username>@<url>".
# Then it's enough to check just first 8 characters
if [ "q${1:0:8}" == "qUsername" ]
then
    echo $GIT_USERNAME
elif [ "q${1:0:8}" == "qPassword" ]
then
    echo $GIT_PASSWORD
else
    # Normal check failed, fallback to workaround:
    # On first invocation, reply with username;
    # On second invocation, reply with password.
    flag_file=/tmp/cfengine-vcs-askpass
    if [ ! -f $flag_file ]
    then
        # If there is no "flag file", it's a first invocation
        echo $GIT_USERNAME
        touch $flag_file
    else
        # It's a second invocation
        echo $GIT_PASSWORD
        rm $flag_file
    fi
fi
exit 0

