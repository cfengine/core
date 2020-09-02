#!/bin/bash

set -e
trap "echo FAILURE" ERR

set -x

cd ../../../

if which podman ; then
  CLI="sudo podman --cgroup-manager=cgroupfs"
else
  CLI="docker"
fi

$CLI build --tag ubuntu:mycfecontainer -f ./core/tests/valgrind-check/Containerfile .
$CLI run --rm ubuntu:mycfecontainer
