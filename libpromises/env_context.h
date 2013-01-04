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

/**
  The global heap
  Classes are added to the global heap using NewClass().
  */
extern AlphaList VHEAP;
extern AlphaList VHARDHEAP;

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

/* - Parsing/evaluating expressions - */
void ValidateClassSyntax(const char *str);
bool IsDefinedClass(const char *class, const char *ns);
bool IsExcluded(const char *exception, const char *ns);

bool EvalProcessResult(const char *process_result, AlphaList *proc_attr);
bool EvalFileResult(const char *file_result, AlphaList *leaf_attr);

/* - Rest - */
int Abort(void);
void AddAbortClass(const char *name, const char *classes);
void KeepClassContextPromise(Promise *pp);
void PushPrivateClassContext(int inherit);
void PopPrivateClassContext(void);
void DeletePrivateClassContext(void);
void DeleteEntireHeap(void);
void NewPersistentContext(char *name, char *namespace, unsigned int ttl_minutes, enum statepolicy policy);
void DeletePersistentContext(const char *name);
void LoadPersistentContext(void);
void AddEphemeralClasses(const Rlist *classlist, const char *ns);
void HardClass(const char *oclass);
void DeleteHardClass(const char *oclass);
void NewClass(const char *oclass, const char *namespace);      /* Copies oclass */
void NewBundleClass(const char *oclass, const char *bundle, const char *namespace);
Rlist *SplitContextExpression(const char *context, Promise *pp);
void DeleteClass(const char *oclass, const char *namespace);
int VarClassExcluded(Promise *pp, char **classes);
void NewClassesFromString(const char *classlist);
void NegateClassesFromString(const char *classlist);
bool IsSoftClass(const char *sp);
bool IsHardClass(const char *sp);
bool IsTimeClass(const char *sp);
void SaveClassEnvironment(void);
void DeleteAllClasses(const Rlist *list);
void AddAllClasses(char *namespace, const Rlist *list, int persist, enum statepolicy policy);
void ListAlphaList(Writer *writer, AlphaList al, char sep);
void MarkPromiseHandleDone(const Promise *pp);
int MissingDependencies(const Promise *pp);

#endif
