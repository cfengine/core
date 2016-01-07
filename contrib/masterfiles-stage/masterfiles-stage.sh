#!/usr/bin/env bash
# Authors: Ted Zlatanov <tzz@lifelogs.com>, Nick Anderson <nick@cmdln.org>, Mike Weilgart <mikeweilgart@gmail.com>
mydirname="$(dirname "$0")";

# Load option parsing
# Sets vars: PARAMS, MASTERDIR, verbose_mode, debug_mode
source "${mydirname}/options.sh" || { echo "options.sh couldn't be sourced" >&2 ; exit 17 ; }

# Load common functionality, upstream implementations
source "${mydirname}/common.sh" || { echo "common.sh couldn't be sourced" >&2 ; exit 17 ; }

[ -f "$PARAMS" ] || error_exit "ERROR: Missing '$PARAMS'"

source "$PARAMS"

  # The VCS_TYPE based function calls in the case switch below
  # can count on the following environment variables to be set:
  #
  # MASTERDIR (set in options.sh)
  # PARAMS (set in options.sh)
  # ROOT (set in the PARAMS file)
  # VCS_TYPE (set in the PARAMS file)
  # and, depending on the VCS_TYPE,
  # some of the following vars may also be set in the PARAMS file:
  # GIT_URL, GIT_TAG_OR_COMMIT, GIT_BRANCH, channel_config_file, SVN_URL, SVN_BRANCH

case "${VCS_TYPE}" in
    GIT_POLICY_CHANNELS)
        git_stage_policy_channels
	;;
    GIT_TAG_OR_COMMIT)
        git_masterstage "${GIT_TAG_OR_COMMIT}"
        ;;
    GIT)
        git_masterstage "${GIT_BRANCH}"
        ;;
    SVN)
        svn_branch
        ;;
    *)
        error_exit "Unknown VCS TYPE: '${VCS_TYPE}'."
        ;;
esac
