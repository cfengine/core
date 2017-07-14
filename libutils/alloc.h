/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_ALLOC_H
#define CFENGINE_ALLOC_H

#include <platform.h>
#include <compiler.h>

void *xcalloc(size_t nmemb, size_t size);
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);
char *xstrndup(const char *str, size_t n);
void *xmemdup(const void *mem, size_t size);
int xasprintf(char **strp, const char *fmt, ...) FUNC_ATTR_PRINTF(2, 3);
int xvasprintf(char **strp, const char *fmt, va_list ap) FUNC_ATTR_PRINTF(2, 0);

/*
 * Prevent any code from using un-wrapped allocators.
 *
 * Use x* equivalents instead.
 */

/**
 * Currently regular malloc() calls are allowed for mission-critical code that
 * can somehow recover, like cf-serverd dropping connections or cf-execd
 * postponing its scheduled actions.
 *
 * @note for 99% of the cases (libpromises, cf-agent etc) use xmalloc() and
 *       friends.
 **/
#if 0

# undef malloc
# undef calloc
# undef realloc
# undef strdup
# undef strndup
# undef memdup
# undef asprintf
# undef vasprintf
# define malloc __error_unchecked_malloc
# define calloc __error_unchecked_calloc
# define realloc __error_unchecked_realloc
# define strdup __error_unchecked_strdup
# define strndup __error_unchecked_strndup
# define memdup __error_unchecked_memdup
# define asprintf __error_unchecked_asprintf
# define vasprintf __error_unchecked_vasprintf

void __error_unchecked_malloc(void);
void __error_unchecked_calloc(void);
void __error_unchecked_realloc(void);
void __error_unchecked_strdup(void);
void __error_unchecked_strndup(void);
void __error_unchecked_memdup(void);
void __error_unchecked_asprintf(void);
void __error_unchecked_vasprintf(void);

#endif

#endif
