#!/bin/bash
ROOT="/tmp/masterfiles_staging"
GIT_URL="https://gitlab.com/nickanderson/masterfiles_demo_3.7.git"
GIT_BRANCH="master"
# GIT_BRANCH can also be a git tag, or a git commit hash.
# It will be staged correctly regardless.
GIT_WORKING_BRANCH="CF_WORKING_BRANCH"
VCS_TYPE="GIT"
export PATH="${PATH}:/var/cfengine/bin"
