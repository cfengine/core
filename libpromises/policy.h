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

#ifndef CFENGINE_POLICY_H
#define CFENGINE_POLICY_H

#include <cf3.defs.h>

#include <writer.h>
#include <sequence.h>
#include <json.h>
#include <set.h>

typedef enum
{
    POLICY_ELEMENT_TYPE_POLICY,
    POLICY_ELEMENT_TYPE_BUNDLE,
    POLICY_ELEMENT_TYPE_BODY,
    POLICY_ELEMENT_TYPE_PROMISE_TYPE,
    POLICY_ELEMENT_TYPE_PROMISE,
    POLICY_ELEMENT_TYPE_CONSTRAINT
} PolicyElementType;

typedef struct
{
    PolicyElementType type;
    const void *subject;
    char *message;
} PolicyError;

struct Policy_
{
    char *release_id;

    Seq *bundles;
    Seq *bodies;
};

typedef struct
{
    size_t start;
    size_t end;
    size_t line;
    size_t context;
} SourceOffset;

struct Bundle_
{
    Policy *parent_policy;

    char *type;
    char *name;
    char *ns;
    Rlist *args;

    Seq *promise_types;

    char *source_path;
    SourceOffset offset;
};

struct Body_
{
    Policy *parent_policy;

    char *type;
    char *name;
    char *ns;
    Rlist *args;

    Seq *conlist;

    char *source_path;
    SourceOffset offset;
};

struct PromiseType_
{
    Bundle *parent_bundle;

    char *name;
    Seq *promises;

    SourceOffset offset;
};

struct Promise_
{
    PromiseType *parent_promise_type;

    char *classes;
    char *comment;
    char *promiser;
    Rval promisee;
    Seq *conlist;

    const Promise *org_pp;            /* A ptr to the unexpanded raw promise */

    SourceOffset offset;
};

struct Constraint_
{
    PolicyElementType type;
    union {
        Promise *promise;
        Body *body;
    } parent;

    char *lval;
    Rval rval;

    char *classes;
    bool references_body;

    SourceOffset offset;
};

const char *NamespaceDefault(void);

Policy *PolicyNew(void);
int PolicyCompare(const void *a, const void *b);
void PolicyDestroy(Policy *policy);
unsigned PolicyHash(const Policy *policy);

StringSet *PolicySourceFiles(const Policy *policy);

Policy *PolicyMerge(Policy *a, Policy *b);
Body *PolicyGetBody(const Policy *policy, const char *ns, const char *type, const char *name);
Bundle *PolicyGetBundle(const Policy *policy, const char *ns, const char *type, const char *name);
bool PolicyIsRunnable(const Policy *policy);
const Policy *PolicyFromPromise(const Promise *promise);
char *BundleQualifiedName(const Bundle *bundle);

PolicyError *PolicyErrorNew(PolicyElementType type, const void *subject, const char *error_msg, ...);
void PolicyErrorDestroy(PolicyError *error);
void PolicyErrorWrite(Writer *writer, const PolicyError *error);

bool PolicyCheckPartial(const Policy *policy, Seq *errors);
bool PolicyCheckRunnable(const EvalContext *ctx, const Policy *policy, Seq *errors, bool ignore_missing_bundles);

Bundle *PolicyAppendBundle(Policy *policy, const char *ns, const char *name, const char *type, const Rlist *args, const char *source_path);
Body *PolicyAppendBody(Policy *policy, const char *ns, const char *name, const char *type, Rlist *args, const char *source_path);

JsonElement *PolicyToJson(const Policy *policy);
JsonElement *BundleToJson(const Bundle *bundle);
JsonElement *BodyToJson(const Body *body);
Policy *PolicyFromJson(JsonElement *json_policy);
void PolicyToString(const Policy *policy, Writer *writer);

PromiseType *BundleAppendPromiseType(Bundle *bundle, const char *name);
const PromiseType *BundleGetPromiseType(const Bundle *bp, const char *name);

Constraint *BodyAppendConstraint(Body *body, const char *lval, Rval rval, const char *classes, bool references_body);
Seq *BodyGetConstraint(Body *body, const char *lval);
bool BodyHasConstraint(const Body *body, const char *lval);

const char *ConstraintGetNamespace(const Constraint *cp);

Promise *PromiseTypeAppendPromise(PromiseType *type, const char *promiser, Rval promisee, const char *classes, const char *varclasses);
void PromiseTypeDestroy(PromiseType *promise_type);

void PromiseDestroy(Promise *pp);

Constraint *PromiseAppendConstraint(Promise *promise, const char *lval, Rval rval, bool references_body);

const char *PromiseGetNamespace(const Promise *pp);
const Bundle *PromiseGetBundle(const Promise *pp);
const Policy *PromiseGetPolicy(const Promise *pp);

void PromisePath(Writer *w, const Promise *pp);
const char *PromiseGetHandle(const Promise *pp);
int PromiseGetConstraintAsInt(const EvalContext *ctx, const char *lval, const Promise *pp);
bool PromiseGetConstraintAsReal(const EvalContext *ctx, const char *lval, const Promise *list, double *value_out);
mode_t PromiseGetConstraintAsOctal(const EvalContext *ctx, const char *lval, const Promise *list);
uid_t PromiseGetConstraintAsUid(const EvalContext *ctx, const char *lval, const Promise *pp);
gid_t PromiseGetConstraintAsGid(const EvalContext *ctx, char *lval, const Promise *pp);
Rlist *PromiseGetConstraintAsList(const EvalContext *ctx, const char *lval, const Promise *pp);
int PromiseGetConstraintAsBoolean(const EvalContext *ctx, const char *lval, const Promise *list);
Constraint *PromiseGetConstraintWithType(const Promise *promise, const char *lval, RvalType type);
Constraint *PromiseGetImmediateConstraint(const Promise *promise, const char *lval);
void *PromiseGetConstraintAsRval(const Promise *promise, const char *lval, RvalType type);
Constraint *PromiseGetConstraint(const Promise *promise, const char *lval);

bool PromiseBundleOrBodyConstraintExists(const EvalContext *ctx, const char *lval, const Promise *pp);

void PromiseRecheckAllConstraints(const EvalContext *ctx, const Promise *pp);

void ConstraintDestroy(Constraint *cp);
int ConstraintsGetAsBoolean(const EvalContext *ctx, const char *lval, const Seq *constraints);
const char *ConstraintContext(const Constraint *cp);
Constraint *EffectiveConstraint(const EvalContext *ctx, Seq *constraints);

void *PromiseGetImmediateRvalValue(const char *lval, const Promise *pp, RvalType rtype);

char *QualifiedNameNamespaceComponent(const char *qualified_name);
char *QualifiedNameScopeComponent(const char *qualified_name);
bool BundleTypeCheck(const char *name);
Rval DefaultBundleConstraint(const Promise *pp, char *promisetype);

#endif
