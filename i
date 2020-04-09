#!/bin/bash
set -e
set -x
make -j16 2>&1 | tee make.log
cf-agent/cf-agent -K --debug -f ./test.cf | tee log
#cf-agent/cf-agent -KIf ./defined.cf
#./libtool --mode=execute gdb cf-agent/cf-agent
#./libtool --mode=execute gdb --args -KIf ./defined.cf cf-agent/cf-agent
