#!/usr/bin/env bash
# Authors: Ted Zlatanov <tzz@lifelogs.com>, Nick Anderson <nick@cmdln.org>, Mike Weilgart <mikeweilgart@gmail.com>
mydirname="$(dirname "$0")";

# Load option parsing
source "${mydirname}/options.sh"

# Load common functionaly, upstream implementations
source "${mydirname}/common.sh"

MASTERDIR="$opt_deploy_dir"
PARAMS="$opt_params_file"

# If PARAMS is unset, then we look to the default location
#PARAMS=${PARAMS:-/opt/cfengine/dc-scripts/params.sh}
[ -f "$PARAMS" ] || error_exit "ERROR: Missing '$PARAMS'"

source "$PARAMS"

  # We probably want a different temporary location for each remote repository
  # so that we can avoid conflicts and potential confusion.
  # Example: 
  # ROOT="/opt/cfengine/masterfiles_staging"
  # PARAMS="/var/cfengine/policychannel/production_1.sh"
  # TMP=/opt/cfengine/masterfiles/staging/_tmp_var_cfengine_policychannel_production_1_sh

  TRANSLATED_PARAMS=$(echo $PARAMS | tr [./] _)
  STAGING_DIR="${ROOT}/_tmp${TRANSLATED_PARAMS}"

case "${VCS_TYPE}" in
    GIT_TAG_OR_COMMIT)
        git_tag_or_commit_masterstage
        ;;
    GIT)
        git_branch_masterstage
        ;;
    SVN)
        svn_branch
        ;;
    *)
        error_exit "Unknown VCS TYPE: '${VCS_TYPE}'."
        ;;
esac
