# strndup.m4 serial 21
dnl Copyright (C) 2002-2003, 2005-2013 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([cf3_FUNC_FCHMODAT],
[
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles
  AC_CHECK_DECLS([fchmodat], [], [], [[#include <sys/stat.h>]])
  AC_REPLACE_FUNCS([fchmodat])
  if test $ac_cv_have_decl_fchmodat = no; then
    HAVE_DECL_FCHMODAT=0
  fi

  # Some platforms do not support setting higher bit flags in the file mode
  # when executing as an alternative user. We need to be an alternative user
  # for security reasons, so check that feature here. See file_lib.c for more
  # information.
  AC_CACHE_CHECK([whether chmod works with seteuid], [cf3_cv_func_chmod_seteuid_highbits_work],
    [AC_RUN_IFELSE([
      AC_LANG_PROGRAM([[#include <sys/types.h>
                        #include <sys/stat.h>
                        #include <fcntl.h>
                        #include <unistd.h>
                        #include <stdio.h>
                        #include <string.h>
                        #include <errno.h>]], [[
#define TEST_FILE "chmod_test.txt"

  int fd;
  int ret;
  int euid_set;
  uid_t old_euid;
  struct stat statbuf;

  fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
  if (fd < 0)
  {
    // Should never fail. But assume we don't have support.
    printf("open failed: %s\n", strerror(errno));
    ret = 1;
    goto cleanup;
  }

  old_euid = geteuid();
  if (old_euid == 0)
  {
    ret = chown(TEST_FILE, 1, 1);
    if (fd < 0)
    {
      // Should never fail. But assume we don't have support.
      printf("chown failed: %s\n", strerror(errno));
      return 1;
    }
    ret = seteuid(1);
    if (fd < 0)
    {
      // Should never fail. But assume we don't have support.
      printf("seteuid failed: %s\n", strerror(errno));
      return 1;
    }
    euid_set = 1;
  }
  else
  {
    euid_set = 0;
  }

  ret = chmod(TEST_FILE, 01644);
  if (fd < 0)
  {
    // Should never fail. But assume we don't have support.
    printf("chmod failed: %s\n", strerror(errno));
    ret = 1;
    goto cleanup;
  }
  ret = stat(TEST_FILE, &statbuf);
  if (fd < 0)
  {
    // Should never fail. But assume we don't have support.
    printf("stat failed: %s\n", strerror(errno));
    ret = 1;
    goto cleanup;
  }
  if (statbuf.st_mode & 01000)
  {
    // Supported.
    ret = 0;
  }
  else
  {
    // Not supported.
    ret = 1;
  }

cleanup:
  if (euid_set)
  {
    seteuid(old_euid);
  }

  if (fd >= 0)
  {
    close(fd);
  }
  unlink(TEST_FILE);

  return ret;]])],
      [cf3_cv_func_chmod_seteuid_highbits_work=yes],
      [cf3_cv_func_chmod_seteuid_highbits_work=no],
      [
         AS_CASE([$host_os],
           [aix|hpux], [
             cf3_cv_func_chmod_seteuid_highbits_work=yes
           ], [
             cf3_cv_func_chmod_seteuid_highbits_work=no
           ]
         )
      ]
      )
    ]
  )

  AS_CASE([$cf3_cv_func_chmod_seteuid_highbits_work],
    [yes], [
      AC_DEFINE(CHMOD_SETEUID_HIGHBITS_WORK, 1, [Whether chmod supports higher bit flags in mode argument when running as alternative user.])
    ]
  )
])
