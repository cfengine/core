/*
   Copyright 2017 Northern.tech AS

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

#include <platform.h>
#include <alloc.h>
#include <atexit.h>

#if defined(__MINGW32__)

typedef struct AtExitList
{
    AtExitFn fn;
    struct AtExitList *next;
} AtExitList;

static pthread_mutex_t atexit_functions_mutex = PTHREAD_MUTEX_INITIALIZER;
static AtExitList *atexit_functions;

/* To be called externally only by Windows service implementation */

void CallAtExitFunctions(void)
{
    pthread_mutex_lock(&atexit_functions_mutex);

    AtExitList *p = atexit_functions;
    while (p)
    {
        AtExitList *cur = p;
        (cur->fn)();
        p = cur->next;
        free(cur);
    }

    atexit_functions = NULL;

    pthread_mutex_unlock(&atexit_functions_mutex);
}

#endif

void RegisterAtExitFunction(AtExitFn fn)
{
#if defined(__MINGW32__)
    pthread_mutex_lock(&atexit_functions_mutex);

    AtExitList *p = xmalloc(sizeof(AtExitList));
    p->fn = fn;
    p->next = atexit_functions;

    atexit_functions = p;

    pthread_mutex_unlock(&atexit_functions_mutex);
#endif

    atexit(fn);
}
