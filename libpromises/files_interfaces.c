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

#include <files_interfaces.h>

#include <eval_context.h>
#include <promises.h>
#include <dir.h>
#include <files_names.h>
#include <files_hashes.h>
#include <files_copy.h>
#include <item_lib.h>
#include <vars.h>
#include <matching.h>
#include <client_code.h>
#include <string_lib.h>
#include <rlist.h>
#include <stat_cache.h>                                      /* remote_stat */

int cf_lstat(const char *file, struct stat *buf, FileCopy fc, AgentConnection *conn)
{
    if (conn == NULL)
    {
        int ret = lstat(file, buf);
        if (ret == -1)
        {
            Log(LOG_LEVEL_ERR, "lstat: %s", GetErrorStr());
        }
        return ret;
    }
    else
    {
        assert(fc.servers && strcmp(fc.servers->val.item, "localhost"));
        return cf_remote_stat(conn, fc.encrypt, file, buf, "link");
    }
}
