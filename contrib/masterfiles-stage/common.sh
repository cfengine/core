# This file does nothing else other than redirects to logfile,
# and defining functions.  This allows for code reuse.
if [ ! "$debug_mode" == "true" ]; then
  # close STDERR and STDOUT
  exec 1<&-
  exec 2<&-

  # open STDOUT
  exec 1>>/var/cfengine/outputs/dc-scripts.log

  # redirect STDERR to STDOUT
  exec 2>&1
fi

error_exit() {
    # Display error message and exit
    echo "${0}: ${1:-"Unknown Error"}" 1>&2
    exit 1
}

check_git_installed() {
  git --version >/dev/null 2>&1 || error_exit "git not found on path: '${PATH}'"
}

git_setup_local_mirrored_repo() {
# Contributed by Mike Weilgart

  # Depends on $GIT_URL
  # Sets $local_mirrored_repo
  # Accepts one arg:
  # $1 - absolute path in which to place the mirror.

  # This function sets the variable local_mirrored_repo to a directory path
  # based on the value of GIT_URL, and if that directory doesn't exist,
  # creates it as a mirrored clone of the repo at GIT_URL.  If it does exist,
  # update it with git fetch.
  #
  # This code could be improved if there is an inexpensive way to check that
  # a local bare repository is in fact a *mirrored* repository of a specified
  # GIT_URL, but for now if the local_mirrored_repo is in fact a bare git
  # repo (guaranteed by the success of the "git fetch" command) then we just
  # assume it is a *mirrored* repository.
  #
  # Since the pathname is directly based on GIT_URL, there is no chance
  # of *accidental* name collision.

  # Check first char of the argument to ensure it's an absolute path
  [ "${1:0:1}" = / ] ||
    error_exit "Improper path passed to git_setup_local_mirrored_repo"
  local_mirrored_repo="${1}/$(printf '%s' "${GIT_URL}" | sed 's/[^A-Za-z0-9._-]/_/g')"
  ########### Example value:
  # GIT_URL="git@mygitserver.net:joeblow/my_policy_repo.git"
  # parameter passed in: /opt/cfengine
  # value of local_mirrored_repo that is set:
  # /opt/cfengine/git_mygitserver.net_joeblow_my_policy_repo.git

  if [ -d "${local_mirrored_repo}" ] ; then
    git --git-dir="${local_mirrored_repo}" fetch && return 0

    # If execution arrives here, the local_mirrored_repo exists but is messed up somehow
    # (or there is network trouble).  Easiest is to wipe it and start fresh.
    rm -rf "${local_mirrored_repo}"
  fi
  git clone --mirror "${GIT_URL}" "${local_mirrored_repo}" ||
    error_exit "Failed: git clone --mirror '${GIT_URL}' '${local_mirrored_repo}'"
}

git_deploy_refspec() {
# Contributed by Mike Weilgart

  # Depends on $local_mirrored_repo
  # Accepts two args:
  # $1 - dir to deploy to
  # $2 - refspec to deploy - a git tagname, branch, or commit hash.

  # This function
  # 1. creates an empty temp dir,
  # 2. checks out the refspec into the empty temp dir,
  #    (including populating .git/HEAD in the temp dir),
  # 3. sets appropriate permissions on the policy set,
  # 4. validates the policy set using cf-promises,
  # 5. moves the temp dir policy set into the given deploy dir,
  #    avoiding triggering policy updates unnecessarily
  #    by comparing the cf_promises_validated flag file.
  #    (See long comment at end of function def.)

  # Ensure absolute pathname is given
  [ "${1:0:1}" = / ] ||
    error_exit "You must specify absolute pathnames in channel_config: '$1'"
  mkdir -p "$(dirname "$1")" || error_exit "Failed to mkdir -p dirname $1"
    # We don't mkdir $1 directly, just its parent dir if that doesn't exist.

  ########################## 1. CREATE EMPTY TEMP DIR
  # Put staging dir right next to deploy dir to ensure it's on same filesystem
  local temp_stage
  temp_stage="$(mktemp -d --tmpdir="$(dirname "$1")" )"
  trap 'rm -rf "$temp_stage"' EXIT

  ########################## 2. CHECKOUT INTO TEMP DIR
  # The '^0' at the end of the refspec
  # populates HEAD with the SHA of the commit
  # rather than potentially using a git branch name.
  # Also see http://stackoverflow.com/a/13293010/5419599
  # and https://github.com/cfengine/core/pull/2465#issuecomment-173656475
  git --git-dir="${local_mirrored_repo}" --work-tree="${temp_stage}" checkout -q -f "${2}^0" ||
    error_exit "Failed to checkout '$2' from '${local_mirrored_repo}'"
  # Grab HEAD so it can be used to populate cf_promises_release_id
  mkdir -p "${temp_stage}/.git"
  cp "${local_mirrored_repo}/HEAD" "${temp_stage}/.git/"

  ########################## 3. SET PERMISSIONS ON POLICY SET
  chown -R root:root "${temp_stage}" || error_exit "Unable to chown '${temp_stage}'"
  find "${temp_stage}" \( -type f -exec chmod 600 {} + \) -o \
                       \( -type d -exec chmod 700 {} + \)

  ########################## 4. VALIDATE POLICY SET
  /var/cfengine/bin/cf-promises -T "${temp_stage}" &&
  /var/cfengine/bin/cf-promises -cf "${temp_stage}/update.cf" ||
  error_exit "Update policy staged in ${temp_stage} could not be validated, aborting."

  ########################## 5. ROLL OUT POLICY SET FROM TEMP DIR TO DEPLOY DIR
  if ! [ -d "$1" ] ; then
    # deploy dir doesn't exist yet
    mv "${temp_stage}" "$1" || error_exit "Failed to mv $temp_stage to $1."
    trap -- EXIT
  else
    if /usr/bin/cmp -s "${temp_stage}/cf_promises_release_id" \
                                 "${1}/cf_promises_release_id" ; then
      # release id is the same in stage and deploy dir
      # so prevent triggering update on hosts by keeping old "validated" flag file
      cp -a "${1}/cf_promises_validated" "${temp_stage}/"
    fi
    local third_dir
    third_dir="$(mktemp -d --tmpdir="$(dirname "$1")" )"
    trap 'rm -rf "$third_dir"' EXIT
    mv "${1}" "${third_dir}"  || error_exit "Can't mv ${1} to ${third_dir}"
      # If the above command fails we will have an extra temp dir left.  Otherwise not.
    mv "${temp_stage}" "${1}"          || error_exit "Can't mv ${temp_stage} to ${1}"
    rm -rf "${third_dir}"
    trap -- EXIT
  fi

  # Note about triggering policy updates:
  #
  # cf_promises_validated gets updated by any run of cf-promises,
  # but hosts use cf_promises_validated as the flag file to see
  # if they need to update everything else (the full policy set.)
  #
  # cf_promises_release_id is the same for a given policy set
  # unless changes have actually been made to the policy, so it
  # can be used to check if we want to trigger an update.
  #
  # In other words, update is triggered by putting the
  # newly created copy of cf_promises_validated into the MASTERDIR
  # and update is avoided either by:
  #
  # 1. Completely skipping the rollout_staged_policy_to_masterdir
  # function, or
  #
  # 2. Copying the MASTERDIR's copy of cf_promises_validated
  # *back* into the STAGING_DIR *before* performing the rollout,
  # so that after the rollout the MASTERDIR's copy of the flag
  # file is the same as it was before the rollout.
  #
  # This function uses the second approach.  --Mike Weilgart
}

######################################################
##           VCS_TYPE-based main functions           #
######################################################

git_stage_policy_channels() {
# Contributed by Mike Weilgart

  # Depends on ${channel_config[@]} and $dir_to_hold_mirror
  # Calls functions dependent on $GIT_URL
  # (See the example git policy channels params file.)
  #
  # Stages multiple policy channels from a specified GIT_URL,
  # each to the specified path.
  #
  # The paths to stage to as well as the policy sets to stage
  # are both specified in the "channel_config" array in the
  # PARAMS file.
  #
  # The value of MASTERDIR that is assigned in masterfiles-stage.sh
  # is ignored by this function, since there is effectively a separate
  # MASTERDIR for each separate policy channel.
  #
  # Example value for channel_config:
  # (This is a single value, split into three lines for readability.)
  #
  # channel_config=()
  # channel_config+=( "/var/cfengine/masterfiles" "my_git_tag" )
  # channel_config+=( "/var/cfengine/policy_channels/channel_1" "my_git_branch" )

  # Simplest validation check first
  set -- "${channel_config[@]}"
  [ "$#" -gt 1 ] ||
    error_exit "The channel_config array must have at least two elements."

  check_git_installed
  git_setup_local_mirrored_repo "$dir_to_hold_mirror"

  while [ "$#" -gt 1 ] ; do
    # At start of every loop, "$1" contains deploy dir and "$2" is refspec.
    git_deploy_refspec "$1" "$2"
    echo "Successfully deployed a policy release of '${2}' from '${GIT_URL}' to '${1}' on $(date)"
    shift 2
  done

  [ "$#" -eq 1 ] && error_exit "Trailing parameter found, please fix params file: '$1'"
}

git_masterstage() {
  # Depends on $GIT_URL, $ROOT, $MASTERDIR, $GIT_REFSPEC
  check_git_installed
  git_setup_local_mirrored_repo "$( dirname "$ROOT" )"
  git_deploy_refspec "$MASTERDIR" "${GIT_REFSPEC}"
  echo "Successfully deployed '${GIT_REFSPEC}' from '${GIT_URL}' to '${MASTERDIR}' on $(date)"
}

svn_branch() {
# Contributed by John Farrar

    # We probably want a different temporary location for each remote repository
    # so that we can avoid conflicts and potential confusion.
    # Example:
    # ROOT="/opt/cfengine/masterfiles_staging"
    # PARAMS="/var/cfengine/policychannel/production_1.sh"
    # STAGING_DIR=/opt/cfengine/masterfiles/staging/_tmp_var_cfengine_policychannel_production_1_sh

    STAGING_DIR="${ROOT}_tmp$(echo "$PARAMS" | tr [./] _)"

    if ! type "svn" >/dev/null ; then
        error_exit "svn not found on path: ${PATH}"
    fi

    CHECKSUM_FILE="svn_promise_checksums"

    # If we already have a checkout, update it, else make a new checkout.
    if [ -d "${STAGING_DIR}/.svn" ] ; then
        svn update --quiet ${STAGING_DIR}
    else
        rm -rf "${STAGING_DIR}"
        svn checkout --quiet "${SVN_URL}"/"${SVN_BRANCH}"/inputs "${STAGING_DIR}"
    fi

    rm -f "${STAGING_DIR}/cf_promises_release_id"

    if /var/cfengine/bin/cf-promises -T "${STAGING_DIR}"; then
        md5sum `find ${STAGING_DIR} -type f -name \*.cf` >"${STAGING_DIR}/${CHECKSUM_FILE}"
        if /usr/bin/diff -q "${STAGING_DIR}/${CHECKSUM_FILE}" "${MASTERDIR}/${CHECKSUM_FILE}" ; then
            # echo "No release needs to be made, the checksum files are the same"
            touch "${STAGING_DIR}"
        else
            cd "${STAGING_DIR}" && (
                chown -R root:root "${STAGING_DIR}" && \
                rsync -CrltDE -c --delete-after --chmod=u+rwX,go-rwx "${STAGING_DIR}/" "${MASTERDIR}/" && \
                rm -rf ${STAGING_DIR}/.svn && \
                echo "Successfully staged a policy release on $(date)"
            )
        fi
    else
       error_exit "The staged policies in ${STAGING_DIR} could not be validated, aborting."
    fi
}
