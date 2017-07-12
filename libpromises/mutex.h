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

#ifndef CFENGINE_MUTEX_H
#define CFENGINE_MUTEX_H

#include <platform.h>

extern pthread_mutex_t *cft_lock;
extern pthread_mutex_t *cft_count;
extern pthread_mutex_t *cft_getaddr;
extern pthread_mutex_t *cft_server_children;
extern pthread_mutex_t *cft_server_filter;

#define ThreadLock(m)     __ThreadLock(m, __func__, __FILE__, __LINE__)
#define ThreadUnlock(m) __ThreadUnlock(m, __func__, __FILE__, __LINE__)

int __ThreadLock(pthread_mutex_t *name,
                 const char *funcname, const char *filename, int lineno);
int __ThreadUnlock(pthread_mutex_t *name,
                   const char *funcname, const char *filename, int lineno);

#endif
