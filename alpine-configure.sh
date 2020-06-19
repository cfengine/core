./configure \
  CFLAGS="-D__ALPINE__" \
  --with-pic \
  --prefix=/usr \
  --enable-fhs \
  --localstatedir=/var \
  --mandir=/usr/share/man \
  --with-lmdb \
  --without-pam
