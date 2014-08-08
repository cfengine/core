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

#ifndef CFENGINE_SYSINFO_H
#define CFENGINE_SYSINFO_H

#include <cf3.defs.h>

void DetectEnvironment(EvalContext *ctx);

void CreateHardClassesFromCanonification(EvalContext *ctx, const char *canonified, char *tags);
int GetUptimeMinutes(time_t now);
int GetUptimeSeconds(time_t now);
void DiscoverDocker(EvalContext *ctx);
DockerPS *QueryDockerProcessTable(DockerPS **containers);
void PrependDockerPS(DockerPS **containers, char *id, char *image, char *name, char *address);
void DeleteDockerPS(DockerPS *list);
int ExecEnvCommand(char *cmd, char *buffer);
void GetDockerIPForContainer(char *name, char *address);

#endif
