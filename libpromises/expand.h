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

#ifndef CFENGINE_EXPAND_H
#define CFENGINE_EXPAND_H

#include "cf3.defs.h"

typedef void PromiseActuator(EvalContext *ctx, Promise *pp, void *param);

void CommonEvalPromise(EvalContext *ctx, Promise *pp, void *param);

void ExpandPromise(EvalContext *ctx, Promise *pp, PromiseActuator *ActOnPromise, void *param);

Rval ExpandDanglers(EvalContext *ctx, const char *scope, Rval rval, const Promise *pp);
void MapIteratorsFromRval(EvalContext *ctx, const char *scope, Rlist **lol, Rlist **los, Rval rval);

int IsExpandable(const char *str);

bool ExpandScalar(const EvalContext *ctx, const char *scope, const char *string, char buffer[CF_EXPANDSIZE]);
Rval ExpandBundleReference(EvalContext *ctx, const char *scopeid, Rval rval);
Rval ExpandPrivateRval(EvalContext *ctx, const char *contextid, Rval rval);
Rlist *ExpandList(EvalContext *ctx, const char *scopeid, const Rlist *list, int expandnaked);
Rval EvaluateFinalRval(EvalContext *ctx, const char *scopeid, Rval rval, int forcelist, const Promise *pp);
int IsNakedVar(const char *str, char vtype);
/**
  @brief Takes a variable and removes decorations.

  This function performs no validations, it is necessary to call the validation functions before calling this function.
  @remarks This function does not check for NULL pointers, that is the caller's responsability.
  @param s1 Buffer to store the undecorated variable.
  @param s2 Decorated variable
  */
void GetNaked(char *s1, const char *s2);
/**
  @brief Checks if a given variable is a list or not.
  @remarks This function does not check for NULL pointers, it is responsability of the caller.
  @param variable Variable to be checked
  @return True if the variable is a list, False otherwise.
  */
bool IsVarList(const char *var);

#endif
