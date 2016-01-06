#!/usr/bin/env bash
# Authors: Ted Zlatanov <tzz@lifelogs.com>, Nick Anderson <nick@cmdln.org>, Mike Weilgart <mikeweilgart@gmail.com>
mydirname="$(dirname "$0")";

# Load option parsing
source "${mydirname}/options.sh"

# Load common functionality, upstream implementations
source "${mydirname}/common.sh"

MASTERDIR="$opt_deploy_dir"
PARAMS="$opt_params_file"

[ -f "$PARAMS" ] || error_exit "ERROR: Missing '$PARAMS'"

source "$PARAMS"

  # The VCS_TYPE based function calls in the case switch below
  # can count on the following environment variables to be set:
  #
  # MASTERDIR (set in this script based on options.sh)
  # PARAMS (set in this script based on options.sh)
  # ROOT (set in the PARAMS file)
  # GIT_URL (set in the PARAMS file)
  # and, of course,
  # VCS_TYPE (set in the PARAMS file)

case "${VCS_TYPE}" in
    GIT_MIRROR_POLICY_CHANNELS)
        git_stage_policy_channels_from_mirror
	;;
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
