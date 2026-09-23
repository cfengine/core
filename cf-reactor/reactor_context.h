/*
  Copyright 2026 Northern.tech AS

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

#ifndef CFENGINE_REACTOR_CONTEXT_H
#define CFENGINE_REACTOR_CONTEXT_H

#include <platform.h>
#include <sequence.h>

typedef enum
{
  REACTOR_FD_NOVA,
  REACTOR_FD_WATCHER
} ReactorFdType;

/**
 * @brief Single file descriptor watched by daemon's select(2) loop as well as metadata of its origin
 */
typedef struct
{
  ReactorFdType type;
  int fd;
} ReactorFd;

/**
 * @brief Shared state for the cf-reactor daemon's single select(2) loop. fds is an array of ReactorFd
 */
typedef struct
{
  Seq *fds;
  fd_set readfds;
  size_t max_nova_fds;
} ReactorContext;

bool ReactorContextInitialize(ReactorContext *reactor_context);
int ReactorContextSetupFileDescriptors(ReactorContext *reactor_context);
void ReactorContextHandleEvents(ReactorContext *reactor_context, time_t *next_tick);
void ReactorContextFinalize(ReactorContext *reactor_context);

#endif
