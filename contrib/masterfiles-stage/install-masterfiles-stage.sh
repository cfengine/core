#!/bin/env bash
SRC_DIR="https://raw.githubusercontent.com/cfengine/core/master/contrib/masterfiles-stage"
DC_SCRIPTS_DIR=$(/var/cfengine/bin/cf-promises --file update.cf --show-vars=dc_scripts | awk '/update_def\.dc_scripts/ {print $2}')
DEFAULT_PARAMS_FILE="/opt/cfengine/dc-scripts/params.sh"
mkdir -p ${DC_SCRIPTS_DIR}
mkdir -p $(dirname ${DEFAULT_PARAMS_FILE})
curl --silent ${SRC_DIR}/masterfiles-stage.sh --output ${DC_SCRIPTS_DIR}/masterfiles-stage.sh
curl --silent ${SRC_DIR}/common.sh --output ${DC_SCRIPTS_DIR}/common.sh
curl --silent ${SRC_DIR}/example_params/PARAMS_example_git_branch.sh --output ${DEFAULT_PARAMS_FILE}
chown root:root ${DC_SCRIPTS_DIR}/masterfiles-stage.sh ${DC_SCRIPTS_DIR}/common.sh ${DEFAULT_PARAMS_FILE}
chmod 500 ${DC_SCRIPTS_DIR}/masterfiles-stage.sh
chmod 400 ${DC_SCRIPTS_DIR}/common.sh
chmod 600 ${DEFAULT_PARAMS_FILE}
echo "Now, edit ${DEFAULT_PARAMS_FILE} to conigure your upstream repository."
echo "Then, run '${DC_SCRIPTS_DIR}/masterfiles-stage.sh --DEBUG' to test deployment"
