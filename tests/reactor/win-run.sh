#!/bin/sh
set -ex

export CF_BINDIR="/c/Program Files/CFEngine/bin"
dir=$(cd "$(dirname "$0")" && pwd)
export TMPDIR="$dir"/tmp
mkdir -p "$TMPDIR"

bash -ex "$dir"/run.sh 2>&1 | tee test.log
