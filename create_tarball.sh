#!/bin/sh
set -e

get_version()
{
  CONF_VERSION=$(sed -ne 's/AM_INIT_AUTOMAKE(cfengine, \(.*\)).*/\1/p' configure.ac)
  case "$CONF_VERSION" in
    *revision*)
      REVISION=$(git rev-list -1 --abbrev-commit HEAD || echo unknown)
      VERSION=$(echo $CONF_VERSION | sed -e "s/revision/${REVISION}/g")
      ;;
    *)
      VERSION=$CONF_VERSION
      ;;
  esac  
echo $VERSION
}
dist()

{ 
  git checkout $BRANCH
  ./autogen.sh --with-tokyocabinet=/usr
  make dist
}

check()
{
  CURR_VERSION=$(get_version)

  cd ..
  tar xf core/cfengine-$CURR_VERSION.tar.gz
  cd cfengine-$CURR_VERSION
  ./configure --with-tokyocabinet=/usr --disable-coverage --disable-shared
  make check -j8
}

if [ $# -eq 0 ]; then
  BRANCH=master
  echo
  echo "Branch/tag has not been specified. Using master branch by default."
  echo
else
  BRANCH=$1
  echo
  echo "Using $BRANCH"
  echo
fi

dist $BRANCH
check
