/*
   Copyright 2018 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#ifndef CFENGINE_DEPRECATED_H
#define CFENGINE_DEPRECATED_H


#include <platform.h>
#include <compiler.h>


/* Mark specific functions as deprecated so that we don't use them. Since the
 * signature of the functions has to be exactly the same as in libc, we only
 * do that for Linux, where main development happens. */


#if defined(__linux__) && defined(__GLIBC__) && (!defined(_FORTIFY_SOURCE) || (_FORTIFY_SOURCE < 1))


int sprintf(char *str, const char *format, ...) \
    FUNC_DEPRECATED("Try snprintf() or xsnprintf() or xasprintf()");

#endif  /* __linux__ && __GLIBC__ */


#endif  /* CFENGINE_DEPRECATED_H */
