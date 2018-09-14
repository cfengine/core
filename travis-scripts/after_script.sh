#!/bin/sh
set +e
cd $TRAVIS_BUILD_DIR || return 1
mkdir artifacts
test "x$DIST_TARBALL" != x  &&  cp "$DIST_TARBALL" artifacts/
mv config.log                   artifacts/ 2>/dev/null
mv tests/acceptance/summary.log artifacts/ 2>/dev/null
mv tests/acceptance/test.log    artifacts/ 2>/dev/null
mv tests/acceptance/workdir     artifacts/ 2>/dev/null
mv serverd-multi-versions-logs  artifacts/ 2>/dev/null

VERSION=$(expr "$DIST_TARBALL" : "cfengine-\(.*\).tar.gz")
VERSION=${VERSION:-master}
test "$TRAVIS_PULL_REQUEST" = "false" && BRANCH_OR_PULL_REQUEST=$TRAVIS_BRANCH || BRANCH_OR_PULL_REQUEST=PULL_$TRAVIS_PULL_REQUEST

filename=cfengine-$VERSION-$BRANCH_OR_PULL_REQUEST-$JOB_TYPE.artifacts.zip
zip -q -r $filename artifacts/

echo ===== uploading $filename to transfer.sh =====
curl --upload-file $filename https://transfer.sh/$filename

cd - >/dev/null
