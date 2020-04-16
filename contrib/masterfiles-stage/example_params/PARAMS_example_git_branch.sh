#!/bin/bash
ROOT="/opt/cfengine/masterfiles_staging"
GIT_URL="https://gitlab.com/nickanderson/masterfiles-vcs"
GIT_REFSPEC="3.15.x"
GIT_USERNAME=""
GIT_PASSWORD=""
GIT_WORKING_BRANCH="CF_WORKING_BRANCH"
PKEY=""
SCRIPT_DIR="/var/cfengine/httpd/htdocs/api/dc-scripts"
VCS_TYPE="GIT"

export PATH="/var/cfengine/bin:${PATH}"
export PKEY
