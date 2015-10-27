# close STDERR and STDOUT
exec 1<&-
exec 2<&-

# open STDOUT
exec 1>>/var/cfengine/outputs/dc-scripts.log

# redirect STDERR to STDOUT
exec 2>&1

function error_exit {
    # Display error message and exit
    echo "${0}: ${1:-"Unknown Error"}" 1>&2
    exit 1
}

# If PARAMS is unset, then we look to the default location
PARAMS=${PARAMS:-/opt/cfengine/dc-scripts/params.sh}
if [ ! -f $PARAMS ]; then
  error_exit "common.sh" "ERROR: Missing '$PARAMS'"
fi

source $PARAMS
