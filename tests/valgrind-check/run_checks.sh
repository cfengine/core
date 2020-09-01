#!/bin/bash

set -x

function check_with_valgrind() {
  bash tests/valgrind-check/valgrind.sh
}

cd "$(dirname $0)"/../../

failure=0
check_with_valgrind || { echo "FAIL: valgrind check failed"; failure=1; }

exit $failure
