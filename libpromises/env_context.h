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

#ifndef CFENGINE_ENV_CONTEXT_H
#define CFENGINE_ENV_CONTEXT_H

#include "cf3.defs.h"

#include "alphalist.h"
#include "writer.h"
#include "set.h"

struct EvalContext_
{
    StringSet *heap_soft;
    StringSet *heap_hard;
};

/**
  Negated classes
  Classes may be negated by using the command line option ‘-N’ or by being cancelled during
  exit status of a classes body: cancel ̇kept etc.
  */
extern Item *VNEGHEAP;

/**
  The bundle heap
  Classes are added to a local bundle heap using NewBundleClass().
  */
extern AlphaList VADDCLASSES;

/**
  List of classes that, if defined by a bundle, will cause the bundle to abort
  */
extern Item *ABORTBUNDLEHEAP;


EvalContext *EvalContextNew(void);
void EvalContextDestroy(EvalContext *ctx);

void EvalContextHeapAddSoft(EvalContext *ctx, const char *context);
void EvalContextHeapAddHard(EvalContext *ctx, const char *context);

bool EvalContextHeapContainsSoft(EvalContext *ctx, const char *context);
bool EvalContextHeapContainsHard(EvalContext *ctx, const char *context);

bool EvalContextHeapRemoveSoft(EvalContext *ctx, const char *context);
bool EvalContextHeapRemoveHard(EvalContext *ctx, const char *context);

void EvalContextHeapClear(EvalContext *ctx);

size_t EvalContextHeapMatchCountSoft(const EvalContext *ctx, const char *context_regex);
size_t EvalContextHeapMatchCountHard(const EvalContext *ctx, const char *context_regex);

StringSetIterator EvalContextHeapIteratorSoft(const EvalContext *ctx);
StringSetIterator EvalContextHeapIteratorHard(const EvalContext *ctx);


/* - Parsing/evaluating expressions - */
void ValidateClassSyntax(const char *str);
bool IsDefinedClass(EvalContext *ctx, const char *context, const char *ns);
bool IsExcluded(EvalContext *ctx, const char *exception, const char *ns);

bool EvalProcessResult(EvalContext *ctx, const char *process_result, AlphaList *proc_attr);
bool EvalFileResult(EvalContext *ctx, const char *file_result, AlphaList *leaf_attr);


// Add new contexts
void NewPersistentContext(char *name, const char *ns, unsigned int ttl_minutes, ContextStatePolicy policy);
void AddAbortClass(const char *name, const char *classes);
void NewClass(EvalContext *ctx, const char *oclass, const char *ns);      /* Copies oclass */
void NewBundleClass(EvalContext *ctx, const char *oclass, const char *bundle, const char *ns);
void AddAllClasses(EvalContext *ctx, const char *ns, const Rlist *list, bool persist, ContextStatePolicy policy, ContextScope context_scope);
void HardClass(EvalContext *ctx, const char *oclass);
void NewClassesFromString(EvalContext *ctx, const char *classlist);
void AddEphemeralClasses(EvalContext *ctx, const Rlist *classlist, const char *ns);
void NegateClassesFromString(EvalContext *ctx, const char *classlist);
void LoadPersistentContext(EvalContext *ctx);

// Remove contexts
void DeleteClass(EvalContext *ctx, const char *oclass, const char *ns);
void DeleteAllClasses(EvalContext *ctx, const Rlist *list);
void DeletePrivateClassContext(void);
void DeletePersistentContext(const char *name);

/* - Rest - */
int Abort(void);
void KeepClassContextPromise(EvalContext *ctx, Promise *pp);
void PushPrivateClassContext(int inherit);
void PopPrivateClassContext(void);
Rlist *SplitContextExpression(const char *context, Promise *pp);
int VarClassExcluded(EvalContext *ctx, Promise *pp, char **classes);
bool IsSoftClass(EvalContext *ctx, const char *sp);
bool IsTimeClass(const char *sp);
void SaveClassEnvironment(EvalContext *ctx);
void ListAlphaList(Writer *writer, AlphaList al, char sep);
void MarkPromiseHandleDone(EvalContext *ctx, const Promise *pp);
int MissingDependencies(EvalContext *ctx, const Promise *pp);

#endif
