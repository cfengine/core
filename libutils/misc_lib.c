/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <misc_lib.h>

#include <platform.h>
#include <alloc.h>

#include <stdarg.h>

unsigned long UnsignedModulus(long dividend, long divisor)
{
    return ((dividend % divisor) + divisor) % divisor;
}

void __ProgrammingError(const char *file, int lineno, const char *format, ...)
{
    va_list ap;
    char *fmt = NULL;

    va_start(ap, format);
    xasprintf(&fmt, "%s:%d: Programming Error: %s\n", file, lineno, format);
    vfprintf(stdout, fmt, ap);
    va_end(ap);

    free(fmt);
#ifdef NDEBUG
    exit(255);
#else
    abort();
#endif
}

/**
  @brief Log unexpected runtime error to stderr, do not exit program.
*/

void __UnexpectedError(const char *file, int lineno, const char *format, ...)
{
    va_list ap;
    char *fmt = NULL;

    va_start(ap, format);
    xasprintf(&fmt,
              "%s:%d: Unexpected Error - this is a BUG, please report it: %s\n",
              file, lineno, format);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    free(fmt);
}
