#!/bin/sh
set +e
cd $TRAVIS_BUILD_DIR || return 1
sudo apt-get -qy install curl
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

echo ===== uploading $filename to file.io =====
curl -F "file=@$filename" https://file.io
echo 'Note that file.io DELETES file from their servers after first download,'
echo "so don't delete it from your machine if you still need it!"

cd - >/dev/null
