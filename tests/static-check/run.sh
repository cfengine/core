#!/bin/bash

set -e
trap "echo FAILURE" ERR

set -x
BASE_IMG="fedora:33"

function create_image() {
  local c=$(buildah from -q $BASE_IMG)
  buildah run $c -- dnf -q -y install "@C Development Tools and Libraries" clang cppcheck which >/dev/null 2>&1
  buildah run $c -- dnf -q -y install pcre-devel openssl-devel libxml2-devel pam-devel lmdb-devel libacl-devel libyaml-devel curl-devel libvirt-devel >/dev/null 2>&1
  buildah run $c -- dnf clean all >/dev/null 2>&1
  buildah commit $c cfengine-static-checker >/dev/null 2>&1
  echo $c
}

# TODO: check how old the image is and recreate if it's too old
if buildah inspect cfengine-static-checker >/dev/null 2>&1; then
  c=$(buildah from cfengine-static-checker)
else
  c=$(create_image)
fi
trap "buildah rm $c >/dev/null" EXIT

buildah copy $c "$(dirname $0)/../../" /tmp/core/ >/dev/null 2>&1
buildah run $c /tmp/core/tests/static-check/run_checks.sh
