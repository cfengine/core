# CF3_WITH_LIBRARY(library-name, checks)
# --------------------------------------
#
# This function popluates CFLAGS, CPPFLAGS and LDFLAGS from the
# --with-$library=PATH and runs a second argument with those options.
#
# After execution flags are returned to previous state, but available in
# ${LIBRARY}_{CFLAGS,LDFLAGS}. Path is available in ${LIBRARY}_PATH.
#
AC_DEFUN([CF3_WITH_LIBRARY],
[
m4_define([ULN],m4_toupper($1))

  if test  "x$with_[$1]" != xyes; then
    ULN[]_PATH="$with_[$1]"
    ULN[]_CFLAGS="-I$with_[$1]/include"
    ULN[]_LDFLAGS="-L$with_[$1]/lib -R$with_[$1]/lib"
  else
    ULN[]_PATH="default path"
  fi

  save_CFLAGS="$CFLAGS"
  save_CPPFLAGS="$CPPFLAGS"
  save_LDFLAGS="$LDFLAGS"

  CFLAGS="$CFLAGS $ULN[]_CFLAGS"
  CPPFLAGS="$CPPFLAGS $ULN[]_CFLAGS"
  LDFLAGS="$LDFLAGS $ULN[]_LDFLAGS"

  $2

  CFLAGS="$save_CFLAGS"
  CPPFLAGS="$save_CPPFLAGS"
  LDFLAGS="$save_LDFLAGS"
])
