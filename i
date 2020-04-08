#!/bin/bash
set -e
set -x
make -j16
pushd tests/acceptance
./testall --debug --printlog 01_vars/02_functions/ifelse_isvariable-ENT-4653.cf | tee ../../log
#./testall --verbose --printlog 01_vars/02_functions/ifelse_isvariable-ENT-4653.cf | tee ../../log
popd
