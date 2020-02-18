TERMUX_PKG_DEPENDS="liblmdb, openssl, libandroid-glob, pcre"
  TERMUX_PKG_EXTRA_CONFIGURE_ARGS="--with-workdir=$TERMUX_PREFIX/var/lib/cfengine --without-pam --without-selinux-policy --without-systemd-service --with-lmdb=$TERMUX_PREFIX --with-openssl=$TERMUX_PREFIX --with-pcre=$TERMUX_PREFIX"
  LDFLAGS+=" -landroid-glob"

    cd masterfiles-${TERMUX_PKG_VERSION}-build1 # TODO may need to be trickier with "-build1", maybe just cd masterfiles-*?
  EXPLICIT_VERSION=${TERMUX_PKG_VERSION} ./autogen.sh --prefix=$TERMUX_PREFIX/var/lib/cfengine --bindir=$TERMUX_PREFIX/bin
  make install
