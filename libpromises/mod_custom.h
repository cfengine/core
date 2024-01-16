/*
  Copyright 2024 Northern.tech AS

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

#ifndef CFENGINE_MOD_CUSTOM_H
#define CFENGINE_MOD_CUSTOM_H

#include <cf3.defs.h>
#include <pipes.h> // IOData

// mod_custom is not like the other modules which define promise types
// (except for mod_common). It just defines some basics needed, for
// custom promise types (promise modules) to work.

// Defines the syntax for the top level promise block, i.e. what
// constraints are allowed when adding new promises via promise modules
// Uses BodySyntax, since promise blocks are syntatctically the same
// as body blocks. (bundles are different, with sections and
// promise attributes).
extern const BodySyntax CUSTOM_PROMISE_BLOCK_SYNTAX;

// Defines custom promise block with no syntax, as syntax will be checked by
// the custom promise module.
extern const BodySyntax CUSTOM_BODY_BLOCK_SYNTAX;

typedef struct PromiseModule
{
    pid_t pid;
    time_t process_start_time;
    IOData fds;
    FILE *input;
    FILE *output;
    char *path;
    char *interpreter;
    bool json;
    bool action_policy;
    JsonElement *message;
} PromiseModule;

bool InitializeCustomPromises();
void FinalizeCustomPromises();
void TerminateCustomPromises();

Body *FindCustomPromiseType(const Promise *promise);
PromiseResult EvaluateCustomPromise(ARG_UNUSED EvalContext *ctx, const Promise *pp);

#endif
