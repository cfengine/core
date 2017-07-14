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

#ifndef CFENGINE_EXPAND_H
#define CFENGINE_EXPAND_H

#include <cf3.defs.h>
#include <generic_agent.h>
#include <actuator.h>

PromiseResult CommonEvalPromise(EvalContext *ctx, const Promise *pp, void *param);

PromiseResult ExpandPromise(EvalContext *ctx, const Promise *pp, PromiseActuator *ActOnPromise, void *param);

Rval ExpandDanglers(EvalContext *ctx, const char *ns, const char *scope, Rval rval, const Promise *pp);
void MapIteratorsFromRval(EvalContext *ctx, const Bundle *bundle, Rval rval, Rlist **scalars, Rlist **lists, Rlist **containers);

bool IsExpandable(const char *str);

bool ExpandScalar(const EvalContext *ctx, const char *ns, const char *scope, const char *string, Buffer *out);
Rval ExpandBundleReference(EvalContext *ctx, const char *ns, const char *scope, Rval rval);
Rval ExpandPrivateRval(EvalContext *ctx, const char *ns, const char *scope, const void *rval_item, RvalType rval_type);
Rlist *ExpandList(EvalContext *ctx, const char *ns, const char *scope, const Rlist *list, int expandnaked);
Rval EvaluateFinalRval(EvalContext *ctx, const Policy *policy, const char *ns, const char *scope, Rval rval, bool forcelist, const Promise *pp);

/**
 * @brief BundleResolve
 * @param ctx
 * @param bundle
 */
void BundleResolve(EvalContext *ctx, const Bundle *bundle);
void PolicyResolve(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
void BundleResolvePromiseType(EvalContext *ctx, const Bundle *bundle, const char *type, PromiseActuator *actuator);

bool IsNakedVar(const char *str, char vtype);
/**
  @brief Takes a variable and removes decorations.

  This function performs no validations, it is necessary to call the validation functions before calling this function.
  @remarks This function does not check for NULL pointers, that is the caller's responsibility.
  @param s1 Buffer to store the undecorated variable.
  @param s2 Decorated variable
  */
void GetNaked(char *s1, const char *s2);
/**
  @brief Checks if a given variable is a list or not.
  @remarks This function does not check for NULL pointers, it is responsibility of the caller.
  @param variable Variable to be checked
  @return True if the variable is a list, False otherwise.
  */
bool IsVarList(const char *var);

ProtocolVersion ProtocolVersionParse(const char *s);

#endif
