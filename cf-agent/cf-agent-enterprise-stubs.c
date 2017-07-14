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

#include <cf-agent-enterprise-stubs.h>

ENTERPRISE_FUNC_8ARG_DEFINE_STUB(PromiseResult, LogFileChange,
                                 ARG_UNUSED EvalContext *, ctx,
                                 ARG_UNUSED const char *, file,
                                 ARG_UNUSED int, change,
                                 ARG_UNUSED Attributes, a,
                                 ARG_UNUSED const Promise *, pp,
                                 ARG_UNUSED CopyRegularFileFunction, CopyRegularFilePtr,
                                 ARG_UNUSED const char *, destination,
                                 ARG_UNUSED DeleteCompressedArrayFunction, DeleteCompressedArrayPtr)
{
    Log(LOG_LEVEL_VERBOSE, "Logging file differences requires version Nova or above");
    return PROMISE_RESULT_NOOP;
}

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, Nova_TrackExecution, ARG_UNUSED const char *, input_file)
{
}

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, GenerateReports, 
                                      ARG_UNUSED const GenericAgentConfig *, config, 
                                      ARG_UNUSED const EvalContext *, ctx)
{
}

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, Nova_NoteAgentExecutionPerformance,
                                      ARG_UNUSED const char *, input_file,
                                      ARG_UNUSED struct timespec, start)
{
}
