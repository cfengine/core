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

set_staging_dir_from_params() {
  # We probably want a different temporary location for each remote repository
  # so that we can avoid conflicts and potential confusion.
  # Example:
  # ROOT="/opt/cfengine/masterfiles_staging"
  # PARAMS="/var/cfengine/policychannel/production_1.sh"
  # STAGING_DIR=/opt/cfengine/masterfiles/staging/_tmp_var_cfengine_policychannel_production_1_sh

  STAGING_DIR="${ROOT}/_tmp$(echo "$PARAMS" | tr [./] _)"
}

check_git_installed() {
  git --version >/dev/null 2>&1 || error_exit "git not found on path: '${PATH}'"
}

git_setup_local_mirrored_repo() {
  # This function sets the variable local_mirrored_repo to a directory path
  # based on the value of GIT_URL and ROOT, and if that directory doesn't exist,
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

  local_mirrored_repo="${ROOT}/$(printf '%s' "${GIT_URL}" | sed 's/[^A-Za-z0-9._-]/_/g')"

  if [ -d "${local_mirrored_repo}" ] ; then
    git --git-dir="${local_mirrored_repo}" fetch && return 0

    # If execution arrives here, the local_mirrored_repo exists but is messed up somehow
    # (or there is network trouble).  Easiest is to wipe it and start fresh.
    rm -rf "${local_mirrored_repo}"
  fi
  git clone --mirror "${GIT_URL}" "${local_mirrored_repo}" ||
    error_exit "Failed: git clone --mirror '${GIT_URL}' '${local_mirrored_repo}'"
}

git_stage_refspec() {
  # This function depends on git_setup_local_mirrored_repo
  # having been run, such that the variable local_mirrored_repo
  # contains the (local) path to a bare git repository.
  #
  # (A mirror repository is a special case of a bare repository;
  # either will work for this function but a bare non-mirror
  # repository will have edge cases that are mishandled.)
  #
  # This function accepts a single argument: refspec,
  # which should be a git tagname, branch, or commit hash.
  #
  # This function stages the refspec to the STAGING_DIR
  # from local_mirrored_repo

  mkdir -p "${STAGING_DIR}" || error_exit "Failed: mkdir -p '$STAGING_DIR'"
  git --git-dir="${local_mirrored_repo}" --work-tree="${STAGING_DIR}" checkout -q -f "$1" ||
    error_exit "Failed to checkout '$2' from '${local_mirrored_repo}'"
  git --git-dir="${local_mirrored_repo}" --work-tree="${STAGING_DIR}" clean -q -dff ||
    error_exit "Failed: git --git-dir='${local_mirrored_repo}' --work-tree='${STAGING_DIR}' clean -q -dff"
}

validate_staged_policy() {
  # If you use this function, ensure you have set STAGING_DIR.
  # Also see function "avoid_triggering_unneeded_policy_updates"
  /var/cfengine/bin/cf-promises -T "${STAGING_DIR}" &&
  /var/cfengine/bin/cf-promises -cf "${STAGING_DIR}/update.cf" ||
  error_exit "Update policy staged in ${STAGING_DIR} could not be validated, aborting."
}

avoid_triggering_unneeded_policy_updates() {
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

  if [ -f "${MASTERDIR}/cf_promises_validated" ] &&
     /usr/bin/cmp -s "${STAGING_DIR}/cf_promises_release_id" \
                       "${MASTERDIR}/cf_promises_release_id"
  then
    cp -a "${MASTERDIR}/cf_promises_validated" "${STAGING_DIR}/" ||
      error_exit "Unable to copy existing file ${MASTERDIR}/cf_promises_validated to ${STAGING_DIR}/"
  fi
}

rollout_staged_policy_to_masterdir() {
  # Put STAGING_DIR to MASTERDIR with a mv command rather than
  # an rsync command so it is one atomic operation.
  #
  # If MASTERDIR was already there, we move it out of the way
  # first.  Then we need to do something with it -- so we put
  # it in the old STAGING_DIR location and let the next round
  # of staging, during the next run of the script, handle any
  # leftover cruft.
  # (git_stage_refspec includes "checkout -f" and "clean",
  # which would handle this cruft; any other staging functions
  # that are built must be designed to handle cruft also.)

  chown -R root:root "${STAGING_DIR}" || error_exit "Unable to chown '${STAGING_DIR}'"
  chmod -R go-rwx    "${STAGING_DIR}" || error_exit "Unable to chmod '${STAGING_DIR}'"

  if [ -d "${MASTERDIR}" ] ; then
    # Put tmpdir in MASTERDIR's parent dir to avoid crossing filesystem boundaries
    # (which could be a danger if we used /tmp).
    third_dir="$(mktemp -d --tmpdir="$(dirname "${MASTERDIR}")" )"

    mv "${MASTERDIR}" "${third_dir}/momentary"  || error_exit "Can't mv ${MASTERDIR} to ${third_dir}"
    mv "${STAGING_DIR}" "${MASTERDIR}"          || error_exit "Can't mv ${STAGING_DIR} to ${MASTERDIR}"
    mv "${third_dir}/momentary" "${STAGING_DIR}"   # We don't care if this fails;
    rm -rf "${third_dir}"                          # we're going to remove third_dir anyways.
  else
    mv "${STAGING_DIR}" "${MASTERDIR}"          || error_exit "Can't mv ${STAGING_DIR} to ${MASTERDIR}"
  fi
}

######################################################
##           VCS_TYPE-based main functions           #
######################################################

git_stage_policy_channels() {
  # Created by Mike Weilgart
  #
  # This "VCS_TYPE-based" function is called from masterfiles-stage.sh.
  #
  # This function stages multiple policy channels each to its masterdir,
  # all based on the simple two-field channel_config_file.
  # (See that file for further documentation of its format.)
  #
  # The GIT_URL is set in the params.sh file;
  # ROOT (the dir in which to put staging dirs) is also set in params.sh
  #
  # The value of MASTERDIR that is assigned in masterfiles-stage.sh
  # is ****IGNORED**** by this function, since there is a separate
  # MASTERDIR for each separate policy channel.

  # channel_config_file should be set in the PARAMS file.
  # Example value:
  # channel_config_file="/var/cfengine/policy_channels/channel_to_source.txt"
  [ -f "${channel_config_file}" ] || error_exit "${channel_config_file} not found"

  check_git_installed
  git_setup_local_mirrored_repo

  # sed removes comments, including trailing comments, and skips empty/whitespace only lines.
  sed -e 's/#.*//; /^[[:space:]]*$/d' "${channel_config_file}" |
    while read channel_name refspec ; do

      STAGING_DIR="${ROOT}/${channel_name}"
      MASTERDIR="/var/cfengine/policy_channels/masterfiles_dirs/${channel_name}"

      git_stage_refspec "$refspec"
      validate_staged_policy
      avoid_triggering_unneeded_policy_updates
      rollout_staged_policy_to_masterdir
      echo "Successfully deployed a policy release of '${refspec}' from '${GIT_URL}' to '${MASTERDIR}' on $(date)"

    done
}

git_masterstage() {
  set_staging_dir_from_params
  check_git_installed
  git_setup_local_mirrored_repo
  git_stage_refspec "$1"
  validate_staged_policy
  avoid_triggering_unneeded_policy_updates
  rollout_staged_policy_to_masterdir

  echo "Successfully deployed '$1' from '${GIT_URL}' to '${MASTERDIR}' on $(date)"
}

svn_branch() {
# Contributed by John Farrar

    set_staging_dir_from_params

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
