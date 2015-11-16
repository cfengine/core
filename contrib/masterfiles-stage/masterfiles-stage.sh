#!/bin/bash
# Authors: Ted Zlatanov <tzz@lifelogs.com>, Nick Anderson <nick@cmdln.org>

DIRNAME=$(dirname $0);

# Check that the required arguments are provided
if [ $# -ne 2 ]; then
    echo "Usage: $0 MASTERDIR PARAMS" 1>&2
    echo "Example: $0 /var/cfengine/masterfiles /opt/cfengine/dc-scripts/params.sh" 1>&2
    exit 1
fi

source "${DIRNAME}/common.sh"

MASTERDIR=$1
PARAMS=$2

# If PARAMS is unset, then we look to the default location
PARAMS=${PARAMS:-/opt/cfengine/dc-scripts/params.sh}
if [ ! -f $PARAMS ]; then
  error_exit "common.sh" "ERROR: Missing '$PARAMS'"
fi

source $PARAMS

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
