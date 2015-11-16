# This file does nothing else other than redirects to logfile,
# and defining functions.  This allows for code reuse.

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

git_branch_masterstage() {
  # This function is designed to stage masterfiles from a git BRANCH
  #   - Ensure git checkout exists, if it does not remove
  #     obstructions and create new clone
  #   - Ensure the origin is set properly
  #   - Fetch updates
  #   - Stash any local changes
  #   - Checkout the proper branch
  #   - Hard reset to remove any changes
  #   - Validate the staged policy
  #   - If Staged policy has validated and the release IDs are different
  #     synchronize to final location (should we sync anyway to make sure the
  #     distribution point is good?)

  if ! type "git" > /dev/null; then
    error_exit "git not found on path: ${PATH}"
  fi

  # We probably want a different temporary location for each remote repository
  # so that we can avoid conflicts and potential confusion.
  # Example: 
  # ROOT="/opt/cfengine/masterfiles_staging"
  # PARAMS="/var/cfengine/policychannel/production_1.sh"
  # TMP=/opt/cfengine/masterfiles/staging/_tmp_var_cfengine_policychannel_production_1_sh

  TRANSLATED_PARAMS=$(echo $PARAMS | tr [./] _)
  TMP="${ROOT}/_tmp${TRANSLATED_PARAMS}"

  # If we have a git checkout ensure the origin is set properly, update it and
  # make sure all modified, extra, or missing files are reset so that we have a
  # clean checkout

  if [ -d "${TMP}/.git" ]; then
      cd "${TMP}" && git remote set-url origin "${GIT_URL}" && (
          git fetch -q origin || error_exit "Failed: git fetch -q origin"
          git stash -q  || error_exit "Failed: git stash -q"
          git checkout -q "${GIT_BRANCH}" || error_exit "Failed: git checkout -q ${GIT_BRANCH}"
          git reset -q --hard "origin/${GIT_BRANCH}"
      )
  else
      rm -rf "${TMP}"/* "${TMP}"/.??*
      git clone --no-hardlinks "${GIT_URL}" "${TMP}" && cd "${TMP}" && git checkout "${GIT_BRANCH}"
  fi

  if /var/cfengine/bin/cf-promises -T "${TMP}"; then
    if ! /var/cfengine/bin/cf-promises -cf "${TMP}/update.cf"; then
        error_exit "Update policy staged in ${TMP} could not be validated, aborting."
    fi

      # you could abort here if DIFFLINES is over 100, for instance (too many changes)
      #DIFFLINES=$(/usr/bin/diff -r  -x .git -x cf_promises_validated -x cf_promises_release_id "${TMP}" "${MASTERDIR}" |/usr/bin/wc -l)

      # roll out the release if the release IDs are different
      # ALWALSY SYNC THEM but what is the negative side effect? POTENTIALY CLIENTS ALWWAYS UPDATE?
      # BUT CPV only triggered on change, so maybe thats what we want.
      #if /usr/bin/diff -q "${TMP}/cf_promises_release_id" "${MASTERDIR}/cf_promises_release_id" ; then
      #    echo "No release needs to be made, the release IDs are the same."
      #    touch "${TMP}"
      #else
          /bin/mkdir -p "${MASTERDIR}" || error_exit "Failed: Creating '${MASTERDIR}'"
          cd "${TMP}" && (
          chown -R root:root "${TMP}" && \
          rsync -rltDE -c --delete-after --chmod=u+rwX,go-rwx "${TMP}/" "${MASTERDIR}/" && echo "Successfully staged a policy release on $(date)"
      )
      #fi
  else
    error_exit "The staged policies in ${TMP} could not be validated, aborting."
  fi
}

git_tag_or_commit_masterstage() {
  # This function is designed to stage masterfiles from a git TAG or COMMIT
  #   - Ensure git checkout exists, if it does not remove
  #     obstructions and create new clone
  #   - Ensure the origin is set properly
  #   - Fetch updates
  #   - Stash any local changes
  #   - Checkout the proper branch
  #   - Hard reset to remove any changes
  #   - Validate the staged policy
  #   - If Staged policy has validated and the release IDs are different
  #     synchronize to final location (should we sync anyway to make sure the
  #     distribution point is good?)

  if ! type "git" > /dev/null; then
    error_exit "git not found on path: ${PATH}"
  fi

  # We probably want a different temporary location for each remote repository
  # so that we can avoid conflicts and potential confusion.
  # Example: 
  # ROOT="/opt/cfengine/masterfiles_staging"
  # PARAMS="/var/cfengine/policychannel/production_1.sh"
  # TMP=/opt/cfengine/masterfiles/staging/_tmp_var_cfengine_policychannel_production_1_sh

  TRANSLATED_PARAMS=$(echo $PARAMS | tr [./] _)
  TMP="${ROOT}/_tmp${TRANSLATED_PARAMS}"

  # If we have a git checkout ensure the origin is set properly, update it and
  # make sure all modified, extra, or missing files are reset so that we have a
  # clean checkout
  if [ -d "${TMP}/.git" ]; then
      cd "${TMP}" && git remote set-url origin "${GIT_URL}" && (
          git fetch -q origin || error_exit "Failed: git fetch -q origin"
          git stash -q  || error_exit "Failed: git stash -q"
          git checkout -q "${GIT_TAG_OR_COMMIT}" || error_exit "Failed: git checkout -q ${GIT_TAG_OR_COMMIT}"
          git pull --rebase origin "${GIT_TAG_OR_COMMIT}" || error_exit "Failed: git pull --rebase origin ${GIT_TAG_OR_COMMIT}"
          git reset -q --hard "${GIT_TAG_OR_COMMIT}" || error_exit "Failed: git reset -q --hard ${GIT_TAG_OR_COMMIT}"
          git clean -f || error_exit "Failed: git clean -f"
          git clean -fd || error_exit "Failed: git clean -fd"
      ) || error_exit "Failed to stage '${GIT_TAG_OR_COMMIT}' from '${GIT_URL}'" 

  # If we don't have a git clone wipe the directory and create a new clone and
  # ensure the proper branch/tag is checked out.
  else
      rm -rf "${TMP}"/* "${TMP}"/.??*
      git clone --no-hardlinks "${GIT_URL}" "${TMP}" && cd "${TMP}" && git checkout "${GIT_TAG_OR_COMMIT}" || error_exit "Failed to stage '${GIT_TAG_OR_COMMIT}' from '${GIT_URL}'"
  fi

  if /var/cfengine/bin/cf-promises -T "${TMP}"; then
    if ! /var/cfengine/bin/cf-promises -cf "${TMP}/update.cf"; then
        error_exit "Update policy staged in ${TMP} could not be validated, aborting."
    fi
      # roll out the release if the release IDs are different
#      if /usr/bin/diff -q "${TMP}/cf_promises_release_id" "${MASTERDIR}/cf_promises_release_id" ; then
#          #echo "No release needs to be made, the release IDs are the same."
#          touch "${TMP}"
#      else
          cd "${TMP}" && (
          chown -R root:root "${TMP}" && rsync -rltDE -c --delete-after --chmod=u+rwX,go-rwx "${TMP}/" "${MASTERDIR}/" && echo "Successfully staged a policy release of '${GIT_TAG_OR_COMMIT}' from '${GIT_URL}' to '${MASTERDIR}' on $(date)"
      )
#      fi
  else
    error_exit "The staged policies in ${TMP} could not be validated, aborting."
  fi
}

svn_branch() {
# Contributed by John Farrar

    if ! type "svn" >/dev/null ; then
	error_exit "svn not found on path: ${PATH}"
    fi

    CHECKSUM_FILE="svn_promise_checksums"

    # We probably want a different temporary location for each remote repository
    # so that we can avoid conflicts and potential confusion.
    # Example: 
    # ROOT="/opt/cfengine/masterfiles_staging"
    # PARAMS="/var/cfengine/policychannel/production_1.sh"
    # TMP=/opt/cfengine/masterfiles/staging/_tmp_var_cfengine_policychannel_production_1_sh

    TRANSLATED_PARAMS=$(echo $PARAMS | tr [./] _)
    TMP="${ROOT}/_tmp${TRANSLATED_PARAMS}"

    # If we already have a checkout, update it, else make a new checkout.
    if [ -d "${TMP}/.svn" ] ; then
	svn update --quiet ${TMP}
    else
	rm -rf "${TMP}"
	svn checkout --quiet "${SVN_URL}"/"${SVN_BRANCH}"/inputs "${TMP}"
    fi

    rm -f "${TMP}/cf_promises_release_id"

    if /var/cfengine/bin/cf-promises -T "${TMP}"; then
	md5sum `find ${TMP} -type f -name \*.cf` >"${TMP}/${CHECKSUM_FILE}"
	if /usr/bin/diff -q "${TMP}/${CHECKSUM_FILE}" "${MASTERDIR}/${CHECKSUM_FILE}" ; then
            # echo "No release needs to be made, the checksum files are the same"
            touch "${TMP}"
	else
            cd "${TMP}" && (
		chown -R root:root "${TMP}" && \
                rsync -CrltDE -c --delete-after --chmod=u+rwX,go-rwx "${TMP}/" "${MASTERDIR}/" && \
                rm -rf ${TMP}/.svn && \
                echo "Successfully staged a policy release on $(date)"
	    )
	fi
    else
       error_exit "The staged policies in ${TMP} could not be validated, aborting."
    fi
}
