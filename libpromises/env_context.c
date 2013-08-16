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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <env_context.h>

#include <files_names.h>
#include <logic_expressions.h>
#include <syntax.h>
#include <item_lib.h>
#include <ornaments.h>
#include <expand.h>
#include <matching.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <assoc.h>
#include <scope.h>
#include <vars.h>
#include <syslog_client.h>
#include <audit.h>
#include <promise_logging.h>
#include <rlist.h>
#include <buffer.h>
#include <promises.h>


static bool EvalContextStackFrameContainsNegated(const EvalContext *ctx, const char *context);

static bool ABORTBUNDLE = false;

static StackFrame *LastStackFrame(const EvalContext *ctx, size_t offset)
{
    if (SeqLength(ctx->stack) <= offset)
    {
        return NULL;
    }
    return SeqAt(ctx->stack, SeqLength(ctx->stack) - 1 - offset);
}

static StackFrame *LastStackFrameByType(const EvalContext *ctx, StackFrameType type)
{
    for (size_t i = 0; i < SeqLength(ctx->stack); i++)
    {
        StackFrame *frame = LastStackFrame(ctx, i);
        if (frame->type == type)
        {
            return frame;
        }
    }

    return NULL;
}

static const char *GetAgentAbortingContext(const EvalContext *ctx)
{
    for (const Item *ip = ctx->heap_abort; ip != NULL; ip = ip->next)
    {
        if (IsDefinedClass(ctx, ip->name, NULL))
        {
            return ip->name;
        }
    }
    return NULL;
}

void EvalContextHeapAddSoft(EvalContext *ctx, const char *context, const char *ns)
{
    char context_copy[CF_MAXVARSIZE];
    char canonified_context[CF_MAXVARSIZE];

    strcpy(canonified_context, context);
    if (Chop(canonified_context, CF_EXPANDSIZE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
    }
    CanonifyNameInPlace(canonified_context);

    if (ns && strcmp(ns, "default") != 0)
    {
        snprintf(context_copy, CF_MAXVARSIZE, "%s:%s", ns, canonified_context);
    }
    else
    {
        strncpy(context_copy, canonified_context, CF_MAXVARSIZE);
    }

    if (strlen(context_copy) == 0)
    {
        return;
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort_current_bundle, context_copy))
    {
        Log(LOG_LEVEL_ERR, "Bundle aborted on defined class '%s'", context_copy);
        ABORTBUNDLE = true;
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort, context_copy))
    {
        FatalError(ctx, "cf-agent aborted on defined class '%s'", context_copy);
    }

    if (EvalContextHeapContainsSoft(ctx, context_copy))
    {
        return;
    }

    StringSetAdd(ctx->heap_soft, xstrdup(context_copy));

    if (!ABORTBUNDLE)
    {
        for (const Item *ip = ctx->heap_abort_current_bundle; ip != NULL; ip = ip->next)
        {
            if (IsDefinedClass(ctx, ip->name, ns))
            {
                Log(LOG_LEVEL_ERR, "Setting abort for '%s' when setting '%s'", ip->name, context_copy);
                ABORTBUNDLE = true;
                break;
            }
        }
    }
}

/*******************************************************************/

void EvalContextHeapAddHard(EvalContext *ctx, const char *context)
{
    char context_copy[CF_MAXVARSIZE];

    strcpy(context_copy, context);
    if (Chop(context_copy, CF_EXPANDSIZE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
    }
    CanonifyNameInPlace(context_copy);

    if (strlen(context_copy) == 0)
    {
        return;
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort_current_bundle, context_copy))
    {
        Log(LOG_LEVEL_ERR, "Bundle aborted on defined class '%s'", context_copy);
        ABORTBUNDLE = true;
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort, context_copy))
    {
        FatalError(ctx, "cf-agent aborted on defined class '%s'", context_copy);
    }

    if (EvalContextHeapContainsHard(ctx, context_copy))
    {
        return;
    }

    StringSetAdd(ctx->heap_hard, xstrdup(context_copy));

    if (!ABORTBUNDLE)
    {
        for (const Item *ip = ctx->heap_abort_current_bundle; ip != NULL; ip = ip->next)
        {
            if (IsDefinedClass(ctx, ip->name, NULL))
            {
                Log(LOG_LEVEL_ERR, "Setting abort for '%s' when setting '%s'", ip->name, context_copy);
                ABORTBUNDLE = true;
                break;
            }
        }
    }
}

void EvalContextStackFrameAddSoft(EvalContext *ctx, const char *context)
{
    assert(SeqLength(ctx->stack) > 0);

    StackFrameBundle frame;
    {
        StackFrame *last_frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
        if (!last_frame)
        {
            ProgrammingError("Attempted to add a soft class on the stack, but stack had no bundle frame");
        }
        frame = last_frame->data.bundle;
    }

    char copy[CF_BUFSIZE];
    if (strcmp(frame.owner->ns, "default") != 0)
    {
         snprintf(copy, CF_MAXVARSIZE, "%s:%s", frame.owner->ns, context);
    }
    else
    {
         strncpy(copy, context, CF_MAXVARSIZE);
    }

    if (Chop(copy, CF_EXPANDSIZE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
    }

    if (strlen(copy) == 0)
    {
        return;
    }

    if (EvalContextHeapContainsSoft(ctx, copy))
    {
        Log(LOG_LEVEL_WARNING, "Private class '%s' in bundle '%s' shadows a global class - you should choose a different name to avoid conflicts",
              copy, frame.owner->name);
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort_current_bundle, copy))
    {
        Log(LOG_LEVEL_ERR, "Bundle aborted on defined class '%s'", copy);
        ABORTBUNDLE = true;
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort, copy))
    {
        FatalError(ctx, "cf-agent aborted on defined class '%s'", copy);
    }

    if (EvalContextStackFrameContainsSoft(ctx, copy))
    {
        return;
    }

    StringSetAdd(frame.contexts, xstrdup(copy));

    if (!ABORTBUNDLE)
    {
        for (const Item *ip = ctx->heap_abort_current_bundle; ip != NULL; ip = ip->next)
        {
            if (IsDefinedClass(ctx, ip->name, frame.owner->ns))
            {
                Log(LOG_LEVEL_ERR, "Setting abort for '%s' when setting '%s'", ip->name, context);
                ABORTBUNDLE = true;
                break;
            }
        }
    }
}

typedef struct
{
    const EvalContext *ctx;
    const char *ns;
} EvalTokenAsClassContext;

static ExpressionValue EvalTokenAsClass(const char *classname, void *param)
{
    const EvalContext *ctx = ((EvalTokenAsClassContext *)param)->ctx;
    const char *ns = ((EvalTokenAsClassContext *)param)->ns;

    char qualified_class[CF_MAXVARSIZE];

    if (strcmp(classname, "any") == 0)
       {
       return true;
       }
    
    if (strchr(classname, ':'))
    {
        if (strncmp(classname, "default:", strlen("default:")) == 0)
        {
            snprintf(qualified_class, CF_MAXVARSIZE, "%s", classname + strlen("default:"));
        }
        else
        {
            snprintf(qualified_class, CF_MAXVARSIZE, "%s", classname);
        }
    }
    else if (ns != NULL && strcmp(ns, "default") != 0)
    {
        snprintf(qualified_class, CF_MAXVARSIZE, "%s:%s", ns, (char *)classname);
    }
    else
    {
        snprintf(qualified_class, CF_MAXVARSIZE, "%s", classname);
    }

    if (EvalContextHeapContainsNegated(ctx, qualified_class))
    {
        return false;
    }
    if (EvalContextStackFrameContainsNegated(ctx, qualified_class))
    {
        return false;
    }
    if (EvalContextHeapContainsHard(ctx, classname))  // Hard classes are always unqualified
    {
        return true;
    }
    if (EvalContextHeapContainsSoft(ctx, qualified_class))
    {
        return true;
    }
    if (EvalContextStackFrameContainsSoft(ctx, qualified_class))
    {
        return true;
    }
    return false;
}

/**********************************************************************/

static char *EvalVarRef(ARG_UNUSED const char *varname, ARG_UNUSED VarRefType type, ARG_UNUSED void *param)
{
/*
 * There should be no unexpanded variables when we evaluate any kind of
 * logic expressions, until parsing of logic expression changes and they are
 * not pre-expanded before evaluation.
 */
    return NULL;
}

/**********************************************************************/

bool IsDefinedClass(const EvalContext *ctx, const char *context, const char *ns)
{
    ParseResult res;

    if (!context)
    {
        return true;
    }

    res = ParseExpression(context, 0, strlen(context));

    if (!res.result)
    {
        Log(LOG_LEVEL_ERR, "Unable to parse class expression '%s'", context);
        return false;
    }
    else
    {
        EvalTokenAsClassContext etacc = {
            .ctx = ctx,
            .ns = ns
        };

        ExpressionValue r = EvalExpression(res.result,
                                           &EvalTokenAsClass, &EvalVarRef,
                                           &etacc);

        FreeExpression(res.result);

        /* r is EvalResult which could be ERROR */
        return r == true;
    }
}

/**********************************************************************/

static ExpressionValue EvalTokenFromList(const char *token, void *param)
{
    StringSet *set = param;
    return StringSetContains(set, token);
}

/**********************************************************************/

static bool EvalWithTokenFromList(const char *expr, StringSet *token_set)
{
    ParseResult res = ParseExpression(expr, 0, strlen(expr));

    if (!res.result)
    {
        Log(LOG_LEVEL_ERR, "Syntax error in expression '%s'", expr);
        return false;           /* FIXME: return error */
    }
    else
    {
        ExpressionValue r = EvalExpression(res.result,
                                           &EvalTokenFromList,
                                           &EvalVarRef,
                                           token_set);

        FreeExpression(res.result);

        /* r is EvalResult which could be ERROR */
        return r == true;
    }
}

/**********************************************************************/

/* Process result expression */

bool EvalProcessResult(const char *process_result, StringSet *proc_attr)
{
    return EvalWithTokenFromList(process_result, proc_attr);
}

/**********************************************************************/

/* File result expressions */

bool EvalFileResult(const char *file_result, StringSet *leaf_attr)
{
    return EvalWithTokenFromList(file_result, leaf_attr);
}

/*****************************************************************************/

void EvalContextHeapPersistentSave(const char *context, const char *ns, unsigned int ttl_minutes, ContextStatePolicy policy)
{
    CF_DB *dbp;
    CfState state;
    time_t now = time(NULL);
    char name[CF_BUFSIZE];

    if (!OpenDB(&dbp, dbid_state))
    {
        return;
    }

    snprintf(name, CF_BUFSIZE, "%s%c%s", ns, CF_NS, context);
    
    if (ReadDB(dbp, name, &state, sizeof(state)))
    {
        if (state.policy == CONTEXT_STATE_POLICY_PRESERVE)
        {
            if (now < state.expires)
            {
                Log(LOG_LEVEL_VERBOSE, "Persisent state '%s' is already in a preserved state --  %jd minutes to go",
                      name, (intmax_t)((state.expires - now) / 60));
                CloseDB(dbp);
                return;
            }
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "New persistent state '%s'", name);
    }

    state.expires = now + ttl_minutes * 60;
    state.policy = policy;

    WriteDB(dbp, name, &state, sizeof(state));
    CloseDB(dbp);
}

/*****************************************************************************/

void EvalContextHeapPersistentRemove(const char *context)
{
    CF_DB *dbp;

    if (!OpenDB(&dbp, dbid_state))
    {
        return;
    }

    DeleteDB(dbp, context);
    Log(LOG_LEVEL_DEBUG, "Deleted persistent class '%s'", context);
    CloseDB(dbp);
}

/*****************************************************************************/

void EvalContextHeapPersistentLoadAll(EvalContext *ctx)
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    int ksize, vsize;
    char *key;
    void *value;
    time_t now = time(NULL);
    CfState q;

    if (LOOKUP)
    {
        return;
    }

    Banner("Loading persistent classes");

    if (!OpenDB(&dbp, dbid_state))
    {
        return;
    }

/* Acquire a cursor for the database. */

    if (!NewDBCursor(dbp, &dbcp))
    {
        Log(LOG_LEVEL_INFO, "Unable to scan persistence cache");
        return;
    }

    while (NextDB(dbcp, &key, &ksize, &value, &vsize))
    {
        memcpy((void *) &q, value, sizeof(CfState));

        Log(LOG_LEVEL_DEBUG, "Found key persistent class key '%s'", key);

        if (now > q.expires)
        {
            Log(LOG_LEVEL_VERBOSE, "Persistent class '%s' expired", key);
            DBCursorDeleteEntry(dbcp);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Persistent class '%s' for %jd more minutes", key, (intmax_t)((q.expires - now) / 60));
            Log(LOG_LEVEL_VERBOSE, "Adding persistent class '%s' to heap", key);
            if (strchr(key, CF_NS))
               {
               char ns[CF_MAXVARSIZE], name[CF_MAXVARSIZE];
               ns[0] = '\0';
               name[0] = '\0';
               sscanf(key, "%[^:]:%[^\n]", ns, name);
               EvalContextHeapAddSoft(ctx, name, ns);
               }
            else
               {
               EvalContextHeapAddSoft(ctx, key, NULL);
               }
        }
    }

    DeleteDBCursor(dbcp);
    CloseDB(dbp);

    Banner("Loaded persistent memory");
}

/***************************************************************************/

int Abort()
{
    if (ABORTBUNDLE)
    {
        ABORTBUNDLE = false;
        return true;
    }

    return false;
}

/*****************************************************************************/

int VarClassExcluded(EvalContext *ctx, Promise *pp, char **classes)
{
    Constraint *cp = PromiseGetConstraint(ctx, pp, "ifvarclass");

    if (cp == NULL)
    {
        return false;
    }

    *classes = (char *) ConstraintGetRvalValue(ctx, "ifvarclass", pp, RVAL_TYPE_SCALAR);

    if (*classes == NULL)
    {
        return true;
    }

    if (strchr(*classes, '$') || strchr(*classes, '@'))
    {
        Log(LOG_LEVEL_DEBUG, "Class expression did not evaluate");
        return true;
    }

    if (*classes && IsDefinedClass(ctx, *classes, PromiseGetNamespace(pp)))
    {
        return false;
    }
    else
    {
        return true;
    }
}

void EvalContextHeapAddAbort(EvalContext *ctx, const char *context, const char *activated_on_context)
{
    if (!IsItemIn(ctx->heap_abort, context))
    {
        AppendItem(&ctx->heap_abort, context, activated_on_context);
    }
}

void EvalContextHeapAddAbortCurrentBundle(EvalContext *ctx, const char *context, const char *activated_on_context)
{
    if (!IsItemIn(ctx->heap_abort_current_bundle, context))
    {
        AppendItem(&ctx->heap_abort_current_bundle, context, activated_on_context);
    }
}

/*****************************************************************************/

void MarkPromiseHandleDone(EvalContext *ctx, const Promise *pp)
{
    char name[CF_BUFSIZE];
    const char *handle = PromiseGetHandle(pp);

    if (handle == NULL)
    {
       return;
    }

    snprintf(name, CF_BUFSIZE, "%s:%s", PromiseGetNamespace(pp), handle);
    StringSetAdd(ctx->dependency_handles, xstrdup(name));
}

/*****************************************************************************/

int MissingDependencies(EvalContext *ctx, const Promise *pp)
{
    if (pp == NULL)
    {
        return false;
    }

    char name[CF_BUFSIZE], *d;
    Rlist *rp, *deps = PromiseGetConstraintAsList(ctx, "depends_on", pp);
    
    for (rp = deps; rp != NULL; rp = rp->next)
    {
        if (strchr(rp->item, ':'))
        {
            d = (char *)rp->item;
        }
        else
        {
            snprintf(name, CF_BUFSIZE, "%s:%s", PromiseGetNamespace(pp), (char *)rp->item);
            d = name;
        }

        if (!StringSetContains(ctx->dependency_handles, d))
        {
            if (LEGACY_OUTPUT)
            {
                Log(LOG_LEVEL_VERBOSE, "\n");
                Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
                Log(LOG_LEVEL_VERBOSE, "Skipping whole next promise (%s), as promise dependency %s has not yet been kept", pp->promiser, d);
                Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Skipping next promise '%s', as promise dependency '%s' has not yet been kept", pp->promiser, d);
            }

            return true;
        }
    }

    return false;
}

static void StackFrameBundleDestroy(StackFrameBundle frame)
{
    StringSetDestroy(frame.contexts);
    StringSetDestroy(frame.contexts_negated);

    VariableTableDestroy(frame.vars);
}

static void StackFrameBodyDestroy(ARG_UNUSED StackFrameBody frame)
{
    VariableTableDestroy(frame.vars);
}

static void StackFramePromiseDestroy(StackFramePromise frame)
{
    VariableTableDestroy(frame.vars);
}

static void StackFramePromiseIterationDestroy(ARG_UNUSED StackFramePromiseIteration frame)
{
    return;
}

static void StackFrameDestroy(StackFrame *frame)
{
    if (frame)
    {
        switch (frame->type)
        {
        case STACK_FRAME_TYPE_BUNDLE:
            StackFrameBundleDestroy(frame->data.bundle);
            break;

        case STACK_FRAME_TYPE_BODY:
            StackFrameBodyDestroy(frame->data.body);
            break;

        case STACK_FRAME_TYPE_PROMISE:
            StackFramePromiseDestroy(frame->data.promise);
            break;

        case STACK_FRAME_TYPE_PROMISE_ITERATION:
            StackFramePromiseIterationDestroy(frame->data.promise_iteration);
            break;

        default:
            ProgrammingError("Unhandled stack frame type");
        }

        free(frame);
    }
}

static unsigned PointerHashFn(const void *p, ARG_UNUSED unsigned int seed, unsigned int max)
{
    return ((unsigned)(uintptr_t)p) % max;
}

static bool PointerEqualFn(const void *key1, const void *key2)
{
    return key1 == key2;
}

TYPED_SET_DEFINE(Promise, const Promise *, &PointerHashFn, &PointerEqualFn, NULL)

EvalContext *EvalContextNew(void)
{
    EvalContext *ctx = xmalloc(sizeof(EvalContext));

    ctx->heap_soft = StringSetNew();
    ctx->heap_hard = StringSetNew();
    ctx->heap_negated = StringSetNew();
    ctx->heap_abort = NULL;
    ctx->heap_abort_current_bundle = NULL;

    ctx->stack = SeqNew(10, StackFrameDestroy);

    ctx->global_variables = VariableTableNew();
    ctx->match_variables = VariableTableNew();

    ctx->dependency_handles = StringSetNew();

    ctx->promises_done = PromiseSetNew();

    return ctx;
}

void EvalContextDestroy(EvalContext *ctx)
{
    if (ctx)
    {
        StringSetDestroy(ctx->heap_soft);
        StringSetDestroy(ctx->heap_hard);
        StringSetDestroy(ctx->heap_negated);
        DeleteItemList(ctx->heap_abort);
        DeleteItemList(ctx->heap_abort_current_bundle);

        SeqDestroy(ctx->stack);

        VariableTableDestroy(ctx->global_variables);
        VariableTableDestroy(ctx->match_variables);

        StringSetDestroy(ctx->dependency_handles);

        PromiseSetDestroy(ctx->promises_done);

        free(ctx);
    }
}

void EvalContextHeapAddNegated(EvalContext *ctx, const char *context)
{
    StringSetAdd(ctx->heap_negated, xstrdup(context));
}

void EvalContextStackFrameAddNegated(EvalContext *ctx, const char *context)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    assert(frame);

    StringSetAdd(frame->data.bundle.contexts_negated, xstrdup(context));
}

bool EvalContextHeapContainsSoft(const EvalContext *ctx, const char *context)
{
    return StringSetContains(ctx->heap_soft, context);
}

bool EvalContextHeapContainsHard(const EvalContext *ctx, const char *context)
{
    return StringSetContains(ctx->heap_hard, context);
}

bool EvalContextHeapContainsNegated(const EvalContext *ctx, const char *context)
{
    return StringSetContains(ctx->heap_negated, context);
}

bool StackFrameContainsSoftRecursive(const EvalContext *ctx, const char *context, size_t stack_index)
{
    StackFrame *frame = SeqAt(ctx->stack, stack_index);
    if (frame->type == STACK_FRAME_TYPE_BUNDLE && StringSetContains(frame->data.bundle.contexts, context))
    {
        return true;
    }
    else if (stack_index > 0 && frame->inherits_previous)
    {
        return StackFrameContainsSoftRecursive(ctx, context, stack_index - 1);
    }
    else
    {
        return false;
    }
}

bool EvalContextStackFrameContainsSoft(const EvalContext *ctx, const char *context)
{
    if (SeqLength(ctx->stack) == 0)
    {
        return false;
    }

    size_t stack_index = SeqLength(ctx->stack) - 1;
    return StackFrameContainsSoftRecursive(ctx, context, stack_index);
}

bool StackFrameContainsNegatedRecursive(const EvalContext *ctx, const char *context, size_t stack_index)
{
    StackFrame *frame = SeqAt(ctx->stack, stack_index);
    if (frame->type == STACK_FRAME_TYPE_BUNDLE && StringSetContains(frame->data.bundle.contexts_negated, context))
    {
        return true;
    }
    else if (stack_index > 0 && frame->inherits_previous)
    {
        return StackFrameContainsNegatedRecursive(ctx, context, stack_index - 1);
    }
    else
    {
        return false;
    }
}

static bool EvalContextStackFrameContainsNegated(const EvalContext *ctx, const char *context)
{
    if (SeqLength(ctx->stack) == 0)
    {
        return false;
    }

    size_t stack_index = SeqLength(ctx->stack) - 1;
    return StackFrameContainsNegatedRecursive(ctx, context, stack_index);
}

bool EvalContextHeapRemoveSoft(EvalContext *ctx, const char *context)
{
    return StringSetRemove(ctx->heap_soft, context);
}

bool EvalContextHeapRemoveHard(EvalContext *ctx, const char *context)
{
    return StringSetRemove(ctx->heap_hard, context);
}

void EvalContextClear(EvalContext *ctx)
{
    StringSetClear(ctx->heap_soft);
    StringSetClear(ctx->heap_hard);
    StringSetClear(ctx->heap_negated);

    VariableTableClear(ctx->global_variables, NULL, NULL, NULL);
    VariableTableClear(ctx->match_variables, NULL, NULL, NULL);
    SeqClear(ctx->stack);
}

static size_t StringSetMatchCount(StringSet *set, const char *regex)
{
    size_t count = 0;
    StringSetIterator it = StringSetIteratorInit(set);
    const char *context = NULL;
    while ((context = SetIteratorNext(&it)))
    {
        if (StringMatchFull(regex, context))
        {
            count++;
        }
    }
    return count;
}

size_t EvalContextHeapMatchCountSoft(const EvalContext *ctx, const char *context_regex)
{
    return StringSetMatchCount(ctx->heap_soft, context_regex);
}

size_t EvalContextHeapMatchCountHard(const EvalContext *ctx, const char *context_regex)
{
    return StringSetMatchCount(ctx->heap_hard, context_regex);
}

size_t EvalContextStackFrameMatchCountSoft(const EvalContext *ctx, const char *context_regex)
{
    if (SeqLength(ctx->stack) == 0)
    {
        return 0;
    }

    const StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    assert(frame);

    return StringSetMatchCount(frame->data.bundle.contexts, context_regex);
}

StringSet *StringSetAddAllMatchingIterator(StringSet* base, StringSetIterator it, const char *filter_regex)
{
    const char *element = NULL;
    while ((element = SetIteratorNext(&it)))
    {
        if (StringMatch(filter_regex, element))
        {
            StringSetAdd(base, xstrdup(element));
        }
    }
    return base;
}

StringSet *StringSetAddAllMatching(StringSet* base, const StringSet* filtered, const char *filter_regex)
{
    return StringSetAddAllMatchingIterator(base, StringSetIteratorInit((StringSet*)filtered), filter_regex);
}

StringSet *EvalContextHeapAddMatchingSoft(const EvalContext *ctx, StringSet* base, const char *context_regex)
{
    return StringSetAddAllMatching(base, ctx->heap_soft, context_regex);
}

StringSet *EvalContextHeapAddMatchingHard(const EvalContext *ctx, StringSet* base, const char *context_regex)
{
    return StringSetAddAllMatching(base, ctx->heap_hard, context_regex);
}

StringSet *EvalContextStackFrameAddMatchingSoft(const EvalContext *ctx, StringSet* base, const char *context_regex)
{
    if (SeqLength(ctx->stack) == 0)
    {
        return base;
    }

    return StringSetAddAllMatchingIterator(base, EvalContextStackFrameIteratorSoft(ctx), context_regex);
}

StringSetIterator EvalContextHeapIteratorSoft(const EvalContext *ctx)
{
    return StringSetIteratorInit(ctx->heap_soft);
}

StringSetIterator EvalContextHeapIteratorHard(const EvalContext *ctx)
{
    return StringSetIteratorInit(ctx->heap_hard);
}

StringSetIterator EvalContextHeapIteratorNegated(const EvalContext *ctx)
{
    return StringSetIteratorInit(ctx->heap_negated);
}

static StackFrame *StackFrameNew(StackFrameType type, bool inherit_previous)
{
    StackFrame *frame = xmalloc(sizeof(StackFrame));

    frame->type = type;
    frame->inherits_previous = inherit_previous;

    return frame;
}

static StackFrame *StackFrameNewBundle(const Bundle *owner, bool inherit_previous)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_BUNDLE, inherit_previous);

    frame->data.bundle.owner = owner;
    frame->data.bundle.contexts = StringSetNew();
    frame->data.bundle.contexts_negated = StringSetNew();
    frame->data.bundle.vars = VariableTableNew();

    return frame;
}

static StackFrame *StackFrameNewBody(const Body *owner)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_BODY, false);

    frame->data.body.owner = owner;
    frame->data.body.vars = VariableTableNew();

    return frame;
}

static StackFrame *StackFrameNewPromise(const Promise *owner)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_PROMISE, true);

    frame->data.promise.owner = owner;
    frame->data.promise.vars = VariableTableNew();

    return frame;
}

static StackFrame *StackFrameNewPromiseIteration(const Rlist *iteration_context)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_PROMISE_ITERATION, true);

    frame->data.promise_iteration.iteration_context = iteration_context;

    return frame;
}

void EvalContextStackFrameRemoveSoft(EvalContext *ctx, const char *context)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    assert(frame);

    StringSetRemove(frame->data.bundle.contexts, context);
}

static void EvalContextStackPushFrame(EvalContext *ctx, StackFrame *frame)
{
    SeqAppend(ctx->stack, frame);
}

void EvalContextStackPushBundleFrame(EvalContext *ctx, const Bundle *owner, const Rlist *args, bool inherits_previous)
{
    assert(!LastStackFrame(ctx, 0) || LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_PROMISE_ITERATION);

    EvalContextStackPushFrame(ctx, StackFrameNewBundle(owner, inherits_previous));

    if (RlistLen(args) > 0)
    {
        const Promise *caller = EvalContextStackCurrentPromise(ctx);
        if (caller)
        {
            VariableTable *table = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE)->data.bundle.vars;
            VariableTableClear(table, NULL, NULL, NULL);
        }

        ScopeAugment(ctx, owner, caller, args);
    }

    {
        VariableTableIterator *iter = VariableTableIteratorNew(ctx->global_variables, owner->ns, owner->name, NULL);
        Variable *var = NULL;
        while ((var = VariableTableIteratorNext(iter)))
        {
            Rval retval = ExpandPrivateRval(ctx, owner->ns, owner->name, var->rval);
            RvalDestroy(var->rval);
            var->rval = retval;
        }
    }
}

void EvalContextStackPushBodyFrame(EvalContext *ctx, const Body *owner, Rlist *args)
{
    assert((!LastStackFrame(ctx, 0) && strcmp("control", owner->name) == 0) || LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_BUNDLE);

    EvalContextStackPushFrame(ctx, StackFrameNewBody(owner));

    if (RlistLen(owner->args) != RlistLen(args))
    {
        const Promise *caller = EvalContextStackCurrentPromise(ctx);
        assert(caller);

        Log(LOG_LEVEL_ERR, "Argument arity mismatch in body '%s' at line %zu in file '%s', expected %d, got %d",
            owner->name, caller->offset.line, PromiseGetBundle(caller)->source_path, RlistLen(owner->args), RlistLen(args));
        return;
    }
    else
    {
        ScopeMapBodyArgs(ctx, owner, args);
    }
}

void EvalContextStackPushPromiseFrame(EvalContext *ctx, const Promise *owner, bool copy_bundle_context)
{
    assert(LastStackFrame(ctx, 0) && LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_BUNDLE);

    EvalContextVariableClearMatch(ctx);

    StackFrame *frame = StackFrameNewPromise(owner);

    EvalContextStackPushFrame(ctx, frame);

    if (copy_bundle_context)
    {
        frame->data.promise.vars = VariableTableCopyLocalized(ctx->global_variables,
                                                              EvalContextStackCurrentBundle(ctx)->ns,
                                                              EvalContextStackCurrentBundle(ctx)->name);
    }
    else
    {
        frame->data.promise.vars = VariableTableNew();
    }
}

void EvalContextStackPushPromiseIterationFrame(EvalContext *ctx, const Rlist *iteration_context)
{
    assert(LastStackFrame(ctx, 0) && LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_PROMISE);

    EvalContextStackPushFrame(ctx, StackFrameNewPromiseIteration(iteration_context));

    if (RlistLen(iteration_context) > 0)
    {
        StackFrame *promise_frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_PROMISE);
        VariableTableIterator *iter = VariableTableIteratorNew(promise_frame->data.promise.vars, NULL, NULL, NULL);

        Variable *var = NULL;
        while ((var = VariableTableIteratorNext(iter)))
        {
            for (const Rlist *rp = iteration_context; rp != NULL; rp = rp->next)
            {
                CfAssoc *cplist = rp->item;

                char *legacy_lval = VarRefToString(var->ref, false);

                if (strcmp(cplist->lval, legacy_lval) == 0)
                {
                    /* Link up temp hash to variable lol */

                    if (rp->state_ptr == NULL || rp->state_ptr->type == RVAL_TYPE_FNCALL)
                    {
                        /* Unexpanded function, or blank variable must be skipped. */
                        return;
                    }

                    if (rp->state_ptr)
                    {
                        // must first free existing rval in scope, then allocate new (should always be string)
                        RvalDestroy(var->rval);

                        // avoids double free - borrowing value from lol (freed in DeleteScope())
                        var->rval.item = xstrdup(rp->state_ptr->item);
                    }

                    switch (var->type)
                    {
                    case DATA_TYPE_STRING_LIST:
                        var->type = DATA_TYPE_STRING;
                        var->rval.type = RVAL_TYPE_SCALAR;
                        break;
                    case DATA_TYPE_INT_LIST:
                        var->type = DATA_TYPE_INT;
                        var->rval.type = RVAL_TYPE_SCALAR;
                        break;
                    case DATA_TYPE_REAL_LIST:
                        var->type = DATA_TYPE_REAL;
                        var->rval.type = RVAL_TYPE_SCALAR;
                        break;
                    default:
                        /* Only lists need to be converted */
                        break;
                    }
                }

                free(legacy_lval);
            }
        }
    }
}

void EvalContextStackPopFrame(EvalContext *ctx)
{
    assert(SeqLength(ctx->stack) > 0);

    StackFrame *last_frame = LastStackFrame(ctx, 0);
    switch (last_frame->type)
    {
    case STACK_FRAME_TYPE_BUNDLE:
        {
            const Bundle *bp = last_frame->data.bundle.owner;
            if (strcmp(bp->type, "edit_line") == 0 || strcmp(bp->type, "edit_xml") == 0)
            {
                VariableTableClear(last_frame->data.bundle.vars, "default", "edit", NULL);
            }
        }
        break;

    default:
        break;
    }

    SeqRemove(ctx->stack, SeqLength(ctx->stack) - 1);

    if (GetAgentAbortingContext(ctx))
    {
        FatalError(ctx, "cf-agent aborted on context '%s'", GetAgentAbortingContext(ctx));
    }
}

StringSetIterator EvalContextStackFrameIteratorSoft(const EvalContext *ctx)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    assert(frame);

    return StringSetIteratorInit(frame->data.bundle.contexts);
}

const Promise *EvalContextStackCurrentPromise(const EvalContext *ctx)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_PROMISE);
    return frame ? frame->data.promise.owner : NULL;
}

const Bundle *EvalContextStackCurrentBundle(const EvalContext *ctx)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    return frame ? frame->data.bundle.owner : NULL;
}


char *EvalContextStackPath(const EvalContext *ctx)
{
    Writer *path = StringWriter();

    for (size_t i = 0; i < SeqLength(ctx->stack); i++)
    {
        StackFrame *frame = SeqAt(ctx->stack, i);
        switch (frame->type)
        {
        case STACK_FRAME_TYPE_BODY:
            WriterWriteF(path, "/%s", frame->data.body.owner->name);
            break;

        case STACK_FRAME_TYPE_BUNDLE:
            WriterWriteF(path, "/%s", frame->data.bundle.owner->name);
            break;

        case STACK_FRAME_TYPE_PROMISE:
            WriterWriteF(path, "/%s", frame->data.promise.owner->parent_promise_type->name);
            WriterWriteF(path, "/'%s'", frame->data.promise.owner->promiser);
            break;

        case STACK_FRAME_TYPE_PROMISE_ITERATION:
            break;
        }
    }

    return StringWriterClose(path);
}

bool EvalContextVariablePutSpecial(EvalContext *ctx, SpecialScope scope, const char *lval, const void *value, DataType type)
{
    switch (scope)
    {
    case SPECIAL_SCOPE_SYS:
    case SPECIAL_SCOPE_MON:
    case SPECIAL_SCOPE_CONST:
    case SPECIAL_SCOPE_EDIT:
    case SPECIAL_SCOPE_BODY:
    case SPECIAL_SCOPE_THIS:
    case SPECIAL_SCOPE_MATCH:
        {
            VarRef *ref = VarRefParseFromScope(lval, SpecialScopeToString(scope));
            Rval rval = (Rval) { value, DataTypeToRvalType(type) };
            bool ret = EvalContextVariablePut(ctx, ref, rval, type);
            VarRefDestroy(ref);
            return ret;
        }

    default:
        assert(false);
        return false;
    }
}

bool EvalContextVariableRemoveSpecial(const EvalContext *ctx, SpecialScope scope, const char *lval)
{
    switch (scope)
    {
    case SPECIAL_SCOPE_SYS:
    case SPECIAL_SCOPE_MON:
    case SPECIAL_SCOPE_CONST:
    case SPECIAL_SCOPE_EDIT:
    case SPECIAL_SCOPE_BODY:
    case SPECIAL_SCOPE_THIS:
        {
            VarRef *ref = VarRefParseFromScope(lval, SpecialScopeToString(scope));
            bool ret = EvalContextVariableRemove(ctx, ref);
            VarRefDestroy(ref);
            return ret;
        }

    case SPECIAL_SCOPE_NONE:
        assert(false && "Attempted to remove none-special variable");
        return false;

    default:
        assert(false && "Unhandled case in switch");
        return false;
    }
}

static VariableTable *GetVariableTableForVarRef(const EvalContext *ctx, const VarRef *ref)
{

    switch (SpecialScopeFromString(ref->scope))
    {
    case SPECIAL_SCOPE_SYS:
    case SPECIAL_SCOPE_MON:
    case SPECIAL_SCOPE_CONST:
        assert(!ref->ns || strcmp("default", ref->ns) == 0);
        return ctx->global_variables;

    case SPECIAL_SCOPE_MATCH:
        assert(!ref->ns || strcmp("default", ref->ns) == 0);
        return ctx->match_variables;

    case SPECIAL_SCOPE_EDIT:
        assert(!ref->ns || strcmp("default", ref->ns) == 0);
        {
            StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
            assert(frame);
            return frame->data.bundle.vars;
        }

    case SPECIAL_SCOPE_BODY:
        assert(!ref->ns || strcmp("default", ref->ns) == 0);
        {
            StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BODY);
            return frame ? frame->data.body.vars : NULL;
        }

    case SPECIAL_SCOPE_THIS:
        assert(!ref->ns || strcmp("default", ref->ns) == 0);
        {
            StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_PROMISE);
            return frame ? frame->data.promise.vars : NULL;
        }

    case SPECIAL_SCOPE_NONE:
        return ctx->global_variables;

    default:
        assert(false && "Unhandled case in switch");
        return NULL;
    }
}

bool EvalContextVariableRemove(const EvalContext *ctx, const VarRef *ref)
{
    VariableTable *table = GetVariableTableForVarRef(ctx, ref);
    return VariableTableRemove(table, ref);
}

bool EvalContextVariablePut(EvalContext *ctx, const VarRef *ref, Rval rval, DataType type)
{
    assert(type != DATA_TYPE_NONE);
    assert(ref);
    assert(ref->lval);

    if (rval.item == NULL)
    {
        return false;
    }

    if (strlen(ref->lval) > CF_MAXVARSIZE)
    {
        char *lval_str = VarRefToString(ref, true);
        Log(LOG_LEVEL_ERR, "Variable '%s'' cannot be added because its length exceeds the maximum length allowed '%d' characters", lval_str, CF_MAXVARSIZE);
        free(lval_str);
        return false;
    }

    // If we are not expanding a body template, check for recursive singularities
    if (strcmp(ref->scope, "body") != 0)
    {
        switch (rval.type)
        {
        case RVAL_TYPE_SCALAR:
            if (StringContainsVar((char *) rval.item, ref->lval))
            {
                Log(LOG_LEVEL_ERR, "Scalar variable '%s.%s' contains itself (non-convergent), value '%s'", ref->scope, ref->lval,
                      (char *) rval.item);
                return false;
            }
            break;

        case RVAL_TYPE_LIST:
            for (const Rlist *rp = rval.item; rp != NULL; rp = rp->next)
            {
                if (StringContainsVar(rp->item, ref->lval))
                {
                    Log(LOG_LEVEL_ERR, "List variable '%s' contains itself (non-convergent)", ref->lval);
                    return false;
                }
            }
            break;

        default:
            break;
        }
    }

    // Look for outstanding lists in variable rvals
    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        Rlist *listvars = NULL;
        Rlist *scalars = NULL; // TODO what do we do with scalars?

        StackFrame *last_frame = LastStackFrame(ctx, 0);

        if (last_frame && (last_frame->type != STACK_FRAME_TYPE_PROMISE && last_frame->type != STACK_FRAME_TYPE_PROMISE_ITERATION))
        {
            MapIteratorsFromRval(ctx, NULL, &listvars, &scalars, rval);

            if (listvars != NULL)
            {
                Log(LOG_LEVEL_ERR, "Redefinition of variable '%s' (embedded list in RHS)", ref->lval);
            }

            RlistDestroy(listvars);
            RlistDestroy(scalars);
        }
    }

    if (strcmp("this", ref->scope) == 0)
    {
        assert(!ref->ns);
        assert(STACK_FRAME_TYPE_PROMISE == LastStackFrame(ctx, 0)->type || STACK_FRAME_TYPE_PROMISE_ITERATION == LastStackFrame(ctx, 0)->type);
        VariableTable *table = GetVariableTableForVarRef(ctx, ref);
        VariableTablePut(table, ref, &rval, type);
        return true;
    }
    else if (strcmp("match", ref->scope) == 0)
    {
        assert(!ref->ns);
        VariableTable *table = GetVariableTableForVarRef(ctx, ref);
        VariableTablePut(table, ref, &rval, type);
        return true;
    }
    else if (strcmp("body", ref->scope) == 0)
    {
        assert(!ref->ns);
        assert(STACK_FRAME_TYPE_BODY == LastStackFrame(ctx, 0)->type);

        VariableTable *table = GetVariableTableForVarRef(ctx, ref);
        VariableTablePut(table, ref, &rval, type);
        return true;
    }
    else if (strcmp("edit", ref->scope) == 0)
    {
        assert(!ref->ns);
        StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
        assert(frame && "Attempted to add an edit variable outside of any bundle evaluation");

        VariableTable *table = GetVariableTableForVarRef(ctx, ref);
        VariableTablePut(table, ref, &rval, type);
        return true;
    }
    else if (strcmp("const", ref->scope) == 0 || strcmp("sys", ref->scope) == 0 || strcmp("mon", ref->scope) == 0)
    {
        assert(!ref->ns);
        VariableTable *table = GetVariableTableForVarRef(ctx, ref);
        VariableTablePut(table, ref, &rval, type);
        return true;
    }
    else
    {
        VariableTable *table = GetVariableTableForVarRef(ctx, ref);
        VariableTablePut(table, ref, &rval, type);
        return true;
    }

    assert(false && "unknown scope");
    return false;
}

bool EvalContextVariableGet(const EvalContext *ctx, const VarRef *ref, Rval *rval_out, DataType *type_out)
{
    assert(ref);

    if (!ref->lval)
    {
        if (rval_out)
        {
            *rval_out = (Rval) {NULL, RVAL_TYPE_SCALAR };
        }
        if (type_out)
        {
            *type_out = DATA_TYPE_NONE;
        }
        return false;
    }

    if (VarRefIsQualified(ref))
    {
        VariableTable *table = GetVariableTableForVarRef(ctx, ref);
        if (!table)
        {
            if (rval_out)
            {
                *rval_out = (Rval) {NULL, RVAL_TYPE_SCALAR };
            }
            if (type_out)
            {
                *type_out = DATA_TYPE_NONE;
            }
            return false;
        }

        Variable *var = VariableTableGet(table, ref);
        if (var)
        {
            if (rval_out)
            {
                *rval_out = var->rval;
            }
            if (type_out)
            {
                *type_out = var->type;
            }
            return true;
        }
        else
        {
            if (rval_out)
            {
                *rval_out = (Rval) {NULL, RVAL_TYPE_SCALAR };
            }
            if (type_out)
            {
                *type_out = DATA_TYPE_NONE;
            }
            return false;
        }
    }
    else
    {
        StackFrame *last_frame = LastStackFrame(ctx, 0);
        assert(last_frame && "Attempted to push unqualified variable to empty stack");

        switch (last_frame->type)
        {
        case STACK_FRAME_TYPE_BODY:
            {
                VarRef *qref = VarRefCopy(ref);
                qref->scope = xstrdup("body");
                bool ret = EvalContextVariableGet(ctx, qref, rval_out, type_out);
                VarRefDestroy(qref);
                return ret;
            }
            break;

        case STACK_FRAME_TYPE_BUNDLE:
            {
                VarRef *qref = VarRefCopy(ref);
                qref->ns = xstrdup(last_frame->data.bundle.owner->ns);
                qref->scope = xstrdup(last_frame->data.bundle.owner->name);
                bool ret = EvalContextVariableGet(ctx, qref, rval_out, type_out);
                VarRefDestroy(qref);
                return ret;
            }
            break;

        case STACK_FRAME_TYPE_PROMISE:
        case STACK_FRAME_TYPE_PROMISE_ITERATION:
            {
                VarRef *qref = VarRefCopy(ref);
                VarRefQualify(qref, NULL, "this");
                bool ret = EvalContextVariableGet(ctx, qref, rval_out, type_out);
                VarRefDestroy(qref);
                return ret;
            }
            break;
        }
    }

    assert(false);
    return false;
}

bool EvalContextVariableClearMatch(EvalContext *ctx)
{
    return VariableTableClear(ctx->match_variables, NULL, NULL, NULL);
}

VariableTableIterator *EvalContextVariableTableIteratorNew(const EvalContext *ctx, const VarRef *ref)
{
    VariableTable *table = GetVariableTableForVarRef(ctx, ref);
    return table ? VariableTableIteratorNew(table, ref->ns, ref->scope, ref->lval) : NULL;
}

VariableTableIterator *EvalContextVariableTableIteratorNewGlobals(const EvalContext *ctx, const char *ns, const char *scope)
{
    return VariableTableIteratorNew(ctx->global_variables, ns, scope, NULL);
}

bool EvalContextVariableControlCommonGet(const EvalContext *ctx, CommonControl lval, Rval *rval_out)
{
    if (lval == COMMON_CONTROL_NONE)
    {
        return false;
    }

    VarRef *ref = VarRefParseFromScope(CFG_CONTROLBODY[lval].lval, "control_common");
    bool ret = EvalContextVariableGet(ctx, ref, rval_out, NULL);
    VarRefDestroy(ref);
    return ret;
}

bool EvalContextPromiseIsDone(const EvalContext *ctx, const Promise *pp)
{
    return PromiseSetContains(ctx->promises_done, pp);
}

void EvalContextMarkPromiseDone(EvalContext *ctx, const Promise *pp)
{
    PromiseSetAdd(ctx->promises_done, pp->org_pp);
}

void EvalContextMarkPromiseNotDone(EvalContext *ctx, const Promise *pp)
{
    PromiseSetRemove(ctx->promises_done, pp->org_pp);
}



/* cfPS and associated machinery */



/*
 * Internal functions temporarily used from logging implementation
 */

static const char *NO_STATUS_TYPES[] =
    { "vars", "classes", "insert_lines", "delete_lines", "replace_patterns", "field_edits", NULL };
static const char *NO_LOG_TYPES[] =
    { "vars", "classes", "insert_lines", "delete_lines", "replace_patterns", "field_edits", NULL };

/*
 * Vars, classes and similar promises which do not affect the system itself (but
 * just support evalution) do not need to be counted as repaired/failed, as they
 * may change every iteration and introduce lot of churn in reports without
 * giving any value.
 */
static bool IsPromiseValuableForStatus(const Promise *pp)
{
    return pp && (pp->parent_promise_type->name != NULL) && (!IsStrIn(pp->parent_promise_type->name, NO_STATUS_TYPES));
}

/*
 * Vars, classes and subordinate promises (like edit_line) do not need to be
 * logged, as they exist to support other promises.
 */

static bool IsPromiseValuableForLogging(const Promise *pp)
{
    return pp && (pp->parent_promise_type->name != NULL) && (!IsStrIn(pp->parent_promise_type->name, NO_LOG_TYPES));
}

static void AddAllClasses(EvalContext *ctx, const char *ns, const Rlist *list, unsigned int persistence_ttl, ContextStatePolicy policy, ContextScope context_scope)
{
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        char *classname = xstrdup(rp->item);

        CanonifyNameInPlace(classname);

        if (EvalContextHeapContainsHard(ctx, classname))
        {
            Log(LOG_LEVEL_ERR, "You cannot use reserved hard class '%s' as post-condition class", classname);
            // TODO: ok.. but should we take any action? continue; maybe?
        }

        if (persistence_ttl > 0)
        {
            if (context_scope != CONTEXT_SCOPE_NAMESPACE)
            {
                Log(LOG_LEVEL_INFO, "Automatically promoting context scope for '%s' to namespace visibility, due to persistence", classname);
            }

            Log(LOG_LEVEL_VERBOSE, "Defining persistent promise result class '%s'", classname);
            EvalContextHeapPersistentSave(CanonifyName(rp->item), ns, persistence_ttl, policy);
            EvalContextHeapAddSoft(ctx, classname, ns);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Defining promise result class '%s'", classname);

            switch (context_scope)
            {
            case CONTEXT_SCOPE_BUNDLE:
                EvalContextStackFrameAddSoft(ctx, classname);
                break;

            default:
            case CONTEXT_SCOPE_NAMESPACE:
                EvalContextHeapAddSoft(ctx, classname, ns);
                break;
            }
        }
    }
}

static void DeleteAllClasses(EvalContext *ctx, const Rlist *list)
{
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (CheckParseContext((char *) rp->item, CF_IDRANGE) != SYNTAX_TYPE_MATCH_OK)
        {
            return; // TODO: interesting course of action, but why is the check there in the first place?
        }

        if (EvalContextHeapContainsHard(ctx, (char *) rp->item))
        {
            Log(LOG_LEVEL_ERR, "You cannot cancel a reserved hard class '%s' in post-condition classes",
                  RlistScalarValue(rp));
        }

        const char *string = (char *) (rp->item);

        Log(LOG_LEVEL_VERBOSE, "Cancelling class '%s'", string);

        EvalContextHeapPersistentRemove(string);

        EvalContextHeapRemoveSoft(ctx, CanonifyName(string));

        EvalContextStackFrameAddNegated(ctx, CanonifyName(string));
    }
}

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, TrackTotalCompliance, ARG_UNUSED PromiseResult, status, ARG_UNUSED const Promise *, pp)
{
}

static void SetPromiseOutcomeClasses(PromiseResult status, EvalContext *ctx, const Promise *pp, DefineClasses dc)
{
    Rlist *add_classes = NULL;
    Rlist *del_classes = NULL;

    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        add_classes = dc.change;
        del_classes = dc.del_change;
        break;

    case PROMISE_RESULT_TIMEOUT:
        add_classes = dc.timeout;
        del_classes = dc.del_notkept;
        break;

    case PROMISE_RESULT_WARN:
    case PROMISE_RESULT_FAIL:
        add_classes = dc.failure;
        del_classes = dc.del_notkept;
        break;

    case PROMISE_RESULT_DENIED:
        add_classes = dc.denied;
        del_classes = dc.del_notkept;
        break;

    case PROMISE_RESULT_INTERRUPTED:
        add_classes = dc.interrupt;
        del_classes = dc.del_notkept;
        break;

    case PROMISE_RESULT_NOOP:
        add_classes = dc.kept;
        del_classes = dc.del_kept;
        break;

    default:
        ProgrammingError("Unexpected status '%c' has been passed to SetPromiseOutcomeClasses", status);
    }

    AddAllClasses(ctx, PromiseGetNamespace(pp), add_classes, dc.persist, dc.timer, dc.scope);
    DeleteAllClasses(ctx, del_classes);
}

static void UpdatePromiseComplianceStatus(PromiseResult status, const Promise *pp, char *reason)
{
    if (!IsPromiseValuableForLogging(pp))
    {
        return;
    }

    char compliance_status;

    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        compliance_status = PROMISE_STATE_REPAIRED;
        break;

    case PROMISE_RESULT_WARN:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_INTERRUPTED:
        compliance_status = PROMISE_STATE_NOTKEPT;
        break;

    case PROMISE_RESULT_NOOP:
        compliance_status = PROMISE_STATE_ANY;
        break;

    default:
        ProgrammingError("Unknown status '%c' has been passed to UpdatePromiseComplianceStatus", status);
    }

    NotePromiseCompliance(pp, compliance_status, reason);
}

static void SummarizeTransaction(EvalContext *ctx, TransactionContext tc, const char *logname)
{
    if (logname && (tc.log_string))
    {
        char buffer[CF_EXPANDSIZE];

        ExpandScalar(ctx, NULL, NULL, tc.log_string, buffer);

        if (strcmp(logname, "udp_syslog") == 0)
        {
            RemoteSysLog(tc.log_priority, buffer);
        }
        else if (strcmp(logname, "stdout") == 0)
        {
            Log(LOG_LEVEL_INFO, "L: %s", buffer);
        }
        else
        {
            struct stat dsb;

            // Does the file exist already?
            if (lstat(logname, &dsb) == -1)
            {
                mode_t filemode = 0600;     /* Mode for log file creation */
                int fd = creat(logname, filemode);
                if (fd >= 0)
                {
                    Log(LOG_LEVEL_VERBOSE, "Created log file '%s' with requested permissions %o", logname, filemode);
                    close(fd);
                }
            }

            FILE *fout = fopen(logname, "a");

            if (fout == NULL)
            {
                Log(LOG_LEVEL_ERR, "Unable to open private log '%s'", logname);
                return;
            }

            Log(LOG_LEVEL_VERBOSE, "Logging string '%s' to '%s'", buffer, logname);
            fprintf(fout, "%s\n", buffer);

            fclose(fout);
        }

        tc.log_string = NULL;     /* To avoid repetition */
    }
}

static void DoSummarizeTransaction(EvalContext *ctx, PromiseResult status, const Promise *pp, TransactionContext tc)
{
    if (!IsPromiseValuableForLogging(pp))
    {
        return;
    }

    char *log_name;

    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        log_name = tc.log_repaired;
        break;

    case PROMISE_RESULT_WARN:
        /* FIXME: nothing? */
        return;

    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_INTERRUPTED:
        log_name = tc.log_failed;
        break;

    case PROMISE_RESULT_NOOP:
        log_name = tc.log_kept;
        break;
    }

    SummarizeTransaction(ctx, tc, log_name);
}

static void NotifyDependantPromises(PromiseResult status, EvalContext *ctx, const Promise *pp)
{
    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
    case PROMISE_RESULT_NOOP:
        MarkPromiseHandleDone(ctx, pp);
        break;

    default:
        /* This promise is not yet done, don't mark it is as such */
        break;
    }
}

void ClassAuditLog(EvalContext *ctx, const Promise *pp, Attributes attr, PromiseResult status)
{
    if (IsPromiseValuableForStatus(pp))
    {
        TrackTotalCompliance(status, pp);
        UpdatePromiseCounters(status, attr.transaction);
    }

    SetPromiseOutcomeClasses(status, ctx, pp, attr.classes);
    NotifyDependantPromises(status, ctx, pp);
    DoSummarizeTransaction(ctx, status, pp, attr.transaction);
}

static void LogPromiseContext(const EvalContext *ctx, const Promise *pp)
{
    Writer *w = StringWriter();
    WriterWrite(w, "Additional promise info:");
    if (PromiseGetHandle(pp))
    {
        WriterWriteF(w, " handle '%s'", PromiseGetHandle(pp));
    }

    {

        Rval retval;
        if (EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_VERSION, &retval))
        {
            WriterWriteF(w, " version '%s'", RvalScalarValue(retval));
        }
    }

    if (PromiseGetBundle(pp)->source_path)
    {
        WriterWriteF(w, " source path '%s' at line %zu", PromiseGetBundle(pp)->source_path, pp->offset.line);
    }

    switch (pp->promisee.type)
    {
    case RVAL_TYPE_SCALAR:
        WriterWriteF(w, " promisee '%s'", RvalScalarValue(pp->promisee));
        break;

    case RVAL_TYPE_LIST:
        WriterWrite(w, " promisee ");
        RlistWrite(w, pp->promisee.item);
        break;
    default:
        break;
    }

    if (pp->comment)
    {
        WriterWriteF(w, " comment '%s'", pp->comment);
    }

    Log(LOG_LEVEL_VERBOSE, "%s", StringWriterData(w));
    WriterClose(w);
}

void cfPS(EvalContext *ctx, LogLevel level, PromiseResult status, const Promise *pp, Attributes attr, const char *fmt, ...)
{
    /*
     * This stub implementation of cfPS delegates to the new logging backend.
     *
     * Due to the fact very little of the code has been converted, this code
     * does a full initialization and shutdown of logging subsystem for each
     * cfPS.
     *
     * Instead, LoggingInit should be called at the moment EvalContext is
     * created, LoggingPromiseEnter/LoggingPromiseFinish should be called around
     * ExpandPromise and LoggingFinish should be called when EvalContext is
     * going to be destroyed.
     *
     * But it requires all calls to cfPS to be eliminated.
     */

    /* FIXME: Ensure that NULL pp is never passed into cfPS */

    if (pp)
    {
        PromiseLoggingInit(ctx);
        PromiseLoggingPromiseEnter(ctx, pp);

        if (level >= LOG_LEVEL_VERBOSE)
        {
            LogPromiseContext(ctx, pp);
        }
    }

    va_list ap;
    va_start(ap, fmt);
    VLog(level, fmt, ap);
    va_end(ap);

    if (pp)
    {
        char *last_msg = PromiseLoggingPromiseFinish(ctx, pp);
        PromiseLoggingFinish(ctx);

        /* Now complete the exits status classes and auditing */

        ClassAuditLog(ctx, pp, attr, status);
        UpdatePromiseComplianceStatus(status, pp, last_msg);

        free(last_msg);
    }
}
