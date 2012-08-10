#!/bin/sh
set -e

usage()
{
  echo
  echo "Usage: release.sh [--branch=<branch-name>] <next version>"
  echo
  echo "By default new branch is not created."
  echo
}

opts()
{
  OPTS=$(getopt -o b: --long branch: -n release.sh -- "$@")
  eval set -- "$OPTS"

  if [ $? != 0 ]; then
    usage
    exit 1
  fi

  while true; do
    case "$1" in
      -b|--branch)
        BRANCH="$2"
        shift 2;;
      --)
        shift
        break;;
      *)
        echo "Internal error!"
        exit 1;;
    esac
  done

  if [ $# -ne 1 ]; then
    usage
    exit 1
  fi

  NEXT_VERSION="$1"
}

detect_current_branch()
{
  R=$(git symbolic-ref HEAD)
  case "$R" in
    refs/heads/*)
      BRANCH=${R#refs/heads/};;
    *)
      echo "Weird current brainch: $R"
      exit 1;;
  esac
}

branch()
{
  if [ -n "$BRANCH" ]; then
    if [ x$(git symbolic-ref -q HEAD) != xrefs/heads/master ]; then
      echo "In order to create stable branch you have to be on trunk!"
      exit 1
    fi
    git tag "${BRANCH}-branchpoint"
    git checkout -b $BRANCH
  else
    detect_current_branch

    if [ "x$BRANCH" = xmaster ]; then
      # Only allow tagging alpha releases from master

      CURR_VERSION=$(sed -ne 's/AM_INIT_AUTOMAKE(cfengine, \(.*\)).*/\1/p' configure.ac)

      if ! expr "$CURR_VERSION" : ".*a[0-9]\.revision.*" >/dev/null; then
        echo "Trying to tag non-alpha version from master branch. Don't."
        exit 1
      fi
    fi
  fi
}

dist()
{
  mkdir -p ../_dist/core
  cd ../_dist/core
  git init
  git fetch $REPO tag $CURR_VERSION
  git checkout $CURR_VERSION

  NO_SUBPROJECTS=1 ./autogen.sh
  make dist
}

check()
{
  cd ..
  tar xf core/cfengine-$CURR_VERSION.tar.gz
  cd cfengine-$CURR_VERSION
  ./configure --with-tokyocabinet --disable-coverage --disable-shared
  make check -j8
}

remove_revision()
{
  sed -i -e 's/AM_INIT_AUTOMAKE(cfengine, \(.*\)\.revision)/AM_INIT_AUTOMAKE(cfengine, \1)/' configure.ac

  if grep AM_INIT_AUTOMAKE configure.ac | grep -q revision; then
    echo "Unable to remove 'revision' from version in configure.ac"
    grep AM_INIT_AUTOMAKE configure.ac
    exit 1
  fi
}

do_tag()
{
  CURR_VERSION=$(sed -ne 's/AM_INIT_AUTOMAKE(cfengine, \(.*\)).*/\1/p' configure.ac)

  if [ -z "$CURR_VERSION" ]; then
    echo "Unable to parse current version from configure.ac"
    grep AM_INIT_AUTOMAKE configure.ac
    exit 1
  fi

  git tag -s $CURR_VERSION -m "Tagging $CURR_VERSION"
}

bump_version()
{
  sed -i -e 's/AM_INIT_AUTOMAKE(cfengine, \(.*\))/AM_INIT_AUTOMAKE(cfengine, '"$NEXT_VERSION"'.revision)/' configure.ac

  if ! grep AM_INIT_AUTOMAKE configure.ac | grep -q revision; then
    echo "Unable to bump version in configure.ac"
    grep AM_INIT_AUTOMAKE configure.ac
    exit 1
  fi
}

commit_version() {
  git add configure.ac
  git commit -m "$1"
}

tag()
{
  remove_revision
  commit_version "Pre-release version bump"
  do_tag
  bump_version
  commit_version "Post-release version bump"
}

REPO=$(git rev-parse --show-toplevel)

opts "$@"
branch
tag
dist
check
