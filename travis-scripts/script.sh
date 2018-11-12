#!/bin/sh
set -e
INSTDIR=$HOME/cf_install
cd $TRAVIS_BUILD_DIR || exit 1

# if [ "$JOB_TYPE" = style_check ]
# then
#     # sh tests/misc/style_check.sh
#     exit 0
# fi

# Unshallow the clone. Fetch the tags from upstream even if we are on a
# foreign clone. Needed for determine-version.py to work, specifically
# `git describe --tags HEAD` was failing once the last tagged commit
# became too old.
git fetch --unshallow
git remote add upstream https://github.com/cfengine/core.git  \
    && git fetch upstream 'refs/tags/*:refs/tags/*'

if [ "$TRAVIS_OS_NAME" = osx ]
then
    set +e
    # On osx the default gcc is actually LLVM
    export CC=gcc-7
    NO_CONFIGURE=1 ./autogen.sh
    ./configure --enable-debug --prefix=$INSTDIR --bindir=$INSTDIR/var/cfengine/bin --with-init-script --with-lmdb=/usr/local/Cellar/lmdb
else
    NO_CONFIGURE=1 ./autogen.sh
    ./configure --enable-debug --prefix=$INSTDIR --with-systemd-service --bindir=$INSTDIR/var/cfengine/bin \
        `[ "x$COVERAGE" != xno ] && echo --enable-coverage`
fi

make dist

DIST_TARBALL=`echo cfengine-*.tar.gz`
export DIST_TARBALL

if [ "$JOB_TYPE" = compile_only ]
then
    make CFLAGS=-Werror
elif [ "$JOB_TYPE" = compile_and_unit_test ]
then
    make CFLAGS="-Werror -Wno-pointer-sign"  &&
    make -C tests/unit check
    make -C tests/load check
    exit
elif [ "$JOB_TYPE" = compile_and_unit_test_asan ]
then
    make CFLAGS="-Werror -Wno-pointer-sign -fsanitize=address" LDFLAGS="-fsanitize=address -pthread"
    make -C tests/unit CFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address -pthread" check
    make -C tests/load CFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address -pthread" check
    exit
else
    make
fi

cd tests/acceptance || exit 1
chmod -R go-w .

if [ "$JOB_TYPE" = acceptance_tests_common ]
then
    ./testall --tests=common
    exit
fi

# WARNING: the following job runs the selected tests as root!
# We are chmod'ing in the end so that code coverage data is readable from user
if [ "$JOB_TYPE" = acceptance_tests_unsafe_serial_network_etc ]
then
    ./testall --gainroot=sudo --tests=timed,slow,errorexit,libxml2,libcurl,serial,network,unsafe
    exit
fi

if [ "$JOB_TYPE" = serverd_multi_versions ]
then
    cd ../..
    set +e
    tests/acceptance/serverd-multi-versions.sh
    exit
fi
