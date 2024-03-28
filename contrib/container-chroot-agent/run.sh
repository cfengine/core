#!/usr/bin/env bash
set -ex
test ! -d /var/cfengine/bin
PWD=$(pwd)

name=cfengine-chroot-agent
docker build -t "$name" -f "${PWD}/Dockerfile-cfengine-chroot-agent" "${PWD}"

# TODO fill in an appropriate IP address for your hub
CFENGINE_HUB_IP=192.168.1.196
export CFENGINE_HUB_IP

# warning: running --privileged on a linux system that is using a GUI will bring down the GUI most likely
docker run -d --env CFENGINE_HUB_IP --privileged \
  -v "/:/rootfs" \
  -v "/proc:/rootproc" \
  -v "/sys:/rootsys" \
  -v "/dev:/rootdev" \
  -v "/run:/rootrun" \
  --name "$name" \
  "$name"
sleep 3
if ! docker ps | grep $name; then
  docker logs $name
fi
