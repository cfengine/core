#!/bin/bash

set -x

n_procs="$(getconf _NPROCESSORS_ONLN)"

function check_with_gcc() {
  rm -f config.cache
  make clean
  ./configure -C --enable-debug CC=gcc
  make -j -l${n_procs} --keep-going CFLAGS="-Wno-format-truncation -Werror"
}

function check_with_clang() {
  rm -f config.cache
  make clean
  ./configure -C --enable-debug CC=clang
  make -j -l${n_procs} --keep-going CFLAGS="-Werror"
}

function check_with_cppcheck() {
  rm -f config.cache
  ./configure -C --enable-debug

  # cppcheck options:
  #   -I -- include paths
  #   -i -- ignored files/folders
  # Identified issues are printed to stderr
  cppcheck --quiet -j${n_procs} --error-exitcode=1 ./ \
           --suppressions-list=tests/static-check/cppcheck_suppressions.txt \
           -I cf-serverd/ -I libpromises/ -I libcfnet/ -I libntech/libutils/ \
           -i 3rdparty -i .lgtm -i libntech/.lgtm -i tests -i libpromises/cf3lex.c \
           2>&1 1>/dev/null
}

cd "$(dirname $0)"/../../

failure=0
check_with_gcc              || { echo "FAIL: GCC check failed"; failure=1; }
check_with_clang            || { echo "FAIL: Clang check failed"; failure=1; }
check_with_cppcheck         || { echo "FAIL: cppcheck failed"; failure=1; }

exit $failure
