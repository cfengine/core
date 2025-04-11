export LDFLAGS+=" -landroid-glob"
./autogen.sh \
  --with-workdir=$PREFIX/var/lib/cfengine  \
  --prefix=$PREFIX/local \
  --without-pam \
  --without-selinux-policy \
  --without-systemd-service
