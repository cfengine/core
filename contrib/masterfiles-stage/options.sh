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
opt_deploy_dir=/var/cfengine/masterfiles
opt_params_file=/opt/cfengine/dc-scripts/params.sh
verbose_mode=false
debug_mode=false

# Process using getopts
while getopts ":d:p:vD" option; do
  case "$option" in
    d)
      opt_deploy_dir="$OPTARG"
      ;;
    p)
      opt_params_file="$OPTARG"
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
  echo "Option Deploy Dir: $opt_deploy_dir";
  echo "Option Params File: $opt_params_file";
fi

# To test this option parsing file by itself, uncomment the below lines.
# printf '%s\n' "opt_deploy_dir is $opt_deploy_dir"
# printf '%s\n' "opt_params_file is $opt_params_file"
# printf '%s\n' "verbose_mode is $verbose_mode"
# printf '%s\n' "debug_mode is $debug_mode"
