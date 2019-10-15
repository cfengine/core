#!/bin/sh
cd $TRAVIS_BUILD_DIR || return 1
mkdir artifacts
test "x$DIST_TARBALL" != x  &&  cp --verbose "$DIST_TARBALL" artifacts/
gzip config.log  &&  mv config.log.gz artifacts/
gzip tests/acceptance/summary.log  &&  mv tests/acceptance/summary.log.gz artifacts/acceptance_summary.log.gz
gzip tests/acceptance/test.log     &&  mv tests/acceptance/test.log.gz    artifacts/acceptance.log.gz
zip -r artifacts/acceptance_workdir.zip tests/acceptance/workdir

for filename in artifacts/*; do
    echo ===== uploading $filename to file.io =====
    curl -F "file=@$filename" https://file.io
    echo 'Note that file.io DELETES file from their servers after first download,'
    echo "so don't delete it from your machine if you still need it!"
done
