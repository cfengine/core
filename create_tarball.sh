#!/bin/sh
#
#  Copyright 2017 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by CFEngine AS.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
# To the extent this program is licensed as part of the Enterprise
# versions of CFEngine, the applicable Commercial Open Source License
# (COSL) may apply to this file if you as a licensee so wish it. See
# included file COSL.txt.
#
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
