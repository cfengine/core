/*
   Copyright 2018 Northern.tech AS

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
#include <scope.h>
#include <variable.h>
#include <class.h>
#include <iteration.h>
#include <rb-tree.h>
#include <ring_buffer.h>

typedef enum
{
    STACK_FRAME_TYPE_BUNDLE,
    STACK_FRAME_TYPE_BODY,
    STACK_FRAME_TYPE_PROMISE_TYPE,
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
    const PromiseType *owner;
} StackFramePromiseType;

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
        StackFramePromiseType promise_type;
        StackFramePromise promise;
        StackFramePromiseIteration promise_iteration;
    } data;

    char *path;
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

void EvalContextHeapAddAbort(EvalContext *ctx, const char *context, const char *activated_on_context);
void EvalContextHeapAddAbortCurrentBundle(EvalContext *ctx, const char *context, const char *activated_on_context);

void EvalContextHeapPersistentSave(EvalContext *ctx, const char *name, unsigned int ttl_minutes, PersistentClassPolicy policy, const char *tags);
void EvalContextHeapPersistentRemove(const char *context);
void EvalContextHeapPersistentLoadAll(EvalContext *ctx);

bool EvalContextClassPutSoft(EvalContext *ctx, const char *name, ContextScope scope, const char *tags);
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

void EvalContextClear(EvalContext *ctx);

Rlist *EvalContextGetPromiseCallerMethods(EvalContext *ctx);

void EvalContextStackPushBundleFrame(EvalContext *ctx, const Bundle *owner, const Rlist *args, bool inherits_previous);
void EvalContextStackPushBodyFrame(EvalContext *ctx, const Promise *caller, const Body *body, const Rlist *args);
void EvalContextStackPushPromiseTypeFrame(EvalContext *ctx, const PromiseType *owner);
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
bool EvalContextVariablePutSpecial(EvalContext *ctx, SpecialScope scope, const char *lval, const void *value, DataType type, const char *tags);
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
bool IsDefinedClass(const EvalContext *ctx, const char *context);
StringSet *ClassesMatching(const EvalContext *ctx, ClassTableIterator *iter, const char* regex, const Rlist *tags, bool first_only);

bool EvalProcessResult(const char *process_result, StringSet *proc_attr);
bool EvalFileResult(const char *file_result, StringSet *leaf_attr);

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

bool Abort(EvalContext *ctx);
void NotifyDependantPromises(EvalContext *ctx, const Promise *pp, PromiseResult result);
bool MissingDependencies(EvalContext *ctx, const Promise *pp);
void cfPS(EvalContext *ctx, LogLevel level, PromiseResult status, const Promise *pp, Attributes attr, const char *fmt, ...) FUNC_ATTR_PRINTF(6, 7);

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
void ClassAuditLog(EvalContext *ctx, const Promise *pp, Attributes attr, PromiseResult status);

ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, TrackTotalCompliance, ARG_UNUSED PromiseResult, status, ARG_UNUSED const Promise *, pp);

ENTERPRISE_VOID_FUNC_3ARG_DECLARE(void, EvalContextLogPromiseIterationOutcome,
                                  EvalContext *, ctx,
                                  const Promise *, pp,
                                  PromiseResult, result);

ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, EvalContextSetupMissionPortalLogHook,
                                  EvalContext *, ctx);
char *MissionPortalLogHook(LoggingPrivContext *pctx, LogLevel level, const char *message);

JsonElement* JsonExpandElement(EvalContext *ctx, const JsonElement *source);

#endif
