#!/bin/sh
set -e

#
# This script is only useful for git-svn-maintained repository.
#

SVN_URL=https://c.cfengine.com/svn/core

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
    refs/heads/master)
      echo "Trying to tag from trunk. Don't."
      exit 1;;
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
    svn copy $SVN_URL/trunk/ $SVN_URL/branches/$BRANCH -m "Create $BRANCH from trunk"
    git svn fetch
    git checkout -b $BRANCH remotes/$BRANCH
  else
    detect_current_branch
  fi
}

dist()
{
  mkdir ../_dist
  cd ../_dist
  svn co $SVN_URL/branches/$BRANCH core
  cd core
  remove_svnversion

  CURR_VERSION=$(sed -ne 's/AM_INIT_AUTOMAKE(cfengine, \(.*\)).*/\1/p' configure.ac)

  if [ -z "$CURR_VERSION" ]; then
    echo "Unable to parse current version from configure.ac"
    grep AM_INIT_AUTOMAKE configure.ac
    exit 1
  fi

  NO_CONFIGURE=1 NO_SUBPROJECTS=1 ./autogen.sh
  ./configure
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

remove_svnversion()
{
  sed -i -e 's/AM_INIT_AUTOMAKE(cfengine, \(.*\)\.svnversion)/AM_INIT_AUTOMAKE(cfengine, \1)/' configure.ac

  if grep AM_INIT_AUTOMAKE configure.ac | grep -q svnversion; then
    echo "Unable to remove 'svnversion' from version in configure.ac"
    grep AM_INIT_AUTOMAKE configure.ac
    exit 1
  fi
}

do_svn_tag()
{
  svn copy $SVN_URL/branches/$BRANCH $SVN_URL/tags/$CURR_VERSION -m "Tagging $CURR_VERSION"
  git svn fetch
}

bump_version()
{
  sed -i -e 's/AM_INIT_AUTOMAKE(cfengine, \(.*\))/AM_INIT_AUTOMAKE(cfengine, '"$NEXT_VERSION"'.svnversion)/' configure.ac

  if ! grep AM_INIT_AUTOMAKE configure.ac | grep -q svnversion; then
    echo "Unable to bump version in configure.ac"
    grep AM_INIT_AUTOMAKE configure.ac
    exit 1
  fi
}

commit_version() {
  git add configure.ac
  git commit -m "$1"
  git svn dcommit
}

tag()
{
  remove_svnversion
  commit_version "Pre-release version bump"
  do_svn_tag
  bump_version
  commit_version "Post-release version bump"
}

opts "$@"
branch
(dist && check)
tag
