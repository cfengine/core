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

#ifndef CFENGINE_POLICY_H
#define CFENGINE_POLICY_H

#include "cf3.defs.h"
#include "sequence.h"

struct Policy_
{
    Seq *bundles;
    Seq *bodies;
    char *current_namespace;
};

Policy *PolicyNew(void);
int PolicyCompare(const void *a, const void *b);
void PolicyDestroy(Policy *policy);

Policy *PolicyMerge(Policy *a, Policy *b);

Body *PolicyGetBody(Policy *policy, const char *ns, const char *type, const char *name);

Policy *PolicyFromPromise(const Promise *promise);
char *BundleQualifiedName(const Bundle *bundle);

typedef enum
{
    POLICY_ELEMENT_TYPE_BUNDLE,
    POLICY_ELEMENT_TYPE_BODY,
    POLICY_ELEMENT_TYPE_SUBTYPE,
    POLICY_ELEMENT_TYPE_PROMISE,
    POLICY_ELEMENT_TYPE_CONSTRAINT
} PolicyElementType;

typedef struct
{
    PolicyElementType type;
    const void *subject;
    char *message;
} PolicyError;

PolicyError *PolicyErrorNew(PolicyElementType type, const void *subject, const char *error_msg, ...);
void PolicyErrorDestroy(PolicyError *error);
void PolicyErrorWrite(Writer *writer, const PolicyError *error);
bool PolicyCheck(const Policy *policy, Seq *errors);

void PolicySetNameSpace(Policy *policy, char *namespace);
char *CurrentNameSpace(Policy *policy);

Bundle *AppendBundle(Policy *policy, const char *name, const char *type, Rlist *args, const char *source_path);
Body *AppendBody(Policy *policy, const char *name, const char *type, Rlist *args, const char *source_path);
SubType *AppendSubType(Bundle *bundle, char *typename);
Promise *AppendPromise(SubType *type, char *promiser, Rval promisee, char *classes, char *bundle, char *bundletype, char *namespace);


const char *NamespaceFromConstraint(const Constraint *cp);


// TODO: legacy
void DeletePromise(Promise *pp);
void DeletePromises(Promise *pp);
Bundle *GetBundle(const Policy *policy, const char *name, const char *agent);
SubType *GetSubTypeForBundle(char *type, Bundle *bp);



#endif
