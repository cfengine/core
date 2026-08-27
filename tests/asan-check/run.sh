#!/bin/bash

set -e
trap "echo FAILURE" ERR

set -x

cd "$(dirname "$0")"/../../

if which podman ; then
  CLI="sudo podman --cgroup-manager=cgroupfs"
else
  CLI="docker"
fi

$CLI build --tag ubuntu:mycfecontainer -f ./tests/asan-check/Containerfile .
$CLI run --rm ubuntu:mycfecontainer
