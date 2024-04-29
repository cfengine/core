#!/bin/bash
# Note that this container build requires about 700MB minimum RAM for dnf to operate
# use debian-12+ or rhel-8+, debian-11 buildah seems to fail setting up networking/dns for the container so dnf doesn't work (CFE-4295)

set -eE # include E so that create_image() failures bubble up to the surface
trap "echo FAILURE" ERR

if [ -z "$STATIC_CHECKS_FEDORA_VERSION" ]; then
  default_f_ver="40"
  echo "No Fedora version for static checks specified, using the default (Fedora $default_f_ver)"
  BASE_IMG="fedora:$default_f_ver"
  STATIC_CHECKS_FEDORA_VERSION="$default_f_ver"
else
  BASE_IMG="fedora:$STATIC_CHECKS_FEDORA_VERSION"
fi

function create_image() {
  local c=$(buildah from -q $BASE_IMG)
  buildah run $c -- dnf -q -y install "@C Development Tools and Libraries" clang cppcheck which >/dev/null 2>&1
  buildah run $c -- dnf -q -y install pcre-devel pcre2-devel openssl-devel libxml2-devel pam-devel lmdb-devel libacl-devel libyaml-devel curl-devel libvirt-devel >/dev/null 2>&1
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
