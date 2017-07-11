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

/**
 * @brief Merge two partial policy objects. The memory for the child objects of the original policies are transferred to the new parent.
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
 * @brief Query a policy for a bundle
 * @param policy The policy to query
 * @param ns Namespace filter (optionally NULL)
 * @param type Bundle type filter
 * @param name Bundle name filter
 * @return Bundle child object if found, otherwise NULL
 */
Bundle *PolicyGetBundle(const Policy *policy, const char *ns, const char *type, const char *name);

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
const Policy *PolicyFromPromise(const Promise *promise);
char *BundleQualifiedName(const Bundle *bundle);

PolicyError *PolicyErrorNew(PolicyElementType type, const void *subject, const char *error_msg, ...);
void PolicyErrorDestroy(PolicyError *error);
void PolicyErrorWrite(Writer *writer, const PolicyError *error);

/**
 * @brief Check a partial policy DOM for errors
 * @param policy Policy to check
 * @param errors Sequence of PolicyError to append errors to
 * @return True if no new errors are found
 */
bool PolicyCheckPartial(const Policy *policy, Seq *errors);

/**
 * @brief Check a runnable policy DOM for errors
 * @param policy Policy to check
 * @param errors Sequence of PolicyError to append errors to
 * @param ignore_missing_bundles Whether to ignore missing bundle references
 * @return True if no new errors are found
 */
bool PolicyCheckRunnable(const EvalContext *ctx, const Policy *policy, Seq *errors, bool ignore_missing_bundles);

Bundle *PolicyAppendBundle(Policy *policy, const char *ns, const char *name, const char *type, const Rlist *args, const char *source_path);
Body *PolicyAppendBody(Policy *policy, const char *ns, const char *name, const char *type, Rlist *args, const char *source_path);

/**
 * @brief Serialize a policy as JSON
 * @param policy The policy to serialize
 * @return A JsonElement representing the input policy
 */
JsonElement *PolicyToJson(const Policy *policy);

/**
 * @brief Deserialize a policy from JSON
 * @param json_policy JSON to deserialize
 * @return A policy DOM
 */
Policy *PolicyFromJson(JsonElement *json_policy);

/**
 * @brief Pretty-print a policy
 * @param policy The policy to print
 * @param writer Writer to write into
 */
void PolicyToString(const Policy *policy, Writer *writer);

PromiseType *BundleAppendPromiseType(Bundle *bundle, const char *name);
const PromiseType *BundleGetPromiseType(const Bundle *bp, const char *name);

Constraint *BodyAppendConstraint(Body *body, const char *lval, Rval rval, const char *classes, bool references_body);

/**
 * @brief A sequence of constraints matching the l-value.
 * @param body Body to query
 * @param lval l-value to match
 * @return Sequence of pointers to the constraints. Destroying it does not alter the DOM.
 */
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

/**
 * @brief Write a string describing the promise location in policy, e.g. /default/foo/packages/'emacs'
 */
void PromisePath(Writer *w, const Promise *pp);

/**
 * @brief Return handle of the promise.
 * @param pp
 * @return Promise handle or NULL if no handle is provided
 */
const char *PromiseGetHandle(const Promise *pp);

/**
 * @brief Get the int value of the first effective constraint found matching, from a promise
 * @param lval
 * @param pp
 * @return Int value, or CF_NOINT
 */
int PromiseGetConstraintAsInt(const EvalContext *ctx, const char *lval, const Promise *pp);

/**
 * @brief Get the real value of the first effective constraint found matching, from a promise
 * @return true if value could be extracted
 */
bool PromiseGetConstraintAsReal(const EvalContext *ctx, const char *lval, const Promise *list, double *value_out);

/**
 * @brief Get the octal value of the first effective constraint found matching, from a promise
 * @param lval
 * @param list
 * @return Double value, or 077 if not found
 */
mode_t PromiseGetConstraintAsOctal(const EvalContext *ctx, const char *lval, const Promise *list);

/**
 * @brief Get the uid value of the first effective constraint found matching, from a promise
 * @param lval
 * @param pp
 * @return Uid value, or CF_SAME_OWNER if not found
 */
uid_t PromiseGetConstraintAsUid(const EvalContext *ctx, const char *lval, const Promise *pp);

/**
 * @brief Get the uid value of the first effective constraint found matching, from a promise
 * @param lval
 * @param pp
 * @return Gid value, or CF_SAME_GROUP if not found
 */
gid_t PromiseGetConstraintAsGid(const EvalContext *ctx, char *lval, const Promise *pp);

/**
 * @brief Get the Rlist value of the first effective constraint found matching, from a promise
 * @param lval
 * @param list
 * @return Rlist or NULL if not found (note: same as empty list)
 */
Rlist *PromiseGetConstraintAsList(const EvalContext *ctx, const char *lval, const Promise *pp);

bool PromiseBundleOrBodyConstraintExists(const EvalContext *ctx, const char *lval, const Promise *pp);

void PromiseRecheckAllConstraints(const EvalContext *ctx, const Promise *pp);

/**
 * @brief Get the trinary boolean value of the first effective constraint found matching, from a promise
 * @param lval
 * @param list
 * @return True/false, or CF_UNDEFINED if not found
 */
int PromiseGetConstraintAsBoolean(const EvalContext *ctx, const char *lval, const Promise *list);

/**
 * @brief Get the first effective constraint from the promise, also does some checking
 * @param promise
 * @param lval
 * @return Effective constraint if found, otherwise NULL
 */
Constraint *PromiseGetConstraint(const Promise *promise, const char *lval);
Constraint *PromiseGetConstraintWithType(const Promise *promise, const char *lval, RvalType type);

/**
 * @brief Get the first constraint from the promise. Checks that constraint does
 * not have any classes attached to it.
 *
 * Kill this function with fire once we have separated promise constraints and body constraints.
 *
 * @param promise
 * @param lval
 * @return Constraint if found, otherwise NULL
 */
Constraint *PromiseGetImmediateConstraint(const Promise *promise, const char *lval);


void ConstraintDestroy(Constraint *cp);



/**
 * @brief Get the context of the given constraint
 * @param cp
 * @return context. never returns NULL.
 */
const char *ConstraintContext(const Constraint *cp);

/**
 * @brief Returns the first effective constraint from a list of candidates, depending on evaluation state.
 * @param constraints The list of potential candidates
 * @return The effective constraint, or NULL if none are found.
 */
Constraint *EffectiveConstraint(const EvalContext *ctx, Seq *constraints);

/**
 * @brief Get the Rval value of the first effective constraint that matches the given type
 * @param lval
 * @param promise
 * @param type
 * @return Rval value if found, NULL otherwise
 */
void *PromiseGetConstraintAsRval(const Promise *promise, const char *lval, RvalType type);

/**
 * @brief Get the Rval value of the first constraint that matches the given
 * type. Checks that this constraint does not have any contexts attached.
 *
 * Kill this function with fire once we have separated body constraints and bundle constraints.
 *
 * @param lval
 * @param promise
 * @param type
 * @return Rval value if found, NULL otherwise
 */
void *PromiseGetImmediateRvalValue(const char *lval, const Promise *pp, RvalType rtype);


/**
 * @brief Get the trinary boolean value of the first effective constraint found matching
 * @param lval
 * @param constraints
 * @return True/false, or CF_UNDEFINED if not found
 */
int ConstraintsGetAsBoolean(const EvalContext *ctx, const char *lval, const Seq *constraints);


/**
 * @return A copy of the namespace component of a qualified name, or NULL. e.g. "foo:bar" -> "foo"
 */
char *QualifiedNameNamespaceComponent(const char *qualified_name);

/**
 * @return A copy of the symbol compoent of a qualified name, or NULL. e.g. "foo:bar" -> "bar"
 */
char *QualifiedNameScopeComponent(const char *qualified_name);

/**
 * @brief Check whether the promise type is allowed one
 */
bool BundleTypeCheck(const char *name);

/**
 * @brief Return a default bundle name for this method/service
 */

Rval DefaultBundleConstraint(const Promise *pp, char *promisetype);

#endif
