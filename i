#!/bin/bash
set -e
set -x
make -j16
pushd tests/acceptance
./testall --printlog 01_vars/02_functions/ifelse_isvariable-ENT-4653.cf
popd
