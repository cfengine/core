#!/usr/bin/env bash
set -ex

function check_errors()
{
  log=$1
  if grep error: "$log" >/dev/null; then
    echo "FAIL: errors in $log"
    grep error: "$log"
    exit 1
  fi
}

rm -f bootstrap.log
rm -f update.log
rm -f promise.log

build_image=cfengine-core-alpine-build-image
installed_image=cfengine-core-alpine-installed-image # cfengine built and installed

# TODO: separate build_image from installed_image
# e.g. install alpine-sdk/etc for build_image but then start fresh from alpine and installed bits in /var/cfengine?
if ! buildah images "$build_image"; then
  c=$(buildah from alpine)
  buildah run "$c" apk update
  buildah run "$c" apk add alpine-sdk lmdb-dev openssl-dev bison flex-dev acl-dev pcre2-dev autoconf automake libtool git python3 gdb librsync-dev
  buildah commit $c "$build_image"
fi


if ! buildah images "$installed_image"; then
  c=$(buildah from "$build_image")
  buildah run --volume $(realpath ../../):/core --workingdir /core "$c" ./autogen.sh --without-pam
  buildah run --volume $(realpath ../../):/core --workingdir /core "$c" make install
  buildah run --volume $(realpath ../../../masterfiles):/mpf --workingdir /mpf "$c" ./autogen.sh
  buildah run --volume $(realpath ../../../masterfiles):/mpf --workingdir /mpf "$c" make install
  buildah commit "$c" "$installed_image"
fi

# run from a blank alpine container to save space and what not
c=$(buildah from alpine)

# python3 and procps are needed for running
buildah run "$c" apk update
buildah run "$c" apk add python3 procps lmdb-dev openssl-dev flex-dev acl-dev pcre2-dev librsync-dev
buildah copy --from "$installed_image" "$c" /var/cfengine /var/cfengine


# generate hostkey if needed (repeated runs should skip)
if ! buildah run "$c" test -f /var/cfengine/ppkeys/localhost.pub; then
  buildah run "$c" /var/cfengine/bin/cf-key
fi
# bootstrap
  buildah run "$c" apk add strace # debug bootstrap hanging on waiting for background process, track fork()
buildah run "$c" sh -c 'strace /var/cfengine/bin/cf-agent --timestamp --debug --bootstrap $(hostname -i)' | tee bootstrap.log
check_errors bootstrap.log

# run update
buildah run "$c" sh -c '/var/cfengine/bin/cf-agent -KIf update.cf | tee update.log
check_errors update.log

# run promises
buildah run "$c" sh -c '/var/cfengine/bin/cf-agent -KI | tee promise.log
check_errors promise.log
buildah run "$c" sh -c '/var/cfengine/bin/cf-agent -KI | tee -a promise.log
check_errors promise.log
buildah run "$c" sh -c '/var/cfengine/bin/cf-agent -KI | tee -a promise.log
check_errors promise.log

# TODO: remove run image, keep build and installed images

# TODO maybe commit the installed/bootstrapped image for other uses
