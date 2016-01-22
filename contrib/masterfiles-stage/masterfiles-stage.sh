#!/usr/bin/env bash
# Authors: Ted Zlatanov <tzz@lifelogs.com>, Nick Anderson <nick@cmdln.org>, Mike Weilgart <mikeweilgart@gmail.com>

################################################################################
#                                OPTION PARSING                                #
################################################################################
# Original option parsing code generated thanks to
# the bash option parsing library at nk412/optparse on github.
#
# Generated code modified and refined by Mike Weilgart <mikeweilgart@gmail.com>

usage() {
cat << EOF
usage: $0 [OPTIONS]

OPTIONS:

        -d --deploy-dir [directory]
                            The directory where masterfiles should be
                            deployed after successful staging.
                            Default: /var/cfengine/masterfiles

        -p --params-file [filename]
                            The file that contains the parameters necessary
                            to interface with UPSTREAM.
                            Default: /opt/cfengine/dc-scripts/params.sh

        -v --verbose:       Set verbose mode on
        -D --DEBUG:         DEBUG mode (do not log to the logfile)
        -? --help:          Display this usage message

EOF
}

# Contract long options into short options
params=()
for param; do
  case "$param" in
    --deploy-dir)
      params+=("-d")
      ;;
    --params-file)
      params+=("-p")
      ;;
    --verbose)
      params+=("-v")
      ;;
    --DEBUG)
      params+=("-D")
      ;;
    "-?"|--help)
      usage
      exit 0
      ;;
    --*)
      printf '%s\n' "Unrecognized long option: $param"
      usage
      exit 2
      ;;
    *)
      # Assume that any other args should stay as-is
      # (This would include short option flags as well
      # as any other arguments passed to the script.)
      params+=("$param")
      ;;
  esac
done

set -- "${params[@]}"

# Set default variable values
# (Don't change these without updating usage.)
MASTERDIR=/var/cfengine/masterfiles
PARAMS=/opt/cfengine/dc-scripts/params.sh
verbose_mode=false
debug_mode=false

# Process using getopts
while getopts ":d:p:vD" option; do
  case "$option" in
    d)
      MASTERDIR="$OPTARG"
      ;;
    p)
      PARAMS="$OPTARG"
      ;;
    v)
      verbose_mode="true"
      ;;
    D)
      debug_mode="true"
      ;;
    :)
      echo "$0: Option -$OPTARG requires an argument"
      usage
      exit 3
      ;;
    *)
      echo "$0: Invalid option: -$OPTARG"
      usage
      exit 2
      ;;
  esac
done
shift "$((OPTIND-1))"

# Since we don't do anything with any remaining args, we should fail
# noisily if we still have args left after the above shift command.
[ "$#" -gt 0 ] && { echo "Too many arguments." ; usage ; exit 3 ; }

if [ "$debug_mode" == "true" ]; then
  echo "Option Deploy Dir: $MASTERDIR";
  echo "Option Params File: $PARAMS";
fi

# To test this option parsing file by itself, uncomment the below lines.
# printf '%s\n' "MASTERDIR is $MASTERDIR"
# printf '%s\n' "PARAMS is $PARAMS"
# printf '%s\n' "verbose_mode is $verbose_mode"
# printf '%s\n' "debug_mode is $debug_mode"
################################################################################
#                              END OPTION PARSING                              #
################################################################################

# Load common functionality, upstream implementations
mydirname="$(dirname "$0")";
source "${mydirname}/common.sh" || { echo "common.sh couldn't be sourced" >&2 ; exit 17 ; }

[ -f "$PARAMS" ] || error_exit "ERROR: Missing '$PARAMS'"

source "$PARAMS"

  # The VCS_TYPE based function calls in the case switch below
  # can count on the following environment variables to be set:
  #
  # MASTERDIR
  # PARAMS
  # VCS_TYPE (set in the PARAMS file)
  # and additional vars set in PARAMS depending on the VCS_TYPE.

case "${VCS_TYPE}" in
    GIT_POLICY_CHANNELS)
        git_stage_policy_channels
        ;;
    GIT)
        git_masterstage
        ;;
    SVN)
        svn_branch
        ;;
    *)
        error_exit "Unknown VCS TYPE: '${VCS_TYPE}'."
        ;;
esac
