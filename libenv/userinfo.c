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

#include <userinfo.h>
#include <stdio.h>

#ifndef _WIN32
#include <sys/types.h>
#include <pwd.h>
#endif

#include <eval_context.h>

void GetCurrentUserInfo(EvalContext *ctx)
{
#ifndef _WIN32
    struct passwd *pw = getpwuid(getuid());
    if (pw == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to get username for uid '%ju'. (getpwuid: %s)", (uintmax_t)getuid(), GetErrorStr());
    }
    else
    {
        char buf[CF_BUFSIZE];

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "name", pw->pw_name, CF_DATA_TYPE_STRING, "source=agent");
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "homedir", pw->pw_dir, CF_DATA_TYPE_STRING, "source=agent");
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "shell", pw->pw_shell, CF_DATA_TYPE_STRING, "source=agent");

        snprintf(buf, CF_BUFSIZE, "%i", pw->pw_uid);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "uid", buf, CF_DATA_TYPE_INT, "source=agent");

        snprintf(buf, CF_BUFSIZE, "%i", pw->pw_gid);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_USER, "gid", buf, CF_DATA_TYPE_INT, "source=agent");
    }
#endif
}
