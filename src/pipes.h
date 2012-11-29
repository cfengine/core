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

#ifndef CFENGINE_PIPES_H
#define CFENGINE_PIPES_H

#include "cf3.defs.h"

FILE *cf_popen(const char *command, char *type);
FILE *cf_popensetuid(const char *command, char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv, int background);
FILE *cf_popen_sh(const char *command, char *type);
FILE *cf_popen_shsetuid(const char *command, char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv, int background);
int cf_pclose(FILE *pp);
int cf_pclose_def(FILE *pfp, Attributes a, Promise *pp);
bool PipeToPid(pid_t *pid, FILE *pp);
int VerifyCommandRetcode(int retcode, int fallback, Attributes a, Promise *pp);

#ifndef MINGW
int cf_pwait(pid_t pid);
#endif /* NOT MINGW */

#endif
