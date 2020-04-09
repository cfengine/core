#!/bin/bash
make -j16
cf-agent/cf-agent -KIf ./test.cf | tee log
#cf-agent/cf-agent -KIf ./defined.cf
#./libtool --mode=execute gdb --args -KIf ./defined.cf cf-agent/cf-agent
