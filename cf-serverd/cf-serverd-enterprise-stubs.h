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

#ifndef CFENGINE_CF_SERVERD_ENTERPRISE_STUBS_H
#define CFENGINE_CF_SERVERD_ENTERPRISE_STUBS_H

#include <cf3.defs.h>
#include <cf-serverd-functions.h>

struct ServerConnectionState;

ENTERPRISE_VOID_FUNC_3ARG_DECLARE(void, RegisterLiteralServerData, EvalContext *, ctx, const char *, handle, const Promise *, pp);
ENTERPRISE_FUNC_3ARG_DECLARE(int, ReturnLiteralData, EvalContext *, ctx, char *, handle, char *, ret);

ENTERPRISE_FUNC_4ARG_DECLARE(int, SetServerListenState, EvalContext *, ctx, size_t, queue_size, bool, server_listen,
                             InitServerFunction, InitServerPtr);

typedef void (*ServerEntryPointFunction)(EvalContext *ctx, char *ipaddr, ConnectionInfo *info);
ENTERPRISE_FUNC_1ARG_DECLARE(bool, ReceiveCollectCall, ServerConnectionState *, conn);

ENTERPRISE_FUNC_3ARG_DECLARE(bool, ReturnQueryData, ServerConnectionState *, conn, char *, menu, int, encrypt);

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, KeepReportDataSelectAccessPromise,
                                  const Promise *, pp);
ENTERPRISE_VOID_FUNC_0ARG_DECLARE(void, CleanReportBookFilterSet);

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, FprintAvahiCfengineTag, FILE *, fp);

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, CollectCallStart, ARG_UNUSED int, interval);
ENTERPRISE_VOID_FUNC_0ARG_DECLARE(void, CollectCallStop);
ENTERPRISE_FUNC_0ARG_DECLARE(bool, CollectCallHasPending);
ENTERPRISE_FUNC_1ARG_DECLARE(int, CollectCallGetPending, ARG_UNUSED int *, queue_length);
ENTERPRISE_VOID_FUNC_0ARG_DECLARE(void, CollectCallMarkProcessed);

#endif
