#!/bin/bash
VCS_TYPE="GIT_POLICY_CHANNELS"
GIT_URL="git@mygitserver.net:joeblow/my_policy_repo.git"
dir_to_hold_mirror="/opt/cfengine"

# ROOT="/opt/cfengine/masterfiles_staging"
# ROOT is not used for this VCS_TYPE but must be present
# for integration with Design Center.

chan_deploy="/var/cfengine/policy_channels"
# chan_deploy is not used outside this file
# and is just for convenience in defining the channel_config array.

channel_config=()
channel_config+=( "$chan_deploy/channel_1"    "my_branch_name" )
channel_config+=( "$chan_deploy/channel_2"    "my_tag_name" )
channel_config+=( "/var/cfengine/masterfiles" "362e11b705" )
# Note that channel_config must have an even number of elements
# and that absolute pathnames must be used.
# The format is, after the initial empty array value is set:
# channel_config+=( "/absolute/path/to/deploy/to"  "git_reference_specifier" )
# The git refspec can be a tag, a commit hash, or a branch name
# and will work correctly regardless.

PKEY="/opt/cfengine/userworkdir/admin/.ssh/id_rsa.pvt"
SCRIPT_DIR="/var/cfengine/httpd/htdocs/api/dc-scripts"
export PATH="${PATH}:/var/cfengine/bin"
export PKEY="${PKEY}"
export GIT_SSH="${SCRIPT_DIR}/ssh-wrapper.sh"
