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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_EXEC_CONFIG_H
#define CFENGINE_EXEC_CONFIG_H

#include "cf3.defs.h"
#include "set.h"

typedef struct
{
    bool scheduled_run;
    char *exec_command;
    int agent_expireafter;

    char *mail_server;
    char *mail_from_address;
    char *mail_to_address;
    int mail_max_lines;

    StringSet *schedule;
    int splay_time;
    char *log_facility;

    /*
     * Host information.
     * Might change during policy reload, so copy is retained in each worker.
     */
    char *fq_name;
    char *ip_address;
} ExecConfig;

ExecConfig *ExecConfigNewDefault(bool scheduled_run, const char *fq_name, const char *ip_address);
ExecConfig *ExecConfigCopy(const ExecConfig *exec_config);
void ExecConfigDestroy(ExecConfig *exec_config);

void ExecConfigUpdate(const EvalContext *ctx, const Policy *policy, ExecConfig *exec_config);

#endif
