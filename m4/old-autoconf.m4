AC_DEFUN([AC_TYPE_LONG_DOUBLE],
[
  AC_CACHE_CHECK([for long double], [ac_cv_type_long_double],
    [if test "$GCC" = yes; then
       ac_cv_type_long_double=yes
     else
       AC_COMPILE_IFELSE(
         [AC_LANG_BOOL_COMPILE_TRY(
            [[/* The Stardent Vistra knows sizeof (long double), but does
                 not support it.  */
              long double foo = 0.0L;]],
            [[/* On Ultrix 4.3 cc, long double is 4 and double is 8.  */
              sizeof (double) <= sizeof (long double)]])],
         [ac_cv_type_long_double=yes],
         [ac_cv_type_long_double=no])
     fi])
  if test $ac_cv_type_long_double = yes; then
    AC_DEFINE([HAVE_LONG_DOUBLE], 1,
      [Define to 1 if the system has the type `long double'.])
  fi
])

AC_DEFUN([AC_TYPE_LONG_LONG_INT],
[
  AC_CACHE_CHECK([for long long int], [ac_cv_type_long_long_int],
    [AC_LINK_IFELSE(
       [_AC_TYPE_LONG_LONG_SNIPPET],
       [dnl This catches a bug in Tandem NonStop Kernel (OSS) cc -O circa 2004.
        dnl If cross compiling, assume the bug isn't important, since
        dnl nobody cross compiles for this platform as far as we know.
        AC_RUN_IFELSE(
          [AC_LANG_PROGRAM(
             [[@%:@include <limits.h>
               @%:@ifndef LLONG_MAX
               @%:@ define HALF \
                        (1LL << (sizeof (long long int) * CHAR_BIT - 2))
               @%:@ define LLONG_MAX (HALF - 1 + HALF)
               @%:@endif]],
             [[long long int n = 1;
               int i;
               for (i = 0; ; i++)
                 {
                   long long int m = n << i;
                   if (m >> i != n)
                     return 1;
                   if (LLONG_MAX / 2 < m)
                     break;
                 }
               return 0;]])],
          [ac_cv_type_long_long_int=yes],
          [ac_cv_type_long_long_int=no],
          [ac_cv_type_long_long_int=yes])],
       [ac_cv_type_long_long_int=no])])
  if test $ac_cv_type_long_long_int = yes; then
    AC_DEFINE([HAVE_LONG_LONG_INT], 1,
      [Define to 1 if the system has the type `long long int'.])
  fi
])

AC_DEFUN([AC_TYPE_UNSIGNED_LONG_LONG_INT],
[
  AC_CACHE_CHECK([for unsigned long long int],
    [ac_cv_type_unsigned_long_long_int],
    [AC_LINK_IFELSE(
       [_AC_TYPE_LONG_LONG_SNIPPET],
       [ac_cv_type_unsigned_long_long_int=yes],
       [ac_cv_type_unsigned_long_long_int=no])])
  if test $ac_cv_type_unsigned_long_long_int = yes; then
    AC_DEFINE([HAVE_UNSIGNED_LONG_LONG_INT], 1,
      [Define to 1 if the system has the type `unsigned long long int'.])
  fi
])

AC_DEFUN([AC_TYPE_INTMAX_T],
[
  AC_REQUIRE([AC_TYPE_LONG_LONG_INT])
  AC_CHECK_TYPE([intmax_t],
    [AC_DEFINE([HAVE_INTMAX_T], 1,
       [Define to 1 if the system has the type `intmax_t'.])],
    [test $ac_cv_type_long_long_int = yes \
       && ac_type='long long int' \
       || ac_type='long int'
     AC_DEFINE_UNQUOTED([intmax_t], [$ac_type],
       [Define to the widest signed integer type
        if <stdint.h> and <inttypes.h> do not define.])])
])

AC_DEFUN([AC_TYPE_UINTMAX_T],
[
  AC_REQUIRE([AC_TYPE_UNSIGNED_LONG_LONG_INT])
  AC_CHECK_TYPE([uintmax_t],
    [AC_DEFINE([HAVE_UINTMAX_T], 1,
       [Define to 1 if the system has the type `uintmax_t'.])],
    [test $ac_cv_type_unsigned_long_long_int = yes \
       && ac_type='unsigned long long int' \
       || ac_type='unsigned long int'
     AC_DEFINE_UNQUOTED([uintmax_t], [$ac_type],
       [Define to the widest unsigned integer type
        if <stdint.h> and <inttypes.h> do not define.])])
])

AC_DEFUN([AC_TYPE_UINTPTR_T],
[
  AC_CHECK_TYPE([uintptr_t],
    [AC_DEFINE([HAVE_UINTPTR_T], 1,
       [Define to 1 if the system has the type `uintptr_t'.])],
    [for ac_type in 'unsigned int' 'unsigned long int' \
        'unsigned long long int'; do
       AC_COMPILE_IFELSE(
         [AC_LANG_BOOL_COMPILE_TRY(
            [AC_INCLUDES_DEFAULT],
            [[sizeof (void *) <= sizeof ($ac_type)]])],
         [AC_DEFINE_UNQUOTED([uintptr_t], [$ac_type],
            [Define to the type of an unsigned integer type wide enough to
             hold a pointer, if such a type exists, and if the system
             does not define it.])
          ac_type=])
       test -z "$ac_type" && break
     done])
])

AC_DEFUN([_AC_TYPE_LONG_LONG_SNIPPET],
[
  AC_LANG_PROGRAM(
    [[/* For now, do not test the preprocessor; as of 2007 there are too many
         implementations with broken preprocessors.  Perhaps this can
         be revisited in 2012.  In the meantime, code should not expect
         #if to work with literals wider than 32 bits.  */
      /* Test literals.  */
      long long int ll = 9223372036854775807ll;
      long long int nll = -9223372036854775807LL;
      unsigned long long int ull = 18446744073709551615ULL;
      /* Test constant expressions.   */
      typedef int a[((-9223372036854775807LL < 0 && 0 < 9223372036854775807ll)
                     ? 1 : -1)];
      typedef int b[(18446744073709551615ULL <= (unsigned long long int) -1
                     ? 1 : -1)];
      int i = 63;]],
    [[/* Test availability of runtime routines for shift and division.  */
      long long int llmax = 9223372036854775807ll;
      unsigned long long int ullmax = 18446744073709551615ull;
      return ((ll << 63) | (ll >> 63) | (ll < i) | (ll > i)
              | (llmax / ll) | (llmax % ll)
              | (ull << 63) | (ull >> 63) | (ull << i) | (ull >> i)
              | (ullmax / ull) | (ullmax % ull));]])
])

