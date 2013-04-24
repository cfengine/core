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

#include "writer.h"
#include "set.h"
#include "sequence.h"
#include "var_expressions.h"

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

    StringSet *contexts;
    StringSet *contexts_negated;
} StackFrameBundle;

typedef struct
{
    const Body *owner;
} StackFrameBody;

typedef struct
{
    const Promise *owner;

    AssocHashTable *variables; // TODO: change to map
} StackFramePromise;

typedef struct
{
    const Promise *owner;
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

struct EvalContext_
{
    StringSet *heap_soft;
    StringSet *heap_hard;
    StringSet *heap_negated;
    Item *heap_abort;
    Item *heap_abort_current_bundle;

    Seq *stack;

    StringSet *dependency_handles;

    PromiseSet *promises_done;
};

EvalContext *EvalContextNew(void);
void EvalContextDestroy(EvalContext *ctx);

void EvalContextHeapAddSoft(EvalContext *ctx, const char *context, const char *ns);
void EvalContextHeapAddHard(EvalContext *ctx, const char *context);
void EvalContextHeapAddNegated(EvalContext *ctx, const char *context);
void EvalContextHeapAddAbort(EvalContext *ctx, const char *context, const char *activated_on_context);
void EvalContextHeapAddAbortCurrentBundle(EvalContext *ctx, const char *context, const char *activated_on_context);
void EvalContextStackFrameAddSoft(EvalContext *ctx, const char *context);
void EvalContextStackFrameAddNegated(EvalContext *ctx, const char *context);

void EvalContextHeapPersistentSave(const char *context, const char *ns, unsigned int ttl_minutes, ContextStatePolicy policy);
void EvalContextHeapPersistentRemove(const char *context);
void EvalContextHeapPersistentLoadAll(EvalContext *ctx);

bool EvalContextHeapContainsSoft(const EvalContext *ctx, const char *context);
bool EvalContextHeapContainsHard(const EvalContext *ctx, const char *context);
bool EvalContextHeapContainsNegated(const EvalContext *ctx, const char *context);
bool EvalContextStackFrameContainsSoft(const EvalContext *ctx, const char *context);

bool EvalContextHeapRemoveSoft(EvalContext *ctx, const char *context);
bool EvalContextHeapRemoveHard(EvalContext *ctx, const char *context);
void EvalContextStackFrameRemoveSoft(EvalContext *ctx, const char *context);

void EvalContextHeapClear(EvalContext *ctx);

size_t EvalContextHeapMatchCountSoft(const EvalContext *ctx, const char *context_regex);
size_t EvalContextHeapMatchCountHard(const EvalContext *ctx, const char *context_regex);
size_t EvalContextStackFrameMatchCountSoft(const EvalContext *ctx, const char *context_regex);

StringSet* EvalContextHeapAddMatchingSoft(const EvalContext *ctx, StringSet* base, const char *context_regex);
StringSet* EvalContextHeapAddMatchingHard(const EvalContext *ctx, StringSet* base, const char *context_regex);
StringSet* EvalContextStackFrameAddMatchingSoft(const EvalContext *ctx, StringSet* base, const char *context_regex);

StringSetIterator EvalContextHeapIteratorSoft(const EvalContext *ctx);
StringSetIterator EvalContextHeapIteratorHard(const EvalContext *ctx);
StringSetIterator EvalContextHeapIteratorNegated(const EvalContext *ctx);
StringSetIterator EvalContextStackFrameIteratorSoft(const EvalContext *ctx);

void EvalContextStackPushBundleFrame(EvalContext *ctx, const Bundle *owner, bool inherits_previous);
void EvalContextStackPushBodyFrame(EvalContext *ctx, const Body *owner);
void EvalContextStackPushPromiseFrame(EvalContext *ctx, const Promise *owner);
void EvalContextStackPushPromiseIterationFrame(EvalContext *ctx, const Promise *owner);
void EvalContextStackPopFrame(EvalContext *ctx);

/**
 * @brief Returns the topmost promise from the stack, or NULL if no promises are pushed
 */
const Promise *EvalContextStackGetTopPromise(const EvalContext *ctx);

bool EvalContextVariablePut(EvalContext *ctx, VarRef lval, Rval rval, DataType type);
bool EvalContextVariableGet(const EvalContext *ctx, VarRef lval, Rval *rval_out, DataType *type_out);

bool EvalContextVariableControlCommonGet(const EvalContext *ctx, CommonControl lval, Rval *rval_out);

/* - Parsing/evaluating expressions - */
void ValidateClassSyntax(const char *str);
bool IsDefinedClass(const EvalContext *ctx, const char *context, const char *ns);

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

/* - Rest - */
int Abort(void);
void KeepClassContextPromise(EvalContext *ctx, Promise *pp, void *param);
int VarClassExcluded(EvalContext *ctx, Promise *pp, char **classes);
void MarkPromiseHandleDone(EvalContext *ctx, const Promise *pp);
int MissingDependencies(EvalContext *ctx, const Promise *pp);
void cfPS(EvalContext *ctx, OutputLevel level, PromiseResult status, const char *errstr, const Promise *pp, Attributes attr, const char *fmt, ...) FUNC_ATTR_PRINTF(7, 8);

/* This function is temporarily exported. It needs to be made an detail of
 * evaluator again, once variables promises are no longer specially handled */
void ClassAuditLog(EvalContext *ctx, const Promise *pp, Attributes attr, PromiseResult status);


#endif
