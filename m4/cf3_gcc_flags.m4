#
#  Copyright 2017 Northern.tech AS
#
#  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.
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
dnl ####################################################################
dnl Set GCC CFLAGS only if using GCC.
dnl ####################################################################

AC_MSG_CHECKING(for HP-UX aC)
AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
#if defined __HP_cc
#This is HP-UX ANSI C
#endif
]])], [AC_MSG_RESULT(no)],[AC_MSG_RESULT(yes)
CFLAGS="$CFLAGS -Agcc"
CPPFLAGS="$CPPFLAGS -Agcc"
HP_UX_AC=yes])

AC_MSG_CHECKING(for GCC specific compile flags)
if test x"$GCC" = "xyes" && test x"$HP_UX_AC" != x"yes"; then
    CFLAGS="$CFLAGS -g -Wall"
    CPPFLAGS="$CPPFLAGS -std=gnu99"
    AC_MSG_RESULT(yes)

    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Wno-pointer-sign"
    AC_MSG_CHECKING(for -Wno-pointer-sign)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)],
     [AC_MSG_RESULT(no)
     CFLAGS="$save_CFLAGS"])

    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Werror=implicit-function-declaration"
    AC_MSG_CHECKING(for -Werror=implicit-function-declaration)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)],
     [AC_MSG_RESULT(no)
     CFLAGS="$save_CFLAGS"])

    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Wunused-parameter"
    AC_MSG_CHECKING(for -Wunused-parameter)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)],
     [AC_MSG_RESULT(no)
     CFLAGS="$save_CFLAGS"])

    dnl Clang does not like 'const const' construct arising from
    dnl expansion of TYPED_SET_DECLARE macro
    dnl
    dnl This check is relying on explicit compilator detection due to
    dnl GCC irregularities checking for -Wno-* command-line options
    dnl (command line is not fully checked until actual warning occurs)
    AC_MSG_CHECKING(for -Wno-duplicate-decl-specifier)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([#ifndef __clang__
# error Not a clang
#endif
int main() {}])],
     [AC_MSG_RESULT(yes)
     CFLAGS="$save_CFLAGS -Wno-duplicate-decl-specifier"],
     [AC_MSG_RESULT(no)])
else 
    AC_MSG_RESULT(no)
fi
