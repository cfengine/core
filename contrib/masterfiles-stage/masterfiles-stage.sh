#!/usr/bin/env bash
# Authors: Ted Zlatanov <tzz@lifelogs.com>, Nick Anderson <nick@cmdln.org>, Mike Weilgart
DIRNAME=$(dirname $0);

# Load option parsing
source "${DIRNAME}/options.sh"

# Load common functionaly, upstream implementations
source "${DIRNAME}/common.sh"

MASTERDIR=$opt_deploy_dir
PARAMS=$opt_params_file

# If PARAMS is unset, then we look to the default location
#PARAMS=${PARAMS:-/opt/cfengine/dc-scripts/params.sh}
if [ ! -f "$PARAMS" ]; then
  error_exit "ERROR: Missing '$PARAMS'"
fi

source $PARAMS

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
        error_exit $(basename $0) "Unknown VCS TYPE: '${VCS_TYPE}'."
        ;;
esac
