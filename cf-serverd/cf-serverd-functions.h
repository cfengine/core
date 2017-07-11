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

#ifndef CFENGINE_CF_SERVERD_FUNCTIONS_H
#define CFENGINE_CF_SERVERD_FUNCTIONS_H


#include <platform.h>

#include <generic_agent.h>
#include <server.h>

#include <eval_context.h>
#include <dir.h>
#include <dbm_api.h>
#include <lastseen.h>
#include <crypto.h>
#include <files_names.h>
#include <vars.h>
#include <promises.h>
#include <item_lib.h>
#include <conversion.h>
#include <xml_writer.h>
#include <pipes.h>


typedef int (*InitServerFunction)(size_t queue_size);


GenericAgentConfig *CheckOpts(int argc, char **argv);
int StartServer(EvalContext *ctx, Policy **policy, GenericAgentConfig *config);


#endif
