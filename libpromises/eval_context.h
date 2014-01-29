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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_EVAL_CONTEXT_H
#define CFENGINE_EVAL_CONTEXT_H

#include <cf3.defs.h>

#include <writer.h>
#include <set.h>
#include <sequence.h>
#include <var_expressions.h>
#include <scope.h>
#include <variable.h>
#include <class.h>
#include <iteration.h>
#include <rb-tree.h>

typedef enum
{
    STACK_FRAME_TYPE_BUNDLE,
    STACK_FRAME_TYPE_PROMISE,
    STACK_FRAME_TYPE_PROMISE_ITERATION,
    STACK_FRAME_TYPE_BODY
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
    size_t pass;

    VariableTable *vars;
} StackFramePromise;

typedef struct
{
    Promise *owner;
    const PromiseIterator *iter_ctx;
    size_t index;
} StackFramePromiseIteration;

typedef struct
{
    StackFrameType type;
    bool inherits_previous; // whether or not this frame inherits context from the previous frame

    union
    {
        StackFrameBundle bundle;
        StackFrameBody body;
        StackFramePromise promise;
        StackFramePromiseIteration promise_iteration;
    } data;
} StackFrame;

TYPED_SET_DECLARE(Promise, const Promise *)

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

void EvalContextHeapPersistentSave(const char *context, const char *ns, unsigned int ttl_minutes, ContextStatePolicy policy);
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

void EvalContextClear(EvalContext *ctx);

void EvalContextStackPushBundleFrame(EvalContext *ctx, const Bundle *owner, const Rlist *args, bool inherits_previous);
void EvalContextStackPushBodyFrame(EvalContext *ctx, const Promise *caller, const Body *body, Rlist *args);
void EvalContextStackPushPromiseFrame(EvalContext *ctx, const Promise *owner, bool copy_bundle_context, size_t pass);
Promise *EvalContextStackPushPromiseIterationFrame(EvalContext *ctx, size_t iteration_index, const PromiseIterator *iter_ctx);
void EvalContextStackPopFrame(EvalContext *ctx);

char *EvalContextStackPath(const EvalContext *ctx);
StringSet *EvalContextStackPromisees(const EvalContext *ctx);
const Promise *EvalContextStackCurrentPromise(const EvalContext *ctx);
const Bundle *EvalContextStackCurrentBundle(const EvalContext *ctx);

bool EvalContextVariablePut(EvalContext *ctx, const VarRef *ref, const void *value, DataType type, const char *tags);
bool EvalContextVariablePutSpecial(EvalContext *ctx, SpecialScope scope, const char *lval, const void *value, DataType type, const char *tags);
const void *EvalContextVariableGet(const EvalContext *ctx, const VarRef *ref, DataType *type_out);
const Promise *EvalContextVariablePromiseGet(const EvalContext *ctx, const VarRef *ref);
bool EvalContextVariableRemoveSpecial(const EvalContext *ctx, SpecialScope scope, const char *lval);
bool EvalContextVariableRemove(const EvalContext *ctx, const VarRef *ref);
StringSet *EvalContextVariableTags(const EvalContext *ctx, const VarRef *ref);
bool EvalContextVariableClearMatch(EvalContext *ctx);
VariableTableIterator *EvalContextVariableTableIteratorNew(const EvalContext *ctx, const char *ns, const char *scope, const char *lval);

bool EvalContextFunctionCacheGet(const EvalContext *ctx, const FnCall *fp, const Rlist *args, Rval *rval_out);
void EvalContextFunctionCachePut(EvalContext *ctx, const FnCall *fp, const Rlist *args, const Rval *rval);

const void  *EvalContextVariableControlCommonGet(const EvalContext *ctx, CommonControl lval);


/* - Parsing/evaluating expressions - */
void ValidateClassSyntax(const char *str);
bool IsDefinedClass(const EvalContext *ctx, const char *context);

bool EvalProcessResult(const char *process_result, StringSet *proc_attr);
bool EvalFileResult(const char *file_result, StringSet *leaf_attr);

/* - Promise status */
bool EvalContextPromiseIsDone(const EvalContext *ctx, const Promise *pp);

/* Those two functions are compromises: there are pieces of code which
 * manipulate promise 'doneness', and it's not simple to figure out how to
 * properly reimplement it. So for the time being, let particular pieces of code
 * continue to manipulate the state.
 */
void EvalContextMarkPromiseDone(EvalContext *ctx, const Promise *pp);
void EvalContextMarkPromiseNotDone(EvalContext *ctx, const Promise *pp);

/* Various global options */
void SetChecksumUpdatesDefault(EvalContext *ctx, bool enabled);
bool GetChecksumUpdatesDefault(const EvalContext *ctx);

/* IP addresses */
Item *EvalContextGetIpAddresses(const EvalContext *ctx);
void EvalContextAddIpAddress(EvalContext *ctx, const char *address);
void EvalContextDeleteIpAddresses(EvalContext *ctx);

/* - Rest - */
bool EvalContextPromiseIsActive(const EvalContext *ctx, const Promise *pp);
void EvalContextSetEvalOption(EvalContext *ctx, EvalContextOption option, bool value);
bool EvalContextGetEvalOption(EvalContext *ctx, EvalContextOption option);

bool EvalContextIsIgnoringLocks(const EvalContext *ctx);
void EvalContextSetIgnoreLocks(EvalContext *ctx, bool ignore);

void EvalContextSetLaunchDirectory(EvalContext *ctx, const char *path);

bool Abort(EvalContext *ctx);
int VarClassExcluded(const EvalContext *ctx, const Promise *pp, char **classes);
void MarkPromiseHandleDone(EvalContext *ctx, const Promise *pp);
int MissingDependencies(EvalContext *ctx, const Promise *pp);
void cfPS(EvalContext *ctx, LogLevel level, PromiseResult status, const Promise *pp, Attributes attr, const char *fmt, ...) FUNC_ATTR_PRINTF(6, 7);

/* This function is temporarily exported. It needs to be made an detail of
 * evaluator again, once variables promises are no longer specially handled */
void ClassAuditLog(EvalContext *ctx, const Promise *pp, Attributes attr, PromiseResult status);

typedef struct EvalContextEnterpriseState_ EvalContextEnterpriseState;

EvalContextEnterpriseState *EvalContextGetEnterpriseState(const EvalContext *ctx);

ENTERPRISE_VOID_FUNC_2ARG_DECLARE(void, TrackTotalCompliance, ARG_UNUSED PromiseResult, status, ARG_UNUSED const Promise *, pp);

ENTERPRISE_FUNC_0ARG_DECLARE(EvalContextEnterpriseState *, EvalContextEnterpriseStateNew);
ENTERPRISE_VOID_FUNC_1ARG_DECLARE(void, EvalContextEnterpriseStateDestroy,
                                  EvalContextEnterpriseState *, estate);

ENTERPRISE_VOID_FUNC_3ARG_DECLARE(void, EvalContextLogPromiseIterationOutcome,
                                  EvalContext *, ctx,
                                  const Promise *, pp,
                                  PromiseResult, result);

#endif
