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
    AC_MSG_CHECKING(for -Wunused-parameter")
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() {}])],
     [AC_MSG_RESULT(yes)],
     [AC_MSG_RESULT(no)
     CFLAGS="$save_CFLAGS"])
else 
    AC_MSG_RESULT(no)
fi
