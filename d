#!/bin/bash
set -e
set -x
make -j16
./libtool --mode=execute gdb --args -KIf ./defined.cf cf-agent/cf-agent
