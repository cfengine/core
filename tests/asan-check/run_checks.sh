#!/bin/bash

set -x

function check_with_asan() {
  local n_procs use_procs
  n_procs="$(getconf _NPROCESSORS_ONLN)"
  use_procs=$((n_procs/2))
  if [ "$use_procs" -lt 1 ]; then
    use_procs=1
  fi

  local asan_flags="-fsanitize=address"

  # Assume we are in core directory
  if [ -f ./configure ] ; then
    ./configure -C --enable-debug
  else
    ./autogen.sh -C --enable-debug
  fi
  make -j"${use_procs}" CFLAGS="-Werror -Wall -Wextra -Wno-sign-compare ${asan_flags}" LDFLAGS="${asan_flags}" &&
  make -C tests/unit CFLAGS="${asan_flags}" LDFLAGS="${asan_flags}" check
}

cd "$(dirname "$0")"/../../

if ! check_with_asan; then
  echo "FAIL: ASAN compile/unit check failed";
  exit 1;
fi
