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
};

Policy *PolicyNew(void);
int PolicyCompare(const void *a, const void *b);
void PolicyDestroy(Policy *policy);

/**
 * @brief Merge two partial policy objects. The memory for the child objects of the original policies are transfered to the new parent.
 * @param a
 * @param b
 * @return Merged policy
 */
Policy *PolicyMerge(Policy *a, Policy *b);

/**
 * @brief Query a policy for a body
 * @param policy The policy to query
 * @param ns Namespace filter (optionally NULL)
 * @param type Body type filter
 * @param name Body name filter
 * @return Body child object if found, otherwise NULL
 */
Body *PolicyGetBody(const Policy *policy, const char *ns, const char *type, const char *name);

/**
 * @brief Check to see if a policy is runnable (contains body common control)
 * @param policy Policy to check
 * @return True if policy is runnable
 */
bool PolicyIsRunnable(const Policy *policy);

/**
 * @brief Convenience function to get the policy object associated with a promise
 * @param promise
 * @return Policy object
 */
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

bool PolicyCheckPartial(const Policy *policy, Seq *errors);
bool PolicyCheckRunnable(const Policy *policy, Seq *errors);

Bundle *PolicyAppendBundle(Policy *policy, const char *ns, const char *name, const char *type, Rlist *args, const char *source_path);
Body *PolicyAppendBody(Policy *policy, const char *ns, const char *name, const char *type, Rlist *args, const char *source_path);

SubType *BundleAppendSubType(Bundle *bundle, char *name);
SubType *BundleGetSubType(Bundle *bp, const char *name);

const char *NamespaceFromConstraint(const Constraint *cp);

Promise *SubTypeAppendPromise(SubType *type, char *promiser, Rval promisee, char *classes, char *bundle, char *bundletype, char *ns);
void PromiseDestroy(Promise *pp);

Constraint *PromiseAppendConstraint(Promise *promise, const char *lval, Rval rval, const char *classes, bool references_body);
Constraint *BodyAppendConstraint(Body *body, const char *lval, Rval rval, const char *classes, bool references_body);


// TODO: legacy

Bundle *GetBundle(const Policy *policy, const char *name, const char *agent);


#endif
