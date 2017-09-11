#!/bin/sh
# A tag ending with "number.number" is not a prerelease
if echo $DIST_TARBALL | grep -E -q '[0-9]{1,2}\.[0-9]{1,2}\.tar\.gz';
then
    IS_PRERELEASE=false;
else
    IS_PRERELEASE=true;
fi;
export IS_PRERELEASE
