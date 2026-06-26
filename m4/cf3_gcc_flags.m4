#
#  Copyright 2021 Northern.tech AS
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
dnl Set GCC flags only if using GCC.
dnl Flags are collected into CF3_CFLAGS for use in AM_CFLAGS.
dnl CFLAGS is left untouched for the user.
dnl ####################################################################

AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
#if defined __HP_cc
#This is HP-UX ANSI C
#endif
]])], [
HP_UX_AC="no"], [
CF3_CFLAGS="$CF3_CFLAGS -Agcc"
HP_UX_AC="yes"])

AC_MSG_CHECKING(for HP-UX aC)
if test "x$HP_UX_AC" = "xyes"; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING(for GCC specific compile flags)
if test x"$GCC" = "xyes" && test x"$HP_UX_AC" != x"yes"; then
    CF3_CFLAGS="$CF3_CFLAGS -std=gnu99 -g -Wall"
    AC_MSG_RESULT(yes)

    AC_MSG_CHECKING(for -Wno-pointer-sign)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)
     CF3_CFLAGS="$CF3_CFLAGS -Wno-pointer-sign"],
     [AC_MSG_RESULT(no)])

    AC_MSG_CHECKING(for -Werror=implicit-function-declaration)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)
     CF3_CFLAGS="$CF3_CFLAGS -Werror=implicit-function-declaration"],
     [AC_MSG_RESULT(no)])

    AC_MSG_CHECKING(for -Wunused-parameter)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)
     CF3_CFLAGS="$CF3_CFLAGS -Wunused-parameter"],
     [AC_MSG_RESULT(no)])

    AC_MSG_CHECKING(for -Wno-incompatible-pointer-types)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)
     CF3_CFLAGS="$CF3_CFLAGS -Wno-incompatible-pointer-types"],
     [AC_MSG_RESULT(no)])

    AC_MSG_CHECKING(for -Wno-duplicate-decl-specifier)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([#ifndef __clang__
# GET_DIR_ERROR Not a clang
#endif
int main() {}])],
     [AC_MSG_RESULT(yes)
     CF3_CFLAGS="$CF3_CFLAGS -Wno-duplicate-decl-specifier"],
     [AC_MSG_RESULT(no)])
else
    AC_MSG_RESULT(no)
fi

AC_SUBST([CF3_CFLAGS])
