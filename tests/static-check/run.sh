#!/bin/bash

set -e
trap "echo FAILURE" ERR

if [ -z "$STATIC_CHECKS_FEDORA_VERSION" ]; then
  # Fedora 33 is the default because it works with old container runtimes
  # (see https://bugzilla.redhat.com/show_bug.cgi?id=1962080 and linked bugs for
  # details)
  echo "No Fedora version for static checks specified, using the default (Fedora 33)"
  BASE_IMG="fedora:33"
  STATIC_CHECKS_FEDORA_VERSION="33"
else
  BASE_IMG="fedora:$STATIC_CHECKS_FEDORA_VERSION"
fi

function create_image() {
  local c=$(buildah from -q $BASE_IMG)
  buildah run $c -- dnf -q -y install "@C Development Tools and Libraries" clang cppcheck which >/dev/null 2>&1
  buildah run $c -- dnf -q -y install pcre-devel openssl-devel libxml2-devel pam-devel lmdb-devel libacl-devel libyaml-devel curl-devel libvirt-devel >/dev/null 2>&1
  buildah run $c -- dnf clean all >/dev/null 2>&1
  buildah commit $c cfengine-static-checker-f$STATIC_CHECKS_FEDORA_VERSION >/dev/null 2>&1
  echo $c
}

set -x

# TODO: check how old the image is and recreate if it's too old
if buildah inspect cfengine-static-checker-f$STATIC_CHECKS_FEDORA_VERSION >/dev/null 2>&1; then
  c=$(buildah from cfengine-static-checker-f$STATIC_CHECKS_FEDORA_VERSION)
else
  c=$(create_image)
fi
trap "buildah rm $c >/dev/null" EXIT

buildah copy $c "$(dirname $0)/../../" /tmp/core/ >/dev/null 2>&1
buildah run $c /tmp/core/tests/static-check/run_checks.sh
