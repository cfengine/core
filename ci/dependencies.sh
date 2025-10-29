#!/usr/bin/env bash
# dependencies.sh is called by install.sh to install libraries and packages needed to build and install CFEngine from source.
set -ex
# limited support here, focused on rhel-like on aarch64 which has no previous CFEngine version to leverage: ENT-13016
if [ -f /etc/os-release ]; then
  source /etc/os-release
  VERSION_MAJOR=${VERSION_ID%.*}
  if [ "$ID" = "rhel" ] || [[ "$ID_LIKE" =~ "rhel" ]]; then
    if [ "$VERSION_MAJOR" -ge "10" ]; then
      # note that having a redhat subscription makes things easier: lmdb-devel and librsync-devel are available from codeready-builder repo
      if subscription-manager status; then
        sudo subscription-manager config --rhsm.manage_repos=1
        sudo subscription-manager repos --enable codeready-builder-for-rhel-"$VERSION_MAJOR"-"$(uname -m)"-rpms
        sudo dnf install --assumeyes https://dl.fedoraproject.org/pub/epel/epel-release-latest-"$VERSION_MAJOR".noarch.rpm
        sudo dnf install --assumeyes flex-devel lmdb-devel librsync-devel fakeroot # only available via subscription with codeready-builder installed
        # flex-devel, libyaml-devel and fakeroot are also only available easily from codeready-builder but are not critical to building CFEngine usable enough to configure a build host.
        # fakeroot is only needed for running tests but can be worked around by using GAINROOT=env with tests/acceptance/testall script
      else
        # here we assume no subscription and so must build those two dependencies from source :)
        sudo yum groups install -y 'Development Tools'
        sudo yum update --assumeyes
        sudo yum install -y gcc gdb make git libtool autoconf automake byacc flex openssl-devel pcre2-devel pam-devel libxml2-devel
        tmpdir="$(mktemp -d)"
        echo "Building lmdb and librsync in $tmpdir"
        (
          cd "$tmpdir"
          git clone --recursive --depth 1 https://github.com/LMDB/lmdb
          cd lmdb/libraries/liblmdb
          make
          sudo make install prefix=/usr
          cd -
          sudo dnf install -y cmake
          git clone --recursive --depth 1 https://github.com/librsync/librsync
          cd librsync
          cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
          make
          sudo make install
        )
      fi
    else
      echo "Unsupported version of redhat for $0"
      exit 1
    fi
  elif [ "$ID" = "ubuntu" ]; then
    sudo apt update -y
    sudo apt install -y libssl-dev libpam0g-dev liblmdb-dev byacc curl librsync-dev
  else
    echo "Unsupported distribution based on /etc/os-release."
  fi
elif [ -n "$TERMUX_VERSION" ]; then
  pkg install build-essential git autoconf automake bison flex liblmdb openssl pcre2 libacl libyaml
else
  echo "Unsupported operating system for $0"
  exit 1
fi

