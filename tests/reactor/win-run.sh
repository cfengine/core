#!/bin/sh
set -ex

export CF_BINDIR="/c/Program Files/CFEngine/bin"
dir=$(cd "$(dirname "$0")" && pwd)
export TMPDIR="$dir"/tmp
mkdir -p "$TMPDIR"

# The exit status of a pipeline is that of its last command, tee here, so
# record the one of run.sh in a file and exit with it
{
    bash -ex "$dir"/run.sh 2>&1 && rc=0 || rc=$?
    echo "$rc" > "$TMPDIR/run.rc"
} | tee test.log
exit "$(cat "$TMPDIR/run.rc")"
