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

#ifndef CFENGINE_EVAL_CONTEXT_H
#define CFENGINE_EVAL_CONTEXT_H

#include <platform.h>

#include <writer.h>
#include <set.h>
#include <sequence.h>
#include <var_expressions.h>
#include <logic_expressions.h>
#include <scope.h>
#include <variable.h>
#include <class.h>
#include <iteration.h>
#include <rb-tree.h>
#include <ring_buffer.h>
#include <generic_agent.h>

typedef enum
{
    STACK_FRAME_TYPE_BUNDLE,
    STACK_FRAME_TYPE_BODY,
    STACK_FRAME_TYPE_BUNDLE_SECTION,
    STACK_FRAME_TYPE_PROMISE,
    STACK_FRAME_TYPE_PROMISE_ITERATION,
    STACK_FRAME_TYPE_MAX
} StackFrameType;

typedef struct
{
    const Bundle *owner;

    ClassTable *classes;
    VariableTable *vars;
} StackFrameBundle;

typedef struct
{
    const Body *owner;

    VariableTable *vars;
} StackFrameBody;

typedef struct
{
    const Promise *owner;

    VariableTable *vars;
} StackFramePromise;

typedef struct
{
    const BundleSection *owner;
} StackFrameBundleSection;

typedef struct
{
    Promise *owner;
    const PromiseIterator *iter_ctx;
    size_t index;
    RingBuffer *log_messages;
} StackFramePromiseIteration;

typedef struct
{
    StackFrameType type;
    bool inherits_previous; // whether or not this frame inherits context from the previous frame

    union
    {
        StackFrameBundle bundle;
        StackFrameBody body;
        StackFrameBundleSection bundle_section;
        StackFramePromise promise;
        StackFramePromiseIteration promise_iteration;
    } data;

    char *path;
    bool override_immutable;
} StackFrame;

typedef enum
{
    EVAL_OPTION_NONE = 0,

    EVAL_OPTION_EVAL_FUNCTIONS = 1 << 0,
    EVAL_OPTION_CACHE_SYSTEM_FUNCTIONS = 1 << 1,

    EVAL_OPTION_FULL = 0xFFFFFFFF
} EvalContextOption;

EvalContext *EvalContextNew(void);
void EvalContextDestroy(EvalContext *ctx);

void EvalContextSetConfig(EvalContext *ctx, const GenericAgentConfig *config);
const GenericAgentConfig *EvalContextGetConfig(EvalContext *ctx);

Rlist *EvalContextGetRestrictKeys(const EvalContext *ctx);
void EvalContextSetRestrictKeys(EvalContext *ctx, const Rlist *restrict_keys);

void EvalContextHeapAddAbort(EvalContext *ctx, const char *context, const char *activated_on_context);
void EvalContextHeapAddAbortCurrentBundle(EvalContext *ctx, const char *context, const char *activated_on_context);

void EvalContextHeapPersistentSave(EvalContext *ctx, const char *name, unsigned int ttl_minutes, PersistentClassPolicy policy, const char *tags);
void EvalContextHeapPersistentRemove(const char *context);
void EvalContextHeapPersistentLoadAll(EvalContext *ctx);

void EvalContextOverrideImmutableSet(EvalContext *ctx, bool should_override);
bool EvalContextOverrideImmutableGet(EvalContext *ctx);

/**
 * Sets negated classes (persistent classes that should not be defined).
 *
 * @note Takes ownership of #negated_classes
 */
void EvalContextSetNegatedClasses(EvalContext *ctx, StringSet *negated_classes);

bool EvalContextClassPutSoft(EvalContext *ctx, const char *name, ContextScope scope, const char *tags);
bool EvalContextClassPutSoftTagsSet(EvalContext *ctx, const char *name, ContextScope scope, StringSet *tags);
bool EvalContextClassPutSoftTagsSetWithComment(EvalContext *ctx, const char *name, ContextScope scope,
                                               StringSet *tags, const char *comment);
bool EvalContextClassPutSoftNS(EvalContext *ctx, const char *ns, const char *name,
                               ContextScope scope, const char *tags);
bool EvalContextClassPutSoftNSTagsSet(EvalContext *ctx, const char *ns, const char *name,
                                      ContextScope scope, StringSet *tags);
bool EvalContextClassPutSoftNSTagsSetWithComment(EvalContext *ctx, const char *ns, const char *name,
                                                 ContextScope scope, StringSet *tags, const char *comment);
bool EvalContextClassPutHard(EvalContext *ctx, const char *name, const char *tags);
Class *EvalContextClassGet(const EvalContext *ctx, const char *ns, const char *name);
Class *EvalContextClassMatch(const EvalContext *ctx, const char *regex);
bool EvalContextClassRemove(EvalContext *ctx, const char *ns, const char *name);
StringSet *EvalContextClassTags(const EvalContext *ctx, const char *ns, const char *name);

ClassTableIterator *EvalContextClassTableIteratorNewGlobal(const EvalContext *ctx, const char *ns, bool is_hard, bool is_soft);
ClassTableIterator *EvalContextClassTableIteratorNewLocal(const EvalContext *ctx);

// Class Logging
const StringSet *EvalContextAllClassesGet(const EvalContext *ctx);
void EvalContextAllClassesLoggingEnable(EvalContext *ctx, bool enable);

void EvalContextPushBundleName(const EvalContext *ctx, const char *bundle_name);
const StringSet *EvalContextGetBundleNames(const EvalContext *ctx);

void EvalContextPushRemoteVarPromise(EvalContext *ctx, const char *bundle_name, const Promise *pp);
const Seq *EvalContextGetRemoteVarPromises(const EvalContext *ctx, const char *bundle_name);

void EvalContextClear(EvalContext *ctx);

Rlist *EvalContextGetPromiseCallerMethods(EvalContext *ctx);

void EvalContextStackPushBundleFrame(EvalContext *ctx, const Bundle *owner, const Rlist *args, bool inherits_previous);
void EvalContextStackPushBodyFrame(EvalContext *ctx, const Promise *caller, const Body *body, const Rlist *args);
void EvalContextStackPushBundleSectionFrame(EvalContext *ctx, const BundleSection *owner);
void EvalContextStackPushPromiseFrame(EvalContext *ctx, const Promise *owner);
Promise *EvalContextStackPushPromiseIterationFrame(EvalContext *ctx, const PromiseIterator *iter_ctx);
void EvalContextStackPopFrame(EvalContext *ctx);
const char *EvalContextStackToString(EvalContext *ctx);
void EvalContextSetBundleArgs(EvalContext *ctx, const Rlist *args);
void EvalContextSetPass(EvalContext *ctx, int pass);
Rlist *EvalContextGetBundleArgs(EvalContext *ctx);
int EvalContextGetPass(EvalContext *ctx);

char *EvalContextStackPath(const EvalContext *ctx);
StringSet *EvalContextStackPromisees(const EvalContext *ctx);
const Promise *EvalContextStackCurrentPromise(const EvalContext *ctx);
const Bundle *EvalContextStackCurrentBundle(const EvalContext *ctx);
const RingBuffer *EvalContextStackCurrentMessages(const EvalContext *ctx);

Rlist *EvalContextGetPromiseCallerMethods(EvalContext *ctx);
JsonElement *EvalContextGetPromiseCallers(EvalContext *ctx);

bool EvalContextVariablePut(EvalContext *ctx, const VarRef *ref, const void *value, DataType type, const char *tags);
bool EvalContextVariablePutTagsSet(EvalContext *ctx, const VarRef *ref, const void *value, DataType type, StringSet *tags);
bool EvalContextVariablePutTagsSetWithComment(EvalContext *ctx,
                                              const VarRef *ref, const void *value,
                                              DataType type, StringSet *tags,
                                              const char *comment);
bool EvalContextVariablePutSpecial(EvalContext *ctx, SpecialScope scope, const char *lval, const void *value, DataType type, const char *tags);
bool EvalContextVariablePutSpecialTagsSet(EvalContext *ctx, SpecialScope scope, const char *lval,
                                          const void *value, DataType type, StringSet *tags);
bool EvalContextVariablePutSpecialTagsSetWithComment(EvalContext *ctx, SpecialScope scope,
                                                     const char *lval, const void *value,
                                                     DataType type, StringSet *tags,
                                                     const char *comment);
const void *EvalContextVariableGetSpecial(const EvalContext *ctx, const SpecialScope scope, const char *varname, DataType *type_out);
const char *EvalContextVariableGetSpecialString(const EvalContext *ctx, const SpecialScope scope, const char *varname);
const void *EvalContextVariableGet(const EvalContext *ctx, const VarRef *ref, DataType *type_out);
const Promise *EvalContextVariablePromiseGet(const EvalContext *ctx, const VarRef *ref);
bool EvalContextVariableRemoveSpecial(const EvalContext *ctx, SpecialScope scope, const char *lval);
bool EvalContextVariableRemove(const EvalContext *ctx, const VarRef *ref);
StringSet *EvalContextVariableTags(const EvalContext *ctx, const VarRef *ref);
bool EvalContextVariableClearMatch(EvalContext *ctx);
VariableTableIterator *EvalContextVariableTableIteratorNew(const EvalContext *ctx, const char *ns, const char *scope, const char *lval);
VariableTableIterator *EvalContextVariableTableFromRefIteratorNew(const EvalContext *ctx, const VarRef *ref);

bool EvalContextPromiseLockCacheContains(const EvalContext *ctx, const char *key);
void EvalContextPromiseLockCachePut(EvalContext *ctx, const char *key);
void EvalContextPromiseLockCacheRemove(EvalContext *ctx, const char *key);
bool EvalContextFunctionCacheGet(const EvalContext *ctx, const FnCall *fp, const Rlist *args, Rval *rval_out);
void EvalContextFunctionCachePut(EvalContext *ctx, const FnCall *fp, const Rlist *args, const Rval *rval);

const void  *EvalContextVariableControlCommonGet(const EvalContext *ctx, CommonControl lval);

/**
 * @brief Find a bundle for a bundle call, given a callee reference (in the form of ns:bundle), and a type of bundle.
 *        This is requires EvalContext because the callee reference may be unqualified.
 *        Hopefully this should go away in the future if we make a more generalized API to simply call a bundle,
 *        but we have a few special rules around edit_line and so on.
 */
const Bundle *EvalContextResolveBundleExpression(const EvalContext *ctx, const Policy *policy,
                                                 const char *callee_reference, const char *callee_type);

const Body *EvalContextFindFirstMatchingBody(const Policy *policy, const char *type,
                                             const char *namespace, const char *name);

/**
  @brief Returns a Sequence of const Body* elements, first the body and then its parents

  Uses `inherit_from` to figure out the parents.
  */
Seq *EvalContextResolveBodyExpression(const EvalContext *ctx, const Policy *policy,
                                      const char *callee_reference, const char *callee_type);

/* - Parsing/evaluating expressions - */
void ValidateClassSyntax(const char *str);
ExpressionValue CheckClassExpression(const EvalContext *ctx, const char *context);
static inline bool IsDefinedClass(const EvalContext *ctx, const char *context)
{
    return (CheckClassExpression(ctx, context) == EXPRESSION_VALUE_TRUE);
}
StringSet *ClassesMatching(const EvalContext *ctx, ClassTableIterator *iter, const char* regex, const Rlist *tags, bool first_only);
StringSet *ClassesMatchingGlobal(const EvalContext *ctx, const char* regex, const Rlist *tags, bool first_only);
StringSet *ClassesMatchingLocal(const EvalContext *ctx, const char* regex, const Rlist *tags, bool first_only);
bool EvalProcessResult(const char *process_result, StringSet *proc_attr);
bool EvalFileResult(const char *file_result, StringSet *leaf_attr);

/**
 * @brief Evaluates a class expression based on a set of defined classes.
 * @param expr Class expression (E.g. "GMT_Monday|GMT_Wednesday").
 * @param token_set Set of defined classes.
 * @return True if the class expression evaluates to true, otherwise false.
 */
bool EvalWithTokenFromList(const char *expr, StringSet *token_set);

/* Various global options */
void SetChecksumUpdatesDefault(EvalContext *ctx, bool enabled);
bool GetChecksumUpdatesDefault(const EvalContext *ctx);

/* IP addresses */
Item *EvalContextGetIpAddresses(const EvalContext *ctx);
void EvalContextAddIpAddress(EvalContext *ctx, const char *address, const char *iface);
void EvalContextDeleteIpAddresses(EvalContext *ctx);

/* - Rest - */
void EvalContextSetEvalOption(EvalContext *ctx, EvalContextOption option, bool value);
bool EvalContextGetEvalOption(EvalContext *ctx, EvalContextOption option);

bool EvalContextIsIgnoringLocks(const EvalContext *ctx);
void EvalContextSetIgnoreLocks(EvalContext *ctx, bool ignore);

void EvalContextSetLaunchDirectory(EvalContext *ctx, const char *path);
void EvalContextSetEntryPoint(EvalContext* ctx, const char *entry_point);
const char *EvalContextGetEntryPoint(EvalContext* ctx);

bool BundleAbort(EvalContext *ctx);
bool EvalAborted(const EvalContext *ctx);
void NotifyDependantPromises(EvalContext *ctx, const Promise *pp, PromiseResult result);
bool MissingDependencies(EvalContext *ctx, const Promise *pp);

/**
 * Record promise status (result).
 *
 * This function should be called once for every promise to record its
 * status/result. It logs the given message (#fmt and varargs) with the given
 * #level based on the logging specifications in the 'action' body and
 * increments the counters of actuated promises and promises
 * kept/repaired/failed/...
 *
 * If #fmt is NULL or an empty string, nothing is logged. #level should be
 * #LOG_LEVEL_NOTHING in such cases.
 */
void cfPS(EvalContext *ctx, LogLevel level, PromiseResult status, const Promise *pp, const Attributes *attr, const char *fmt, ...) FUNC_ATTR_PRINTF(6, 7);

/**
 * Log change done by the agent when evaluating policy and set the outcome
 * classes.
 *
 * Unlike cfPS(), this function is expected to be called multiple times for the
 * same promise. It's not recording a promise status, but rather one out of
 * multiple changes done by the given promise.
 */
void RecordChange(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...) FUNC_ATTR_PRINTF(4, 5);
void RecordNoChange(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...) FUNC_ATTR_PRINTF(4, 5);
void RecordFailure(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...) FUNC_ATTR_PRINTF(4, 5);
void RecordWarning(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...) FUNC_ATTR_PRINTF(4, 5);
void RecordDenial(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...) FUNC_ATTR_PRINTF(4, 5);
void RecordInterruption(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...) FUNC_ATTR_PRINTF(4, 5);

/**
 * Check if the given promise is allowed to make changes in the current agent
 * run and if not, log the fact and update #result accordingly.
 *
 * The #change_desc_fmt argument and the ones following it should format a
 * message that describes the action, like "rename the file '/etc/issue'", the
 * implementation of this function prepends and, potentially, appends text to
 * the message to form a complete sentence.
 */
bool MakingChanges(EvalContext *ctx, const Promise *pp, const Attributes *attr,
                   PromiseResult *result, const char *change_desc_fmt, ...) FUNC_ATTR_PRINTF(5, 6);

/**
 * Similar to MakingChanges() above, but checking if changes to internal
 * structures are allowed. Audit modes should, for example, not make such
 * changes, even though they make other changes (in a changes chroot).
 */
bool MakingInternalChanges(EvalContext *ctx, const Promise *pp, const Attributes *attr,
                           PromiseResult *result, const char *change_desc_fmt, ...) FUNC_ATTR_PRINTF(5, 6);

/**
 * Whether to make changes in a chroot or not.
 */
static inline bool ChrootChanges()
{
    return ((EVAL_MODE == EVAL_MODE_SIMULATE_DIFF) ||
            (EVAL_MODE == EVAL_MODE_SIMULATE_MANIFEST) ||
            (EVAL_MODE == EVAL_MODE_SIMULATE_MANIFEST_FULL));
}

/**
 * Set the chroot for recording changes in files (in simulate mode(s)).
 *
 * @note This function should only be called once.
 */
void SetChangesChroot(const char *chroot);

/**
 * Get the path for #orig_path under the changes chroot (where changes in simulate
 * mode(s) are done). #orig_path is expected to be an absolute path.
 *
 * @note Returns a pointer to an internal buffer and the value is only valid
 *       until the function is called again.
 * @warning not thread-safe
 */
const char *ToChangesChroot(const char *orig_path);

/**
 * Possibly transform #path to the path where changes are to be made.
 * @see ChrootChanges()
 * @see ToChangesChroot()
 */
static inline const char *ToChangesPath(const char *path)
{
    return (ChrootChanges() ? ToChangesChroot(path) : path);
}

/**
 * Reverse operation for ToChangesChroot().
 *
 * @return A pointer to an offset of #orig_path where the non-chrooted path starts.
 * @warning Doesn't work on Windows (because on Windows the operation is not as easy as just
 *          shifting the pointer by the offset).
 */
#ifndef __MINGW32__
const char *ToNormalRoot(const char *orig_path);
#else
const char *ToNormalRoot(const char *orig_path) __attribute__((error ("Not supported on Windows")));
#endif

PackagePromiseContext *GetPackageDefaultsFromCtx(const EvalContext *ctx);

bool EvalContextGetSelectEndMatchEof(const EvalContext *ctx);
void EvalContextSetSelectEndMatchEof(EvalContext *ctx, bool value);

void AddDefaultPackageModuleToContext(const EvalContext *ctx, char *name);
void AddDefaultInventoryToContext(const EvalContext *ctx, Rlist *inventory);

void AddPackageModuleToContext(const EvalContext *ctx, PackageModuleBody *pm);
PackageModuleBody *GetPackageModuleFromContext(const EvalContext *ctx, const char *name);
PackageModuleBody *GetDefaultPackageModuleFromContext(const EvalContext *ctx);
Rlist *GetDefaultInventoryFromContext(const EvalContext *ctx);
PackagePromiseContext *GetPackagePromiseContext(const EvalContext *ctx);

/* This function is temporarily exported. It needs to be made an detail of
 * evaluator again, once variables promises are no longer specially handled */
void ClassAuditLog(EvalContext *ctx, const Promise *pp, const Attributes *attr, PromiseResult status);

/**
 * Set classes based on the promise outcome/result.
 *
 * @note This function should only be called in special cases, ClassAuditLog()
 *       (which calls this function internally) should be called in most places.
 */
void SetPromiseOutcomeClasses(EvalContext *ctx, PromiseResult status, const DefineClasses *dc);

ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, TrackTotalCompliance, ARG_UNUSED PromiseResult, status, ARG_UNUSED const Promise *, pp);

ENTERPRISE_VOID_FUNC_3ARG_DECLARE(void, EvalContextLogPromiseIterationOutcome,
                                  EvalContext *, ctx,
                                  const Promise *, pp,
                                  PromiseResult, result);

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, EvalContextSetupMissionPortalLogHook,
                                  EvalContext *, ctx);
char *MissionPortalLogHook(LoggingPrivContext *pctx, LogLevel level, const char *message);

JsonElement* JsonExpandElement(EvalContext *ctx, const JsonElement *source);

void EvalContextSetDumpReports(EvalContext *ctx, bool dump_reports);
bool EvalContextGetDumpReports(EvalContext *ctx);
void EvalContextUpdateDumpReports(EvalContext *ctx);

#endif
