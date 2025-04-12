#!/usr/bin/env bash
set -ex
export LDFLAGS+=" -landroid-glob"
./autogen.sh \
  --prefix=$PREFIX \
  --with-workdir=$PREFIX/var/lib/cfengine \
  --without-pam \
  --without-selinux-policy \
  --without-systemd-service
make -j8 install
#DESTDIR=/data/data/com.termux/files make -j8 install
