# Nice option parsing with nk412/optparse
# Source the optparse.bash file ---------------------------------------------------
source "${DIRNAME}/lib/optparse/optparse.bash"
# Define options
optparse.define short=d long=deploy-dir desc="The directory where masterfiles should be deployed after successful staging" variable=opt_deploy_dir  default=/var/cfengine/masterfiles
optparse.define short=p long=params-file desc="The file that contains the paramaters necessary to interface with UPSTREAM" variable=opt_params_file default=/opt/cfengine/dc-scripts/params.sh
optparse.define short=v long=verbose desc="Flag to set verbose mode on" variable=verbose_mode value=true default=false
# Because it's currently in active development, we are defaulting debug mode to on
optparse.define short=D long=DEBUG desc="DEBUG mode and do not log to the logfile" variable=debug_mode value=true default=false
# Source the output file ----------------------------------------------------------
source $( optparse.build )

if [ "$debug_mode" == "true" ]; then
  echo "Option Deploy Dir: $opt_deploy_dir";
  echo "Option Params File: $opt_params_file";
fi


