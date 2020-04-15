#!/bin/bash
# a list of various test iteration steps I used during testing
set -e
set -x
#make -j16 2>&1 && true || exit | tee make.log
make -j16
#make -C tests/unit expand_test && tests/unit/expand_test
cf-agent/cf-agent -K --debug -f ./test2.cf | tee log
#cf-agent/cf-agent -K --debug -f ./test.cf | tee log
#cf-agent/cf-agent -KIf ./defined.cf
#./libtool --mode=execute gdb cf-agent/cf-agent
#./libtool --mode=execute gdb --args -KIf ./defined.cf cf-agent/cf-agent
