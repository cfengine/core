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

#ifndef CFENGINE_PIPES_H
#define CFENGINE_PIPES_H

#include <cf3.defs.h>

typedef struct
{
    int write_fd;
    int read_fd;
} IOData;

IOData cf_popen_full_duplex(const char *command, bool capture_stderr);
int cf_pclose_full_duplex(IOData *data);
int cf_pclose_full_duplex_side(int fd);

FILE *cf_popen(const char *command, const char *type, bool capture_stderr);
FILE *cf_popensetuid(const char *command, const char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv, int background);
FILE *cf_popen_sh(const char *command, const char *type);
FILE *cf_popen_shsetuid(const char *command, const char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv, int background);
int cf_pclose(FILE *pp);
bool PipeToPid(pid_t *pid, FILE *pp);
bool PipeTypeIsOk(const char *type);

#ifdef __MINGW32__
FILE *cf_popen_powershell(const char *command, const char *type);
FILE *cf_popen_powershell_setuid(const char *command, const char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv,
                              int background);
#endif

#endif
