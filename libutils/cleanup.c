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

#include <platform.h>
#include <alloc.h>
#include <cleanup.h>

typedef struct CleanupList
{
    CleanupFn fn;
    struct CleanupList *next;
} CleanupList;

static pthread_mutex_t cleanup_functions_mutex = PTHREAD_MUTEX_INITIALIZER;
static CleanupList *cleanup_functions;

/* To be called externally only by Windows binaries */
void CallCleanupFunctions(void)
{
    pthread_mutex_lock(&cleanup_functions_mutex);

    CleanupList *p = cleanup_functions;
    while (p)
    {
        CleanupList *cur = p;
        (cur->fn)();
        p = cur->next;
        free(cur);
    }

    cleanup_functions = NULL;

    pthread_mutex_unlock(&cleanup_functions_mutex);
}

void DoCleanupAndExit(int ret)
{
    CallCleanupFunctions();
    exit(ret);
}

void RegisterCleanupFunction(CleanupFn fn)
{
    pthread_mutex_lock(&cleanup_functions_mutex);

    CleanupList *p = xmalloc(sizeof(CleanupList));
    p->fn = fn;
    p->next = cleanup_functions;

    cleanup_functions = p;

    pthread_mutex_unlock(&cleanup_functions_mutex);
}

