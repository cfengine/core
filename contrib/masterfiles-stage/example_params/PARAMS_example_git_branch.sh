#!/bin/bash
ROOT="/tmp/masterfiles_staging"
GIT_URL="https://gitlab.com/nickanderson/masterfiles_demo_3.7.git"
GIT_REFSPEC="master"
# GIT_REFSPEC can be a git branch, tag, or commit hash.
# It will be staged correctly regardless.
GIT_WORKING_BRANCH="CF_WORKING_BRANCH"
VCS_TYPE="GIT"
export PATH="${PATH}:/var/cfengine/bin"
