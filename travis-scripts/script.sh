#!/bin/sh
set -e
set -x

cd $TRAVIS_BUILD_DIR || exit 1

if [ "$JOB_TYPE" = valgrind_health_check ]
then
    sudo bash travis-scripts/valgrind.sh
    exit
fi

INSTDIR=$HOME/cf_install

# if [ "$JOB_TYPE" = style_check ]
# then
#     # sh tests/misc/style_check.sh
#     exit 0
# fi

# Fetch the tags from upstream even if we are on a
# foreign clone. Needed for determine-version.sh to work.
git remote add upstream https://github.com/cfengine/core.git  \
    && git fetch --no-recurse-submodules upstream 'refs/tags/*:refs/tags/*'

if [ "$TRAVIS_OS_NAME" = osx ]
then
    ./autogen.sh --enable-debug --with-openssl="$(brew --prefix openssl)" --prefix=$INSTDIR --bindir=$INSTDIR/var/cfengine/bin
    gmake --version
    gmake CFLAGS="-Werror -Wall"
    # Tests are disabled on OS X, because they started hanging in travis,
    # for no apparent reason.
    # gmake --debug -C tests/unit check
    exit
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
    make CFLAGS="-Werror" -k
elif [ "$JOB_TYPE" = compile_and_unit_test ]
then
    make CFLAGS="-Wall -Wextra -Werror -Wno-sign-compare"
    make -C tests/unit check
    make -C tests/load check
    exit
elif [ "$JOB_TYPE" = compile_and_unit_test_asan ]
then
    make CFLAGS="-Werror -Wall -fsanitize=address" LDFLAGS="-fsanitize=address"
    make -C tests/unit CFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address" check
    make -C tests/load CFLAGS="-fsanitize=address" LDFLAGS="-fsanitize=address" check
    exit
else
    make
fi

cd tests/acceptance || exit 1
chmod -R go-w .

if [ "$JOB_TYPE" = acceptance_tests_common ]
then
    ./testall --printlog --tests=common,errorlog
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
