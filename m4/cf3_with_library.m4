#
#  Copyright 2017 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by CFEngine AS.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation; version 3.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#
# To the extent this program is licensed as part of the Enterprise
# versions of CFEngine, the applicable Commercial Open Source License
# (COSL) may apply to this file if you as a licensee so wish it. See
# included file COSL.txt.
#
# CF3_WITH_LIBRARY(library-name, checks)
# --------------------------------------
#
# This function popluates CFLAGS, CPPFLAGS and LDFLAGS from the
# --with-$library=PATH and runs a second argument with those options.
#
# After execution flags are returned to previous state, but available in
# ${LIBRARY}_{CFLAGS,LDFLAGS}. Path is available in ${LIBRARY}_PATH.
#
# Libraries added to LIBS are available as ${LIBRARY}_LIBS afterwards.
#
AC_DEFUN([CF3_WITH_LIBRARY],
[
  m4_define([ULN],m4_toupper($1))

  #
  # Populate ${LIBRARY}_{PATH,CFLAGS,LDFLAGS} according to arguments
  #
  if test "x$with_[$1]" != xyes &&
     test "x$with_[$1]" != xcheck &&
     test "x$with_[$1]" != x
  then
    test -z "$ULN[]_PATH" && ULN[]_PATH="$with_[$1]"
    if test "x$with_[$1]" != x/usr &&
       test "x$with_[$1]" != x/
    then
      test -z "$ULN[]_CFLAGS" && ULN[]_CFLAGS=""
      test -z "$ULN[]_CPPFLAGS" && ULN[]_CPPFLAGS="-I$with_[$1]/include"
      test -z "$ULN[]_LDFLAGS" && ULN[]_LDFLAGS="-L$with_[$1]/lib"
    fi
  else
    ULN[]_PATH="default path"
  fi

  #
  # Save old environment
  #
  save_CFLAGS="$CFLAGS"
  save_CPPFLAGS="$CPPFLAGS"
  save_LDFLAGS="$LDFLAGS"
  save_LIBS="$LIBS"

  CFLAGS="$CFLAGS $ULN[]_CFLAGS"
  CPPFLAGS="$CPPFLAGS $ULN[]_CPPFLAGS"
  LDFLAGS="$LDFLAGS $ULN[]_LDFLAGS"

  #
  # Run checks passed as argument
  #
  $2

  #
  # Pick up any libraries added by tests
  #
  test -z "$ULN[]_LIBS" && ULN[]_LIBS="$LIBS"

  #
  # libtool understands -R$path, but we are not using libtool in configure
  # snippets, so -R$path goes to $pkg_LDFLAGS only after autoconf tests
  #
  if test "x$with_[$1]" != xyes &&
     test "x$with_[$1]" != xcheck &&
     test "x$with_[$1]" != x/usr &&
     test "x$with_[$1]" != x/
  then
    ULN[]_LDFLAGS="$ULN[]_LDFLAGS -R$with_[$1]/lib"
  fi

  #
  # Restore pristine environment
  #
  CFLAGS="$save_CFLAGS"
  CPPFLAGS="$save_CPPFLAGS"
  LDFLAGS="$save_LDFLAGS"
  LIBS="$save_LIBS"

  AC_SUBST(ULN[]_PATH)
  AC_SUBST(ULN[]_CPPFLAGS)
  AC_SUBST(ULN[]_CFLAGS)
  AC_SUBST(ULN[]_LDFLAGS)
  AC_SUBST(ULN[]_LIBS)
])
