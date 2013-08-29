dnl
dnl Arguments:
dnl  $1 - function name
dnl  $2 - headers (to compile $3)
dnl  $3 - body for compilation
dnl  $4 - function invocation
dnl
dnl This macro checks that the function (argument 1) is defined,
dnl and that the code piece (arguments 2, 3, like in AC_LANG_PROGRAM) can be
dnl compiled.
dnl
dnl If the code compiles successfully, it defines HAVE_$1_PROPER macro.
dnl
dnl If the code fails, it adds '$4' to $post_macros variable. 
dnl If you want rpl_$1.c to be compiled as a replacement, call
dnl CF3_REPLACE_PROPER_FUNC with the same function name.
dnl
dnl  ** How to use **
dnl
dnl  CF3_CHECK_PROPER_FUNC(function, [#include <stdio.h>], [void function(FILE *);], [#define function rpl_function])
dnl  CF3_REPLACE_PROPER_FUNC(function)
dnl
dnl  Then in libutils/platform.h:
dnl
dnl    #if !HAVE_FUNCTION_PROPER
dnl    void rpl_function(FILE *);
dnl    #endif
dnl
dnl  And libcompat/rpl_function.c:
dnl
dnl    #include "platform.h"
dnl
dnl    void rpl_function(FILE *) { ... }
dnl

AC_DEFUN([CF3_CHECK_PROPER_FUNC],
[
  AC_CHECK_FUNC([$1], [], [AC_MSG_ERROR([Unable to find function $1])])

  AC_CACHE_CHECK([whether $1 is properly declared],
    [hw_cv_func_$1_proper],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([$2],[$3])],
      [hw_cv_func_$1_proper=yes],
      [hw_cv_func_$1_proper=no])])

  AC_SUBST([hw_cv_func_$1_proper])

  AS_IF([test "$hw_cv_func_$1_proper" = yes],
    [AC_DEFINE([HAVE_$1_PROPER], [1], [Define to 1 if you have properly defined `$1' function])],
    [post_macros="$post_macros
$4"])
])

AC_DEFUN([CF3_REPLACE_PROPER_FUNC],
[
  AS_IF([test "$hw_cv_func_$1_proper" = "no"],
    [AC_LIBOBJ(rpl_$1)]
  )
])
