#!/usr/bin/env bash
# dependencies.sh is called by install.sh to install libraries and packages needed to build and install CFEngine from source.
set -ex

GAINROOT=""
if [ "$(id -u)" != "0" ]; then
    GAINROOT="sudo"
    if ! command -v sudo >/dev/null; then
        echo "Sorry, either run $0 as root or install sudo."
        exit 1
    fi
fi

build_lmdb() {
    if [ -f /usr/include/lmdb.h ]; then
        echo lmdb libraries already installed, no-op.
        return
    fi
    tmpdir="$(mktemp -d)"
    echo building lmdb in "$tmpdir"
    (
        cd "$tmpdir"
        git clone --recursive --depth 1 https://github.com/LMDB/lmdb
        cd lmdb/libraries/liblmdb
        make
        $GAINROOT make install prefix=/usr
    )
}

build_librsync() {
    tmpdir="$(mktemp -d)"
    echo building librsync in "$tmpdir"
    (
        cd "$tmpdir"
        git clone --recursive --depth 1 https://github.com/librsync/librsync
        cd librsync
        cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release .
        make
        $GAINROOT make install
    )
}

# limited support here, focused on rhel-like on aarch64 which has no previous CFEngine version to leverage: ENT-13016
if [ -f /etc/os-release ]; then
    source /etc/os-release
    VERSION_MAJOR=${VERSION_ID%.*}
    if [[ "$ID_LIKE" =~ "suse" ]]; then
        $GAINROOT zypper -qn install gdb gcc make lmdb autoconf automake libtool git python3 pcre2-devel libopenssl-devel pam-devel cmake flex byacc
        build_lmdb
        build_librsync
    elif [ "$ID" = "rhel" ] || [[ "$ID_LIKE" =~ "rhel" ]]; then
        if [ "$VERSION_MAJOR" -ge "10" ]; then
            # note that having a redhat subscription makes things easier: lmdb-devel and librsync-devel are available from codeready-builder repo
            if subscription-manager status; then
                $GAINROOT subscription-manager config --rhsm.manage_repos=1
                $GAINROOT subscription-manager repos --enable codeready-builder-for-rhel-"$VERSION_MAJOR"-"$(uname -m)"-rpms
                $GAINROOT dnf install --assumeyes https://dl.fedoraproject.org/pub/epel/epel-release-latest-"$VERSION_MAJOR".noarch.rpm
                $GAINROOT dnf install --assumeyes autoconf automake
                $GAINROOT dnf install --assumeyes gcc gdb make git libtool autoconf automake byacc flex openssl-devel pcre2-devel lmdb-devel pam-devel flex-devel libyaml-devel fakeroot libxml2-devel librsync-devel
                # flex-devel, libyaml-devel and fakeroot are also only available easily from codeready-builder but are not critical to building CFEngine usable enough to configure a build host.
                # fakeroot is only needed for running tests but can be worked around by using GAINROOT=env with tests/acceptance/testall script
            else
                # here we assume no subscription and so must build those two dependencies from source :)
                $GAINROOT yum groups install --assumeyes 'Development Tools'
                $GAINROOT yum update --assumeyes
                # cmake is for building librsync
                $GAINROOT yum install --assumeyes gcc gdb make git libtool autoconf automake byacc flex openssl-devel pcre2-devel pam-devel libxml2-devel cmake
                build_lmdb
                build_librsync
            fi
        else
            echo "Unsupported version of redhat for $0"
            exit 1
        fi
    elif [ "$ID" = "debian" ] || [[ "$ID_LIKE" =~ "debian" ]]; then
        $GAINROOT apt update --yes
        $GAINROOT apt install --yes build-essential git libtool autoconf automake bison flex libssl-dev libpcre2-dev libbison-dev libacl1 libacl1-dev lmdb-utils liblmdb-dev libpam0g-dev libtool libyaml-dev libxml2-dev librsync-dev
    else
        echo "Unsupported distribution based on /etc/os-release."
    fi
elif [ -n "$TERMUX_VERSION" ]; then
    pkg install build-essential git autoconf automake bison flex liblmdb openssl pcre2 libacl libyaml
else
    echo "Unsupported operating system for $0"
    exit 1
fi
