/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_MUTEX_H
#define CFENGINE_MUTEX_H

#include "platform.h"

extern pthread_mutex_t *cft_system;
extern pthread_mutex_t *cft_count;
extern pthread_mutex_t *cft_getaddr;
extern pthread_mutex_t *cft_lock;
extern pthread_mutex_t *cft_output;
extern pthread_mutex_t *cft_vscope;
extern pthread_mutex_t *cft_server_children;

int ThreadLock(pthread_mutex_t *name);
int ThreadUnlock(pthread_mutex_t *name);

#endif
