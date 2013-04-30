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

#include "cf-serverd-enterprise-stubs.h"

#include "server.h"
#include "logging_old.h"
#include "cf-serverd-functions.h"

void RegisterLiteralServerData(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const char *handle, ARG_UNUSED Promise *pp)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "# Access to server literals is only available in CFEngine Enterprise\n");
}

int ReturnLiteralData(ARG_UNUSED EvalContext *ctx, ARG_UNUSED char *handle, ARG_UNUSED char *ret)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "# Access to server literals is only available in CFEngine Enterprise\n");
    return 0;
}

int SetServerListenState(ARG_UNUSED EvalContext *ctx, ARG_UNUSED size_t queue_size)
{
    if (!SERVER_LISTEN)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Disable listening on port is only supported in CFEngine Enterprise");
    }

    return InitServer(queue_size);
}

void TryCollectCall(void)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Collect calling is only supported in CFEngine Enterprise");
}

int ReceiveCollectCall(ARG_UNUSED struct ServerConnectionState *conn)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "  Collect Call only supported in the CFEngine Enterprise");
    return false;
}

bool ReturnQueryData(ARG_UNUSED struct ServerConnectionState *conn, ARG_UNUSED char *menu)
{
    return false;
}
