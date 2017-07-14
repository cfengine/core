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

#include <cf-serverd-enterprise-stubs.h>

#include <server.h>
#include <cf-serverd-functions.h>

ENTERPRISE_VOID_FUNC_3ARG_DEFINE_STUB(void, RegisterLiteralServerData,
                                      ARG_UNUSED EvalContext *, ctx,
                                      ARG_UNUSED const char *, handle,
                                      ARG_UNUSED const Promise *, pp)
{
    Log(LOG_LEVEL_VERBOSE, "Access to server literals is only available in CFEngine Enterprise");
}

ENTERPRISE_FUNC_3ARG_DEFINE_STUB(int, ReturnLiteralData, ARG_UNUSED EvalContext *, ctx, ARG_UNUSED char *, handle, ARG_UNUSED char *, ret)
{
    Log(LOG_LEVEL_VERBOSE, "Access to server literals is only available in CFEngine Enterprise");
    return 0;
}

ENTERPRISE_FUNC_4ARG_DEFINE_STUB(int, SetServerListenState, ARG_UNUSED EvalContext *, ctx, ARG_UNUSED size_t, queue_size, ARG_UNUSED bool, server_listen,
                                 InitServerFunction, InitServerPtr)
{
    if (!server_listen)
    {
        Log(LOG_LEVEL_VERBOSE, "Disable listening on port is only supported in CFEngine Enterprise");
    }

    return InitServerPtr(queue_size);
}

ENTERPRISE_FUNC_1ARG_DEFINE_STUB(bool, ReceiveCollectCall, ARG_UNUSED ServerConnectionState *, conn)
{
    return false;
}

ENTERPRISE_FUNC_3ARG_DEFINE_STUB(bool, ReturnQueryData, ARG_UNUSED ServerConnectionState *, conn, ARG_UNUSED char *, menu, ARG_UNUSED int, encrypt)
{
    return false;
}

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, KeepReportDataSelectAccessPromise,
                                      ARG_UNUSED const Promise *, pp)
{
    Log(LOG_LEVEL_ERR, "Report data select is only available in CFEngine Enterprise");
}

ENTERPRISE_VOID_FUNC_0ARG_DEFINE_STUB(void, CleanReportBookFilterSet)
{
    return;
}

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, CollectCallStart, ARG_UNUSED int, interval)
{
}

ENTERPRISE_VOID_FUNC_0ARG_DEFINE_STUB(void, CollectCallStop)
{
}

ENTERPRISE_FUNC_0ARG_DEFINE_STUB(bool, CollectCallHasPending)
{
    return false;
}

ENTERPRISE_FUNC_1ARG_DEFINE_STUB(int, CollectCallGetPending, ARG_UNUSED int *, queue_length)
{
    return -1;
}

ENTERPRISE_VOID_FUNC_0ARG_DEFINE_STUB(void, CollectCallMarkProcessed)
{
}

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, FprintAvahiCfengineTag, FILE *, fp)
{
    fprintf(fp,"<name replace-wildcards=\"yes\" >CFEngine Community %s Policy Server on %s </name>\n", Version(), "%h");
}
