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

#include <cf-monitord-enterprise-stubs.h>

ENTERPRISE_VOID_FUNC_0ARG_DEFINE_STUB(void, MonOtherInit)
{
}

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, MonOtherGatherData, ARG_UNUSED double *, cf_this)
{
}

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, HistoryUpdate, ARG_UNUSED EvalContext *, ctx, ARG_UNUSED Averages, newvals)
{
}

ENTERPRISE_FUNC_4ARG_DEFINE_STUB(PromiseResult, VerifyMeasurement,
                                 ARG_UNUSED EvalContext *, ctx,
                                 ARG_UNUSED double *, this,
                                 ARG_UNUSED Attributes, a,
                                 ARG_UNUSED const Promise *, pp)
{
    return PROMISE_RESULT_NOOP;
}
