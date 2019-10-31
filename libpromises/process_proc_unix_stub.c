/*
   Copyright 2019 Northern.tech AS

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

/* **********************************************************
 * Create a JSON representation of processes from "/proc".
 *
 * This dummy version is simply for linkage on not-yet-supported systems.
 *
 * The stub functions return values intended to indicate failure.
 *
 * (David Lee, 2019)
 ********************************************************** */

#include <processes_select.h>
#include <process_unix_priv.h>
#include <process_lib.h>

JsonElement *LoadProcStat(ARG_UNUSED pid_t pid)
{
    return NULL;
}

time_t LoadBootTime(void)
{
    return -1;
}
