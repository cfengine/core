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

#include "cf-serverd-enterprise-stubs.h"

#include "server.h"
#include "cf-serverd-functions.h"

void RegisterLiteralServerData(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const char *handle, ARG_UNUSED Promise *pp)
{
    Log(LOG_LEVEL_VERBOSE, "Access to server literals is only available in CFEngine Enterprise");
}

int ReturnLiteralData(ARG_UNUSED EvalContext *ctx, ARG_UNUSED char *handle, ARG_UNUSED char *ret)
{
    Log(LOG_LEVEL_VERBOSE, "Access to server literals is only available in CFEngine Enterprise");
    return 0;
}

int SetServerListenState(ARG_UNUSED EvalContext *ctx, ARG_UNUSED size_t queue_size)
{
    if (!SERVER_LISTEN)
    {
        Log(LOG_LEVEL_VERBOSE, "Disable listening on port is only supported in CFEngine Enterprise");
    }

    return InitServer(queue_size);
}

void TryCollectCall(void)
{
    Log(LOG_LEVEL_VERBOSE, "Collect calling is only supported in CFEngine Enterprise");
}

int ReceiveCollectCall(ARG_UNUSED struct ServerConnectionState *conn)
{
    Log(LOG_LEVEL_VERBOSE, "  Collect Call only supported in the CFEngine Enterprise");
    return false;
}

bool ReturnQueryData(ARG_UNUSED struct ServerConnectionState *conn, ARG_UNUSED char *menu)
{
    return false;
}

void KeepReportDataSelectAccessPromise(Promise *pp)
{
    Log(LOG_LEVEL_ERR, "Report data select is only available in CFEngine Enterprise");
}

void CleanReportBookFilterSet(void){}
