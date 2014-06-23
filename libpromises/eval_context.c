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

#include <eval_context.h>

#include <files_names.h>
#include <logic_expressions.h>
#include <syntax.h>
#include <item_lib.h>
#include <ornaments.h>
#include <expand.h>
#include <matching.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <file_lib.h>
#include <assoc.h>
#include <scope.h>
#include <vars.h>
#include <syslog_client.h>
#include <audit.h>
#include <rlist.h>
#include <buffer.h>
#include <promises.h>
#include <fncall.h>
#include <ring_buffer.h>
#include <logging_priv.h>
#include <known_dirs.h>
#include <printsize.h>

static bool BundleAborted(const EvalContext *ctx);
static void SetBundleAborted(EvalContext *ctx);

static bool EvalContextStackFrameContainsSoft(const EvalContext *ctx, const char *context);
static bool EvalContextHeapContainsSoft(const EvalContext *ctx, const char *ns, const char *name);
static bool EvalContextHeapContainsHard(const EvalContext *ctx, const char *name);
static bool EvalContextClassPut(EvalContext *ctx, const char *ns, const char *name, bool is_soft, ContextScope scope, const char *tags);
static const char *EvalContextCurrentNamespace(const EvalContext *ctx);
static ClassRef IDRefQualify(const EvalContext *ctx, const char *id);


struct EvalContext_
{
    int eval_options;
    bool bundle_aborted;
    bool checksum_updates_default;
    Item *ip_addresses;
    bool ignore_locks;

    Item *heap_abort;
    Item *heap_abort_current_bundle;

    Seq *stack;

    ClassTable *global_classes;
    VariableTable *global_variables;

    VariableTable *match_variables;

    StringSet *promise_lock_cache;
    StringSet *dependency_handles;
    RBTree *function_cache;

    uid_t uid;
    uid_t gid;
    pid_t pid;
    pid_t ppid;

    // Full path to directory that the binary was launched from.
    char *launch_directory;
};

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

static LogLevel AdjustLogLevel(LogLevel base, LogLevel adjust)
{
    if (adjust == -1)
    {
        return base;
    }
    else
    {
        return MAX(base, adjust);
    }
}

static LogLevel StringToLogLevel(const char *value)
{
    if (value)
    {
        if (!strcmp(value, "verbose"))
        {
            return LOG_LEVEL_VERBOSE;
        }
        if (!strcmp(value, "inform"))
        {
            return LOG_LEVEL_INFO;
        }
        if (!strcmp(value, "error"))
        {
            return LOG_LEVEL_NOTICE; /* Error level includes warnings and notices */
        }
    }
    return -1;
}

static LogLevel GetLevelForPromise(const Promise *pp, const char *attr_name)
{
    return StringToLogLevel(PromiseGetConstraintAsRval(pp, attr_name, RVAL_TYPE_SCALAR));
}

static LogLevel CalculateLogLevel(const Promise *pp)
{
    LogLevel log_level = LogGetGlobalLevel();

    if (pp)
    {
        log_level = AdjustLogLevel(log_level, GetLevelForPromise(pp, "log_level"));
    }

    /* Disable system log for dry-runs */
    if (DONTDO)
    {
        log_level = LOG_LEVEL_NOTHING;
    }

    return log_level;
}

static LogLevel CalculateReportLevel(const Promise *pp)
{
    LogLevel report_level = LogGetGlobalLevel();

    if (pp)
    {
        report_level = AdjustLogLevel(report_level, GetLevelForPromise(pp, "report_level"));
    }

    return report_level;
}

static char *LogHook(LoggingPrivContext *pctx, LogLevel level, const char *message)
{
    const EvalContext *ctx = pctx->param;

    StackFrame *last_frame = LastStackFrame(ctx, 0);
    if (last_frame)
    {
        if (last_frame->type == STACK_FRAME_TYPE_PROMISE_ITERATION)
        {
            if (level <= LOG_LEVEL_INFO)
            {
                RingBufferAppend(last_frame->data.promise_iteration.log_messages, xstrdup(message));
            }
        }

        return StringConcatenate(3, last_frame->path, ": ", message);
    }
    else
    {
        return xstrdup(message);
    }
}

static const char *GetAgentAbortingContext(const EvalContext *ctx)
{
    for (const Item *ip = ctx->heap_abort; ip != NULL; ip = ip->next)
    {
        if (IsDefinedClass(ctx, ip->classes))
        {
            const char *regex = ip->name;
            Class *cls = EvalContextClassMatch(ctx, regex);

            if (cls)
            {
                return regex;
            }
        }
    }
    return NULL;
}

static void EvalContextStackFrameAddSoft(EvalContext *ctx, const char *context, const char *tags)
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
         strlcpy(copy, context, CF_MAXVARSIZE);
    }

    if (Chop(copy, CF_EXPANDSIZE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
    }

    if (strlen(copy) == 0)
    {
        return;
    }

    if (EvalContextHeapContainsSoft(ctx, frame.owner->ns, context))
    {
        Log(LOG_LEVEL_WARNING, "Private class '%s' in bundle '%s' shadows a global class - you should choose a different name to avoid conflicts",
              copy, frame.owner->name);
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort_current_bundle, copy))
    {
        Log(LOG_LEVEL_ERR, "Bundle aborted on defined class '%s'", copy);
        SetBundleAborted(ctx);
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort, copy))
    {
        FatalError(ctx, "cf-agent aborted on defined class '%s'", copy);
    }

    if (EvalContextStackFrameContainsSoft(ctx, copy))
    {
        return;
    }

    ClassTablePut(frame.classes, frame.owner->ns, context, true, CONTEXT_SCOPE_BUNDLE, tags);

    if (!BundleAborted(ctx))
    {
        for (const Item *ip = ctx->heap_abort_current_bundle; ip != NULL; ip = ip->next)
        {
            if (IsDefinedClass(ctx, ip->name))
            {
                Log(LOG_LEVEL_ERR, "Setting abort for '%s' when setting '%s'", ip->name, context);
                SetBundleAborted(ctx);
                break;
            }
        }
    }
}

static ExpressionValue EvalTokenAsClass(const char *classname, void *param)
{
    const EvalContext *ctx = param;
    ClassRef ref = ClassRefParse(classname);
    if (ClassRefIsQualified(ref))
    {
        if (strcmp(ref.ns, NamespaceDefault()) == 0)
        {
            if (EvalContextHeapContainsHard(ctx, ref.name))
            {
                ClassRefDestroy(ref);
                return true;
            }
        }
    }
    else
    {
        if (EvalContextHeapContainsHard(ctx, ref.name))
        {
            ClassRefDestroy(ref);
            return true;
        }

        const char *ns = EvalContextCurrentNamespace(ctx);
        if (ns)
        {
            ClassRefQualify(&ref, ns);
        }
        else
        {
            ClassRefQualify(&ref, NamespaceDefault());
        }
    }

    assert(ClassRefIsQualified(ref));
    bool classy = (strcmp("any", ref.name) == 0 ||
                   EvalContextHeapContainsSoft(ctx, ref.ns, ref.name) ||
                   EvalContextStackFrameContainsSoft(ctx, ref.name));

    ClassRefDestroy(ref);
    return classy; /* ExpressionValue is just an enum extending bool... */
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

bool IsDefinedClass(const EvalContext *ctx, const char *context)
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
        ExpressionValue r = EvalExpression(res.result,
                                           &EvalTokenAsClass, &EvalVarRef,
                                           (void *)ctx); // controlled cast. None of these should modify EvalContext

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


void EvalContextHeapPersistentSave(EvalContext *ctx, const char *name, unsigned int ttl_minutes,
                                   PersistentClassPolicy policy, const char *tags)
{
    assert(tags);

    time_t now = time(NULL);

    CF_DB *dbp;
    if (!OpenDB(&dbp, dbid_state))
    {
        char *db_path = DBIdToPath(GetWorkDir(), dbid_state);
        Log(LOG_LEVEL_ERR, "While persisting class, unable to open database at '%s' (OpenDB: %s)",
            db_path, GetErrorStr());
        free(db_path);
        return;
    }

    ClassRef ref = IDRefQualify(ctx, name);
    char *key = ClassRefToString(ref.ns, ref.name);
    ClassRefDestroy(ref);
    
    size_t tags_length = strlen(tags) + 1;
    size_t new_info_size = sizeof(PersistentClassInfo) + tags_length;

    PersistentClassInfo *new_info = xcalloc(1, new_info_size);

    new_info->expires = now + ttl_minutes * 60;
    new_info->policy = policy;
    strlcpy(new_info->tags, tags, tags_length);

    // first see if we have an existing record, and if we should bother to update
    {
        int existing_info_size = ValueSizeDB(dbp, key, strlen(key));
        if (existing_info_size > 0)
        {
            PersistentClassInfo *existing_info = xcalloc(existing_info_size, 1);
            if (ReadDB(dbp, key, existing_info, existing_info_size))
            {
                if (existing_info->policy == CONTEXT_STATE_POLICY_PRESERVE &&
                    now < existing_info->expires &&
                    strcmp(existing_info->tags, new_info->tags) == 0)
                {
                    Log(LOG_LEVEL_VERBOSE, "Persisent class '%s' is already in a preserved state --  %jd minutes to go",
                        key, (intmax_t)((existing_info->expires - now) / 60));
                    CloseDB(dbp);
                    free(key);
                    free(new_info);
                    return;
                }
            }
            else
            {
                Log(LOG_LEVEL_ERR, "While persisting class '%s', error reading existing value", key);
                CloseDB(dbp);
                free(key);
                free(new_info);
                return;
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Updating persistent class '%s'", key);

    WriteDB(dbp, key, new_info, new_info_size);

    CloseDB(dbp);
    free(key);
    free(new_info);
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
    time_t now = time(NULL);

    Banner("Loading persistent classes");

    CF_DB *dbp;
    if (!OpenDB(&dbp, dbid_state))
    {
        return;
    }

    CF_DBC *dbcp;
    if (!NewDBCursor(dbp, &dbcp))
    {
        Log(LOG_LEVEL_INFO, "Unable to scan persistence cache");
        return;
    }

    const char *key;
    int key_size = 0;
    void *info_p;
    int info_size = 0;

    while (NextDB(dbcp, (char **)&key, &key_size, &info_p, &info_size))
    {
        Log(LOG_LEVEL_DEBUG, "Found key persistent class key '%s'", key);

        /* Info points to db-owned data, which is not aligned properly and
         * dereferencing might be slow or even cause SIGBUS! */
        PersistentClassInfo info = { 0 };
        memcpy(&info, info_p,
               info_size < sizeof(info) ? info_size : sizeof(info));

        const char *tags = NULL;
        if (info_size > sizeof(PersistentClassInfo))
        {
            /* This is char pointer, it can point to unaligned data. */
            tags = ((PersistentClassInfo *) info_p)->tags;
        }

        if (now > info.expires)
        {
            Log(LOG_LEVEL_VERBOSE, "Persistent class '%s' expired", key);
            DBCursorDeleteEntry(dbcp);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Persistent class '%s' for %jd more minutes",
                key, (intmax_t) ((info.expires - now) / 60));
            Log(LOG_LEVEL_VERBOSE, "Adding persistent class '%s' to heap", key);

            ClassRef ref = ClassRefParse(key);
            EvalContextClassPut(ctx, ref.ns, ref.name, true, CONTEXT_SCOPE_NAMESPACE, tags);

            StringSet *tag_set = EvalContextClassTags(ctx, ref.ns, ref.name);
            assert(tag_set);

            StringSetAdd(tag_set, xstrdup("source=persistent"));

            ClassRefDestroy(ref);
        }
    }

    DeleteDBCursor(dbcp);
    CloseDB(dbp);

    Banner("Loaded persistent memory");
}

bool Abort(EvalContext *ctx)
{
    if (ctx->bundle_aborted)
    {
        ctx->bundle_aborted = false;
        return true;
    }

    return false;
}

bool BundleAborted(const EvalContext* ctx)
{
    return ctx->bundle_aborted;
}

void SetBundleAborted(EvalContext *ctx)
{
    ctx->bundle_aborted = true;
}

bool VarClassExcluded(const EvalContext *ctx, const Promise *pp, char **classes)
{
    Constraint *cp = PromiseGetConstraint(pp, "ifvarclass");
    if (!cp)
    {
        return false;
    }

    if (cp->rval.type == RVAL_TYPE_FNCALL)
    {
        return false;
    }

    *classes = PromiseGetConstraintAsRval(pp, "ifvarclass", RVAL_TYPE_SCALAR);

    if (*classes == NULL)
    {
        return true;
    }

    if (strchr(*classes, '$') || strchr(*classes, '@'))
    {
        Log(LOG_LEVEL_DEBUG, "Class expression did not evaluate");
        return true;
    }

    if (*classes && IsDefinedClass(ctx, *classes))
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool EvalContextPromiseIsActive(const EvalContext *ctx, const Promise *pp)
{
    if (!IsDefinedClass(ctx, pp->classes))
    {
        return false;
    }
    else
    {
        char *classes = NULL;
        if (VarClassExcluded(ctx, pp, &classes))
        {
            return false;
        }
    }

    return true;
}

void EvalContextHeapAddAbort(EvalContext *ctx, const char *context, const char *activated_on_context)
{
    if (!IsItemIn(ctx->heap_abort, context))
    {
        AppendItem(&ctx->heap_abort, context, activated_on_context);
    }

    if (GetAgentAbortingContext(ctx))
    {
        FatalError(ctx, "cf-agent aborted on context '%s'", GetAgentAbortingContext(ctx));
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

bool MissingDependencies(EvalContext *ctx, const Promise *pp)
{
    const Rlist *dependenies = PromiseGetConstraintAsList(ctx, "depends_on", pp);
    if (RlistIsNullList(dependenies))
    {
        return false;
    }

    for (const Rlist *rp = PromiseGetConstraintAsList(ctx, "depends_on", pp); rp; rp = rp->next)
    {
        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            return true;
        }

        if (!StringSetContains(ctx->dependency_handles, RlistScalarValue(rp)))
        {
            if (LEGACY_OUTPUT)
            {
                Log(LOG_LEVEL_VERBOSE, "\n");
                Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
                Log(LOG_LEVEL_VERBOSE, "Skipping whole next promise (%s), as promise dependency %s has not yet been kept",
                    pp->promiser, RlistScalarValue(rp));
                Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Skipping next promise '%s', as promise dependency '%s' has not yet been kept",
                    pp->promiser, RlistScalarValue(rp));
            }

            return true;
        }
    }

    return false;
}

static void StackFrameBundleDestroy(StackFrameBundle frame)
{
    ClassTableDestroy(frame.classes);
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

static void StackFramePromiseIterationDestroy(StackFramePromiseIteration frame)
{
    PromiseDestroy(frame.owner);
    RingBufferDestroy(frame.log_messages);
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

        case STACK_FRAME_TYPE_PROMISE_TYPE:
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

        free(frame->path);
        free(frame);
    }
}

EvalContext *EvalContextNew(void)
{
    EvalContext *ctx = xmalloc(sizeof(EvalContext));

    ctx->eval_options = EVAL_OPTION_FULL;
    ctx->bundle_aborted = false;
    ctx->checksum_updates_default = false;
    ctx->ip_addresses = NULL;
    ctx->ignore_locks = false;

    ctx->heap_abort = NULL;
    ctx->heap_abort_current_bundle = NULL;

    ctx->stack = SeqNew(10, StackFrameDestroy);

    ctx->global_classes = ClassTableNew();

    ctx->global_variables = VariableTableNew();
    ctx->match_variables = VariableTableNew();

    ctx->dependency_handles = StringSetNew();

    ctx->uid = getuid();
    ctx->gid = getgid();
    ctx->pid = getpid();

#ifdef __MINGW32__
    ctx->ppid = 0;
#else
    ctx->ppid = getppid();
#endif

    ctx->promise_lock_cache = StringSetNew();
    ctx->function_cache = RBTreeNew(NULL, NULL, NULL,
                                    NULL, NULL, NULL);

    {
        LoggingPrivContext *pctx = LoggingPrivGetContext();
        assert(!pctx && "Logging context bound to something else");

        pctx = xcalloc(1, sizeof(LoggingPrivContext));
        pctx->param = ctx;
        pctx->log_hook = &LogHook;

        LoggingPrivSetContext(pctx);
    }

    ctx->launch_directory = NULL;

    return ctx;
}

void EvalContextDestroy(EvalContext *ctx)
{
    if (ctx)
    {
        free(ctx->launch_directory);

        {
            LoggingPrivContext *pctx = LoggingPrivGetContext();
            free(pctx);
            LoggingPrivSetContext(NULL);
        }

        EvalContextDeleteIpAddresses(ctx);

        DeleteItemList(ctx->heap_abort);
        DeleteItemList(ctx->heap_abort_current_bundle);

        SeqDestroy(ctx->stack);

        ClassTableDestroy(ctx->global_classes);
        VariableTableDestroy(ctx->global_variables);
        VariableTableDestroy(ctx->match_variables);

        StringSetDestroy(ctx->dependency_handles);
        StringSetDestroy(ctx->promise_lock_cache);

        {
            RBTreeIterator *it = RBTreeIteratorNew(ctx->function_cache);
            Rval *rval = NULL;
            while (RBTreeIteratorNext(it, NULL, (void **)&rval))
            {
                RvalDestroy(*rval);
                free(rval);
            }
            RBTreeIteratorDestroy(it);
            RBTreeDestroy(ctx->function_cache);
        }

        free(ctx);
    }
}

static bool EvalContextHeapContainsSoft(const EvalContext *ctx, const char *ns, const char *name)
{
    const Class *cls = ClassTableGet(ctx->global_classes, ns, name);
    return cls && cls->is_soft;
}

static bool EvalContextHeapContainsHard(const EvalContext *ctx, const char *name)
{
    const Class *cls = ClassTableGet(ctx->global_classes, NULL, name);
    return cls && !cls->is_soft;
}

bool StackFrameContainsSoftRecursive(const EvalContext *ctx, const char *context, size_t stack_index)
{
    StackFrame *frame = SeqAt(ctx->stack, stack_index);
    if (frame->type == STACK_FRAME_TYPE_BUNDLE && ClassTableGet(frame->data.bundle.classes, frame->data.bundle.owner->ns, context) != NULL)
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

static bool EvalContextStackFrameContainsSoft(const EvalContext *ctx, const char *context)
{
    if (SeqLength(ctx->stack) == 0)
    {
        return false;
    }

    size_t stack_index = SeqLength(ctx->stack) - 1;
    return StackFrameContainsSoftRecursive(ctx, context, stack_index);
}

bool EvalContextHeapRemoveSoft(EvalContext *ctx, const char *ns, const char *name)
{
    return ClassTableRemove(ctx->global_classes, ns, name);
}

bool EvalContextHeapRemoveHard(EvalContext *ctx, const char *name)
{
    return ClassTableRemove(ctx->global_classes, NULL, name);
}

void EvalContextClear(EvalContext *ctx)
{
    ClassTableClear(ctx->global_classes);
    EvalContextDeleteIpAddresses(ctx);
    VariableTableClear(ctx->global_variables, NULL, NULL, NULL);
    VariableTableClear(ctx->match_variables, NULL, NULL, NULL);
    StringSetClear(ctx->promise_lock_cache);
    SeqClear(ctx->stack);

    {
        RBTreeIterator *it = RBTreeIteratorNew(ctx->function_cache);
        Rval *rval = NULL;
        while (RBTreeIteratorNext(it, NULL, (void **)&rval))
        {
            RvalDestroy(*rval);
            free(rval);
        }
        RBTreeIteratorDestroy(it);
        RBTreeClear(ctx->function_cache);
    }

}

static StackFrame *StackFrameNew(StackFrameType type, bool inherit_previous)
{
    StackFrame *frame = xmalloc(sizeof(StackFrame));

    frame->type = type;
    frame->inherits_previous = inherit_previous;
    frame->path = NULL;

    return frame;
}

static StackFrame *StackFrameNewBundle(const Bundle *owner, bool inherit_previous)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_BUNDLE, inherit_previous);

    frame->data.bundle.owner = owner;
    frame->data.bundle.classes = ClassTableNew();
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

static StackFrame *StackFrameNewPromiseType(const PromiseType *owner)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_PROMISE_TYPE, true);

    frame->data.promise_type.owner = owner;

    return frame;
}

static StackFrame *StackFrameNewPromise(const Promise *owner)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_PROMISE, true);

    frame->data.promise.owner = owner;

    return frame;
}

static StackFrame *StackFrameNewPromiseIteration(Promise *owner, const PromiseIterator *iter_ctx, unsigned index)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_PROMISE_ITERATION, true);

    frame->data.promise_iteration.owner = owner;
    frame->data.promise_iteration.iter_ctx = iter_ctx;
    frame->data.promise_iteration.index = index;
    frame->data.promise_iteration.log_messages = RingBufferNew(5, NULL, free);

    return frame;
}

void EvalContextStackFrameRemoveSoft(EvalContext *ctx, const char *context)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    assert(frame);

    ClassTableRemove(frame->data.bundle.classes, frame->data.bundle.owner->ns, context);
}

static void EvalContextStackPushFrame(EvalContext *ctx, StackFrame *frame)
{
    StackFrame *last_frame = LastStackFrame(ctx, 0);
    if (last_frame)
    {
        if (last_frame->type == STACK_FRAME_TYPE_PROMISE_ITERATION)
        {
            LoggingPrivSetLevels(LogGetGlobalLevel(), LogGetGlobalLevel());
        }
    }

    SeqAppend(ctx->stack, frame);

    assert(!frame->path);
    frame->path = EvalContextStackPath(ctx);
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
            Rval retval = ExpandPrivateRval(ctx, owner->ns, owner->name, var->rval.item, var->rval.type);
            RvalDestroy(var->rval);
            var->rval = retval;
        }
        VariableTableIteratorDestroy(iter);
    }
}

void EvalContextStackPushBodyFrame(EvalContext *ctx, const Promise *caller, const Body *body, const Rlist *args)
{
#ifndef NDEBUG
    StackFrame *last_frame = LastStackFrame(ctx, 0);
    if (last_frame)
    {
        assert(last_frame->type == STACK_FRAME_TYPE_PROMISE_TYPE);
    }
    else
    {
        assert(strcmp("control", body->name) == 0);
    }
#endif


    EvalContextStackPushFrame(ctx, StackFrameNewBody(body));

    if (RlistLen(body->args) != RlistLen(args))
    {
        if (caller)
        {
            Log(LOG_LEVEL_ERR, "Argument arity mismatch in body '%s' at line %zu in file '%s', expected %d, got %d",
                body->name, caller->offset.line, PromiseGetBundle(caller)->source_path, RlistLen(body->args), RlistLen(args));
        }
        else
        {
            assert(strcmp("control", body->name) == 0);
            ProgrammingError("Control body stack frame was pushed with arguments. This should have been caught before");
        }
        return;
    }
    else
    {
        ScopeMapBodyArgs(ctx, body, args);
    }
}

void EvalContextStackPushPromiseTypeFrame(EvalContext *ctx, const PromiseType *owner)
{
    assert(LastStackFrame(ctx, 0) && LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_BUNDLE);

    StackFrame *frame = StackFrameNewPromiseType(owner);
    EvalContextStackPushFrame(ctx, frame);
}

void EvalContextStackPushPromiseFrame(EvalContext *ctx, const Promise *owner, bool copy_bundle_context)
{
    assert(LastStackFrame(ctx, 0));
    assert(LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_PROMISE_TYPE);

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

    if (PromiseGetBundle(owner)->source_path)
    {
        char path[CF_BUFSIZE];
        if (!IsAbsoluteFileName(PromiseGetBundle(owner)->source_path) && ctx->launch_directory)
        {
            snprintf(path, CF_BUFSIZE, "%s%c%s", ctx->launch_directory, FILE_SEPARATOR, PromiseGetBundle(owner)->source_path);
        }
        else
        {
            strlcpy(path, PromiseGetBundle(owner)->source_path, CF_BUFSIZE);
        }

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promise_filename", path, CF_DATA_TYPE_STRING, "source=promise");

        // We now make path just the directory name!
        DeleteSlash(path);
        ChopLastNode(path);

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promise_dirname", path, CF_DATA_TYPE_STRING, "source=promise");
        char number[CF_SMALLBUF];
        snprintf(number, CF_SMALLBUF, "%llu", (unsigned long long)owner->offset.line);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promise_linenumber", number, CF_DATA_TYPE_STRING, "source=promise");
    }

    char v[PRINTSIZE(int)];
    sprintf(v, "%d", (int) ctx->uid);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_uid", v, CF_DATA_TYPE_INT, "source=agent");
    sprintf(v, "%d", (int) ctx->gid);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_gid", v, CF_DATA_TYPE_INT, "source=agent");
    sprintf(v, "%d", (int) ctx->pid);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_pid", v, CF_DATA_TYPE_INT, "source=agent");
    sprintf(v, "%d", (int) ctx->ppid);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_ppid", v, CF_DATA_TYPE_INT, "source=agent");

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "bundle", PromiseGetBundle(owner)->name, CF_DATA_TYPE_STRING, "source=promise");
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "namespace", PromiseGetNamespace(owner), CF_DATA_TYPE_STRING, "source=promise");
}

Promise *EvalContextStackPushPromiseIterationFrame(EvalContext *ctx, size_t iteration_index, const PromiseIterator *iter_ctx)
{
    assert(LastStackFrame(ctx, 0) && LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_PROMISE);

    if (iter_ctx)
    {
        PromiseIteratorUpdateVariable(ctx, iter_ctx);
    }

    bool excluded = false;
    Promise *pexp = ExpandDeRefPromise(ctx, LastStackFrame(ctx, 0)->data.promise.owner, &excluded);
    if (excluded)
    {
        PromiseDestroy(pexp);
        return NULL;
    }

    EvalContextStackPushFrame(ctx, StackFrameNewPromiseIteration(pexp, iter_ctx, iteration_index));

    LoggingPrivSetLevels(CalculateLogLevel(pexp), CalculateReportLevel(pexp));

    return pexp;
}

void EvalContextStackPopFrame(EvalContext *ctx)
{
    assert(SeqLength(ctx->stack) > 0);

    StackFrame *last_frame = LastStackFrame(ctx, 0);
    StackFrameType last_frame_type = last_frame->type;

    switch (last_frame_type)
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

    case STACK_FRAME_TYPE_PROMISE_ITERATION:
        LoggingPrivSetLevels(LogGetGlobalLevel(), LogGetGlobalLevel());
        break;

    default:
        break;
    }

    SeqRemove(ctx->stack, SeqLength(ctx->stack) - 1);

    last_frame = LastStackFrame(ctx, 0);
    if (last_frame)
    {
        if (last_frame->type == STACK_FRAME_TYPE_PROMISE_ITERATION)
        {
            const Promise *pp = EvalContextStackCurrentPromise(ctx);
            LoggingPrivSetLevels(CalculateLogLevel(pp), CalculateReportLevel(pp));
        }
    }
}

bool EvalContextClassRemove(EvalContext *ctx, const char *ns, const char *name)
{
    for (size_t i = 0; i < SeqLength(ctx->stack); i++)
    {
        StackFrame *frame = SeqAt(ctx->stack, i);
        if (frame->type != STACK_FRAME_TYPE_BUNDLE)
        {
            continue;
        }

        ClassTableRemove(frame->data.bundle.classes, ns, name);
    }

    return ClassTableRemove(ctx->global_classes, ns, name);
}

Class *EvalContextClassGet(const EvalContext *ctx, const char *ns, const char *name)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    if (frame)
    {
        Class *cls = ClassTableGet(frame->data.bundle.classes, ns, name);
        if (cls)
        {
            return cls;
        }
    }

    return ClassTableGet(ctx->global_classes, ns, name);
}

Class *EvalContextClassMatch(const EvalContext *ctx, const char *regex)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    if (frame)
    {
        Class *cls = ClassTableMatch(frame->data.bundle.classes, regex);
        if (cls)
        {
            return cls;
        }
    }

    return ClassTableMatch(ctx->global_classes, regex);
}

static bool EvalContextClassPut(EvalContext *ctx, const char *ns, const char *name, bool is_soft, ContextScope scope, const char *tags)
{
    {
        char context_copy[CF_MAXVARSIZE];
        char canonified_context[CF_MAXVARSIZE];

        strcpy(canonified_context, name);
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
            strlcpy(context_copy, canonified_context, CF_MAXVARSIZE);
        }

        if (strlen(context_copy) == 0)
        {
            return false;
        }

        if (IsRegexItemIn(ctx, ctx->heap_abort_current_bundle, context_copy))
        {
            Log(LOG_LEVEL_ERR, "Bundle aborted on defined class '%s'", context_copy);
            SetBundleAborted(ctx);
        }

        if (IsRegexItemIn(ctx, ctx->heap_abort, context_copy))
        {
            FatalError(ctx, "cf-agent aborted on defined class '%s'", context_copy);
        }
    }

    Class *existing_class = EvalContextClassGet(ctx, ns, name);
    if (existing_class && existing_class->scope == scope)
    {
        return false;
    }

    switch (scope)
    {
    case CONTEXT_SCOPE_BUNDLE:
        {
            StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
            if (!frame)
            {
                ProgrammingError("Attempted to add bundle class '%s' while not evaluating a bundle", name);
            }
            ClassTablePut(frame->data.bundle.classes, ns, name, is_soft, scope, tags);
        }
        break;

    case CONTEXT_SCOPE_NAMESPACE:
        ClassTablePut(ctx->global_classes, ns, name, is_soft, scope, tags);
        break;

    case CONTEXT_SCOPE_NONE:
        ProgrammingError("Attempted to add a class without a set scope");
    }

    if (!BundleAborted(ctx))
    {
        for (const Item *ip = ctx->heap_abort_current_bundle; ip != NULL; ip = ip->next)
        {
            const char *class_expr = ip->name;

            if (IsDefinedClass(ctx, class_expr))
            {
                Log(LOG_LEVEL_ERR, "Setting abort for '%s' when setting class '%s'", ip->name, name);
                SetBundleAborted(ctx);
                break;
            }
        }
    }

    return true;
}

static const char *EvalContextCurrentNamespace(const EvalContext *ctx)
{
    for (int i = SeqLength(ctx->stack) - 1; i >= 0; i--)
    {
        StackFrame *frame = SeqAt(ctx->stack, i);
        switch (frame->type)
        {
        case STACK_FRAME_TYPE_BUNDLE:
            return frame->data.bundle.owner->ns;
        case STACK_FRAME_TYPE_BODY:
            return frame->data.body.owner->ns;
        default:
            break;
        }
    }

    return NULL;
}

bool EvalContextClassPutHard(EvalContext *ctx, const char *name, const char *tags)
{
    return EvalContextClassPut(ctx, NULL, name, false, CONTEXT_SCOPE_NAMESPACE, tags);
}

bool EvalContextClassPutSoft(EvalContext *ctx, const char *name, ContextScope scope, const char *tags)
{
    return EvalContextClassPut(ctx, EvalContextCurrentNamespace(ctx), name, true, scope, tags);
}


ClassTableIterator *EvalContextClassTableIteratorNewGlobal(const EvalContext *ctx, const char *ns, bool is_hard, bool is_soft)
{
    return ClassTableIteratorNew(ctx->global_classes, ns, is_hard, is_soft);
}

ClassTableIterator *EvalContextClassTableIteratorNewLocal(const EvalContext *ctx)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    if (!frame)
    {
        return NULL;
    }

    return ClassTableIteratorNew(frame->data.bundle.classes, frame->data.bundle.owner->ns, false, true);
}

const Promise *EvalContextStackCurrentPromise(const EvalContext *ctx)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_PROMISE_ITERATION);
    return frame ? frame->data.promise_iteration.owner : NULL;
}

const Bundle *EvalContextStackCurrentBundle(const EvalContext *ctx)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
    return frame ? frame->data.bundle.owner : NULL;
}

const RingBuffer *EvalContextStackCurrentMessages(const EvalContext *ctx)
{
    StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_PROMISE_ITERATION);
    return frame ? frame->data.promise_iteration.log_messages : NULL;
}

char *EvalContextStackPath(const EvalContext *ctx)
{
    Buffer *path = BufferNew();

    for (size_t i = 0; i < SeqLength(ctx->stack); i++)
    {
        StackFrame *frame = SeqAt(ctx->stack, i);
        switch (frame->type)
        {
        case STACK_FRAME_TYPE_BODY:
            BufferAppendChar(path, '/');
            BufferAppend(path, frame->data.body.owner->name, CF_BUFSIZE);
            break;

        case STACK_FRAME_TYPE_BUNDLE:
            BufferAppendChar(path, '/');
            BufferAppend(path, frame->data.bundle.owner->ns, CF_BUFSIZE);
            BufferAppendChar(path, '/');
            BufferAppend(path, frame->data.bundle.owner->name, CF_BUFSIZE);
            break;

        case STACK_FRAME_TYPE_PROMISE_TYPE:
            BufferAppendChar(path, '/');
            BufferAppend(path, frame->data.promise_type.owner->name, CF_BUFSIZE);

        case STACK_FRAME_TYPE_PROMISE:
            break;

        case STACK_FRAME_TYPE_PROMISE_ITERATION:
            BufferAppendChar(path, '/');
            BufferAppendChar(path, '\'');
            for (const char *ch = frame->data.promise_iteration.owner->promiser; *ch != '\0'; ch++)
            {
                switch (*ch)
                {
                case '*':
                    BufferAppendChar(path, ':');
                    break;

                case '#':
                    BufferAppendChar(path, '.');
                    break;

                default:
                    BufferAppendChar(path, *ch);
                    break;
                }
            }
            BufferAppendChar(path, '\'');
            if (i == SeqLength(ctx->stack) - 1)
            {
                BufferAppendF(path, "[%zd]", frame->data.promise_iteration.index);
            }
            break;
        }
    }

    return BufferClose(path);
}

StringSet *EvalContextStackPromisees(const EvalContext *ctx)
{
    StringSet *promisees = StringSetNew();

    for (size_t i = 0; i < SeqLength(ctx->stack); i++)
    {
        StackFrame *frame = SeqAt(ctx->stack, i);
        if (frame->type != STACK_FRAME_TYPE_PROMISE_ITERATION)
        {
            continue;
        }

        Rval promisee = frame->data.promise_iteration.owner->promisee;

        switch (promisee.type)
        {
        case RVAL_TYPE_SCALAR:
            StringSetAdd(promisees, xstrdup(RvalScalarValue(promisee)));
            break;

        case RVAL_TYPE_LIST:
            {
                for (const Rlist *rp = RvalRlistValue(promisee); rp; rp = rp->next)
                {
                    if (rp->val.type == RVAL_TYPE_SCALAR)
                    {
                        StringSetAdd(promisees, xstrdup(RvalScalarValue(rp->val)));
                    }
                    else
                    {
                        assert(false && "Canary: promisee list contained non-scalar value");
                    }
                }
            }
            break;

        case RVAL_TYPE_NOPROMISEE:
            break;

        default:
            assert(false && "Canary: promisee not scalar or list");
        }
    }

    return promisees;
}

bool EvalContextVariablePutSpecial(EvalContext *ctx, SpecialScope scope, const char *lval, const void *value, DataType type, const char *tags)
{
    if (strchr(lval, '['))
    {
        // dealing with (legacy) array reference in lval, must parse
        VarRef *ref = VarRefParseFromScope(lval, SpecialScopeToString(scope));
        bool ret = EvalContextVariablePut(ctx, ref, value, type, tags);
        VarRefDestroy(ref);
        return ret;
    }
    else
    {
        // plain lval, skip parsing
        const VarRef ref = VarRefConst(NULL, SpecialScopeToString(scope), lval);
        return EvalContextVariablePut(ctx, &ref, value, type, tags);
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

static VariableTable *GetVariableTableForScope(const EvalContext *ctx,
#ifdef NDEBUG
                                               ARG_UNUSED
#endif /* ns is only used in assertions ... */
                                               const char *ns,
                                               const char *scope)
{
    switch (SpecialScopeFromString(scope))
    {
    case SPECIAL_SCOPE_SYS:
    case SPECIAL_SCOPE_MON:
    case SPECIAL_SCOPE_CONST:
        assert(!ns || strcmp("default", ns) == 0);
        return ctx->global_variables;

    case SPECIAL_SCOPE_MATCH:
        assert(!ns || strcmp("default", ns) == 0);
        return ctx->match_variables;

    case SPECIAL_SCOPE_EDIT:
        assert(!ns || strcmp("default", ns) == 0);
        {
            StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
            assert(frame);
            return frame->data.bundle.vars;
        }

    case SPECIAL_SCOPE_BODY:
        assert(!ns || strcmp("default", ns) == 0);
        {
            StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BODY);
            return frame ? frame->data.body.vars : NULL;
        }

    case SPECIAL_SCOPE_THIS:
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
    VariableTable *table = GetVariableTableForScope(ctx, ref->ns, ref->scope);
    return VariableTableRemove(table, ref);
}

static bool IsVariableSelfReferential(const VarRef *ref, const void *value, RvalType rval_type)
{
    switch (rval_type)
    {
    case RVAL_TYPE_SCALAR:
        if (StringContainsVar(value, ref->lval))
        {
            char *ref_str = VarRefToString(ref, true);
            Log(LOG_LEVEL_ERR, "The value of variable '%s' contains a reference to itself, '%s'", ref_str, (char *)value);
            free(ref_str);
            return true;
        }
        break;

    case RVAL_TYPE_LIST:
        for (const Rlist *rp = value; rp != NULL; rp = rp->next)
        {
            if (rp->val.type != RVAL_TYPE_SCALAR)
            {
                continue;
            }

            if (StringContainsVar(RlistScalarValue(rp), ref->lval))
            {
                char *ref_str = VarRefToString(ref, true);
                Log(LOG_LEVEL_ERR, "An item in list variable '%s' contains a reference to itself", ref_str);
                free(ref_str);
                return true;
            }
        }
        break;

    case RVAL_TYPE_FNCALL:
    case RVAL_TYPE_CONTAINER:
    case RVAL_TYPE_NOPROMISEE:
        break;
    }

    return false;
}

static void VarRefStackQualify(const EvalContext *ctx, VarRef *ref)
{
    StackFrame *last_frame = LastStackFrame(ctx, 0);
    assert(last_frame);

    switch (last_frame->type)
    {
    case STACK_FRAME_TYPE_BODY:
        VarRefQualify(ref, NULL, SpecialScopeToString(SPECIAL_SCOPE_BODY));
        break;

    case STACK_FRAME_TYPE_PROMISE_TYPE:
        {
            StackFrame *last_last_frame = LastStackFrame(ctx, 1);
            assert(last_last_frame);
            assert(last_last_frame->type == STACK_FRAME_TYPE_BUNDLE);
            VarRefQualify(ref,
                          last_last_frame->data.bundle.owner->ns,
                          last_last_frame->data.bundle.owner->name);
        }
        break;

    case STACK_FRAME_TYPE_BUNDLE:
        VarRefQualify(ref, last_frame->data.bundle.owner->ns, last_frame->data.bundle.owner->name);
        break;

    case STACK_FRAME_TYPE_PROMISE:
    case STACK_FRAME_TYPE_PROMISE_ITERATION:
        VarRefQualify(ref, NULL, SpecialScopeToString(SPECIAL_SCOPE_THIS));
        break;
    }
}

bool EvalContextVariablePut(EvalContext *ctx,
                            const VarRef *ref, const void *value,
                            DataType type, const char *tags)
{
    assert(type != CF_DATA_TYPE_NONE);
    assert(ref);
    assert(ref->lval);
    assert(value);
    if (!value)
    {
        return false;
    }

    if (strlen(ref->lval) > CF_MAXVARSIZE)
    {
        char *lval_str = VarRefToString(ref, true);
        Log(LOG_LEVEL_ERR, "Variable '%s'' cannot be added because "
            "its length exceeds the maximum length allowed ('%d' characters)",
            lval_str, CF_MAXVARSIZE);
        free(lval_str);
        return false;
    }

    if (strcmp(ref->scope, "body") != 0 &&
        IsVariableSelfReferential(ref, value, DataTypeToRvalType(type)))
    {
        return false;
    }

    Rval rval = (Rval) { (void *)value, DataTypeToRvalType(type) };

    VariableTable *table = GetVariableTableForScope(ctx, ref->ns, ref->scope);
    const Promise *pp = EvalContextStackCurrentPromise(ctx);
    VariableTablePut(table, ref, &rval, type, tags, pp ? pp->org_pp : pp);
    return true;
}

/*
 * Looks up a variable in the the context of the 'current scope'. This basically
 * means that an unqualified reference will be looked up in the context of the top
 * stack frame. Note that when evaluating a promise, this will qualify a reference
 * to the 'this' scope.
 *
 * Ideally, this function should resolve a variable by walking down the stack, but
 * this is pending rework in variable iteration.
 */
static Variable *VariableResolve(const EvalContext *ctx, const VarRef *ref)
{
    assert(ref->lval);

    if (!VarRefIsQualified(ref))
    {
        VarRef *qref = VarRefCopy(ref);
        VarRefStackQualify(ctx, qref);
        Variable *ret = VariableResolve(ctx, qref);
        VarRefDestroy(qref);
        return ret;
    }

    VariableTable *table = GetVariableTableForScope(ctx, ref->ns, ref->scope);
    if (table)
    {
        Variable *var = VariableTableGet(table, ref);
        if (var)
        {
            return var;
        }
        else if (ref->num_indices > 0)
        {
            VarRef *base_ref = VarRefCopyIndexless(ref);
            var = VariableTableGet(table, base_ref);
            VarRefDestroy(base_ref);

            if (var && var->type == CF_DATA_TYPE_CONTAINER)
            {
                return var;
            }
        }
    }

    return NULL;
}

const void *EvalContextVariableGet(const EvalContext *ctx, const VarRef *ref, DataType *type_out)
{
    Variable *var = VariableResolve(ctx, ref);
    if (var)
    {
        if (var->ref->num_indices == 0 && ref->num_indices > 0 && var->type == CF_DATA_TYPE_CONTAINER)
        {
            JsonElement *child = JsonSelect(RvalContainerValue(var->rval), ref->num_indices, ref->indices);
            if (child)
            {
                if (type_out)
                {
                    *type_out = CF_DATA_TYPE_CONTAINER;
                }
                return child;
            }
        }
        else
        {
            if (type_out)
            {
                *type_out = var->type;
            }
            return var->rval.item;
        }
    }
    else if (!VarRefIsQualified(ref))
    {
        /*
         * FALLBACK: Because VariableResolve currently does not walk the stack (rather, it looks at
         * only the top frame), we do an explicit retry here to qualify an unqualified reference to
         * the current bundle.
         *
         * This is overly complicated, and will go away as soon as VariableResolve can walk the stack
         * (which is pending rework in variable iteration).
         */
        const Bundle *bp = EvalContextStackCurrentBundle(ctx);
        if (bp)
        {
            VarRef *qref = VarRefCopy(ref);
            VarRefQualify(qref, bp->ns, bp->name);
            const void *ret = EvalContextVariableGet(ctx, qref, type_out);
            VarRefDestroy(qref);
            return ret;
        }
    }

    if (type_out)
    {
        *type_out = CF_DATA_TYPE_NONE;
    }
    return NULL;
}

const Promise *EvalContextVariablePromiseGet(const EvalContext *ctx, const VarRef *ref)
{
    Variable *var = VariableResolve(ctx, ref);
    return var ? var->promise : NULL;
}

StringSet *EvalContextClassTags(const EvalContext *ctx, const char *ns, const char *name)
{
    Class *cls = EvalContextClassGet(ctx, ns, name);
    if (!cls)
    {
        return NULL;
    }

    if (!cls->tags)
    {
        cls->tags = StringSetNew();
    }

    return cls->tags;
}

StringSet *EvalContextVariableTags(const EvalContext *ctx, const VarRef *ref)
{
    Variable *var = VariableResolve(ctx, ref);
    if (!var)
    {
        return NULL;
    }

    if (!var->tags)
    {
        var->tags = StringSetNew();
    }

    return var->tags;
}

bool EvalContextVariableClearMatch(EvalContext *ctx)
{
    return VariableTableClear(ctx->match_variables, NULL, NULL, NULL);
}

VariableTableIterator *EvalContextVariableTableIteratorNew(const EvalContext *ctx, const char *ns, const char *scope, const char *lval)
{
    VariableTable *table = scope ? GetVariableTableForScope(ctx, ns, scope) : ctx->global_variables;
    return table ? VariableTableIteratorNew(table, ns, scope, lval) : NULL;
}

const void *EvalContextVariableControlCommonGet(const EvalContext *ctx, CommonControl lval)
{
    if (lval >= COMMON_CONTROL_MAX)
    {
        return NULL;
    }

    VarRef *ref = VarRefParseFromScope(CFG_CONTROLBODY[lval].lval, "control_common");
    const void *ret = EvalContextVariableGet(ctx, ref, NULL);
    VarRefDestroy(ref);
    return ret;
}

static ClassRef IDRefQualify(const EvalContext *ctx, const char *id)
{
    // HACK: Because call reference names are equivalent to class names, we abuse ClassRef here
    ClassRef ref = ClassRefParse(id);
    if (!ClassRefIsQualified(ref))
    {
        const char *ns = EvalContextCurrentNamespace(ctx);
        if (ns)
        {
            ClassRefQualify(&ref, ns);
        }
        else
        {
            ClassRefQualify(&ref, NamespaceDefault());
        }
    }

    return ref;
}

const Bundle *EvalContextResolveBundleExpression(const EvalContext *ctx, const Policy *policy,
                                               const char *callee_reference, const char *callee_type)
{
    ClassRef ref = IDRefQualify(ctx, callee_reference);

    const Bundle *bp = NULL;
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        const Bundle *curr_bp = SeqAt(policy->bundles, i);
        if ((strcmp(curr_bp->type, callee_type) != 0) ||
            (strcmp(curr_bp->name, ref.name) != 0) ||
            !StringSafeEqual(curr_bp->ns, ref.ns))
        {
            continue;
        }

        bp = curr_bp;
        break;
    }

    ClassRefDestroy(ref);
    return bp;
}

const Body *EvalContextResolveBodyExpression(const EvalContext *ctx, const Policy *policy,
                                             const char *callee_reference, const char *callee_type)
{
    ClassRef ref = IDRefQualify(ctx, callee_reference);

    const Body *bp = NULL;
    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        const Body *curr_bp = SeqAt(policy->bodies, i);
        if ((strcmp(curr_bp->type, callee_type) != 0) ||
            (strcmp(curr_bp->name, ref.name) != 0) ||
            !StringSafeEqual(curr_bp->ns, ref.ns))
        {
            continue;
        }

        bp = curr_bp;
        break;
    }

    ClassRefDestroy(ref);
    return bp;
}

bool EvalContextPromiseLockCacheContains(const EvalContext *ctx, const char *key)
{
    return StringSetContains(ctx->promise_lock_cache, key);
}

void EvalContextPromiseLockCachePut(EvalContext *ctx, const char *key)
{
    StringSetAdd(ctx->promise_lock_cache, xstrdup(key));
}

bool EvalContextFunctionCacheGet(const EvalContext *ctx, const FnCall *fp, const Rlist *args, Rval *rval_out)
{
    if (!(ctx->eval_options & EVAL_OPTION_CACHE_SYSTEM_FUNCTIONS))
    {
        return false;
    }

    size_t hash = RlistHash(args, FnCallHash(fp, 0, INT_MAX), INT_MAX);
    Rval *rval = RBTreeGet(ctx->function_cache, (void*)hash);
    if (rval)
    {
        if (rval_out)
        {
            *rval_out = *rval;
        }
        return true;
    }
    else
    {
        return false;
    }
}

void EvalContextFunctionCachePut(EvalContext *ctx, const FnCall *fp, const Rlist *args, const Rval *rval)
{
    if (!(ctx->eval_options & EVAL_OPTION_CACHE_SYSTEM_FUNCTIONS))
    {
        return;
    }

    size_t hash = RlistHash(args, FnCallHash(fp, 0, INT_MAX), INT_MAX);
    Rval *rval_copy = xmalloc(sizeof(Rval));
    *rval_copy = RvalCopy(*rval);
    RBTreePut(ctx->function_cache, (void*)hash, rval_copy);
}

/* cfPS and associated machinery */



/*
 * Internal functions temporarily used from logging implementation
 */

static const char *const NO_STATUS_TYPES[] =
    { "vars", "classes", "insert_lines", "delete_lines", "replace_patterns", "field_edits", NULL };
static const char *const NO_LOG_TYPES[] =
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

static void AddAllClasses(EvalContext *ctx, const Rlist *list, unsigned int persistence_ttl,
                          PersistentClassPolicy policy, ContextScope context_scope)
{
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        char *classname = xstrdup(RlistScalarValue(rp));
        if (strcmp(classname, "a_class_global_from_command") == 0 || strcmp(classname, "xxx:a_class_global_from_command") == 0)
        {
            Log(LOG_LEVEL_ERR, "Hit '%s'", classname);
        }

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
            EvalContextHeapPersistentSave(ctx, classname, persistence_ttl, policy, "");
            EvalContextClassPutSoft(ctx, classname, CONTEXT_SCOPE_NAMESPACE, "");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Defining promise result class '%s'", classname);

            switch (context_scope)
            {
            case CONTEXT_SCOPE_BUNDLE:
                EvalContextStackFrameAddSoft(ctx, classname, "");
                break;

            case CONTEXT_SCOPE_NONE:
            case CONTEXT_SCOPE_NAMESPACE:
                EvalContextClassPutSoft(ctx, classname, CONTEXT_SCOPE_NAMESPACE, "");
                break;

            default:
                ProgrammingError("AddAllClasses: Unexpected context_scope %d!",
                                 context_scope);
            }
        }
        free(classname);
    }
}

static void DeleteAllClasses(EvalContext *ctx, const Rlist *list)
{
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (CheckParseContext(RlistScalarValue(rp), CF_IDRANGE) != SYNTAX_TYPE_MATCH_OK)
        {
            return; // TODO: interesting course of action, but why is the check there in the first place?
        }

        if (EvalContextHeapContainsHard(ctx, RlistScalarValue(rp)))
        {
            Log(LOG_LEVEL_ERR, "You cannot cancel a reserved hard class '%s' in post-condition classes",
                  RlistScalarValue(rp));
        }

        const char *string = RlistScalarValue(rp);

        Log(LOG_LEVEL_VERBOSE, "Cancelling class '%s'", string);

        EvalContextHeapPersistentRemove(string);

        {
            ClassRef ref = ClassRefParse(CanonifyName(string));
            EvalContextClassRemove(ctx, ref.ns, ref.name);
            ClassRefDestroy(ref);
        }
        EvalContextStackFrameRemoveSoft(ctx, CanonifyName(string));
    }
}

ENTERPRISE_VOID_FUNC_2ARG_DEFINE_STUB(void, TrackTotalCompliance, ARG_UNUSED PromiseResult, status, ARG_UNUSED const Promise *, pp)
{
}

static void SetPromiseOutcomeClasses(EvalContext *ctx, PromiseResult status, DefineClasses dc)
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
    case PROMISE_RESULT_INTERRUPTED:
        add_classes = dc.failure;
        del_classes = dc.del_notkept;
        break;

    case PROMISE_RESULT_DENIED:
        add_classes = dc.denied;
        del_classes = dc.del_notkept;
        break;

    case PROMISE_RESULT_NOOP:
        add_classes = dc.kept;
        del_classes = dc.del_kept;
        break;

    default:
        ProgrammingError("Unexpected status '%c' has been passed to SetPromiseOutcomeClasses", status);
    }

    AddAllClasses(ctx, add_classes, dc.persist, dc.timer, dc.scope);
    DeleteAllClasses(ctx, del_classes);
}

static void UpdatePromiseComplianceStatus(PromiseResult status, const Promise *pp, const char *reason)
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
        Buffer *buffer = BufferNew();
        ExpandScalar(ctx, NULL, NULL, tc.log_string, buffer);

        if (strcmp(logname, "udp_syslog") == 0)
        {
            RemoteSysLog(tc.log_priority, BufferData(buffer));
        }
        else if (strcmp(logname, "stdout") == 0)
        {
            Log(LOG_LEVEL_INFO, "L: %s", BufferData(buffer));
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

            FILE *fout = safe_fopen(logname, "a");

            if (fout == NULL)
            {
                Log(LOG_LEVEL_ERR, "Unable to open private log '%s'", logname);
                return;
            }

            Log(LOG_LEVEL_VERBOSE, "Logging string '%s' to '%s'", BufferData(buffer), logname);
            fprintf(fout, "%s\n", BufferData(buffer));

            fclose(fout);
        }

        BufferDestroy(buffer);
        tc.log_string = NULL;     /* To avoid repetition */
    }
}

static void DoSummarizeTransaction(EvalContext *ctx, PromiseResult status, const Promise *pp, TransactionContext tc)
{
    if (!IsPromiseValuableForLogging(pp))
    {
        return;
    }

    char *log_name = NULL;

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

    default:
        ProgrammingError("Unexpected promise result status: %d", status);
    }

    SummarizeTransaction(ctx, tc, log_name);
}

void NotifyDependantPromises(EvalContext *ctx, const Promise *pp, PromiseResult result)
{
    switch (result)
    {
    case PROMISE_RESULT_CHANGE:
    case PROMISE_RESULT_NOOP:
        {
            const char *handle = PromiseGetHandle(pp);
            if (handle)
            {
                StringSetAdd(ctx->dependency_handles, xstrdup(handle));
            }
        }
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
        UpdatePromiseCounters(status);
    }

    SetPromiseOutcomeClasses(ctx, status, attr.classes);
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
        const char *version = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_VERSION);
        if (version)
        {
            WriterWriteF(w, " version '%s'", version);
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

    assert(pp);

    if (level >= LOG_LEVEL_VERBOSE)
    {
        LogPromiseContext(ctx, pp);
    }

    va_list ap;
    va_start(ap, fmt);
    char *msg = NULL;
    xvasprintf(&msg, fmt, ap);
    Log(level, "%s", msg);
    va_end(ap);

    /* Now complete the exits status classes and auditing */

    ClassAuditLog(ctx, pp, attr, status);
    UpdatePromiseComplianceStatus(status, pp, msg);
    free(msg);
}

void SetChecksumUpdatesDefault(EvalContext *ctx, bool enabled)
{
    ctx->checksum_updates_default = enabled;
}

bool GetChecksumUpdatesDefault(const EvalContext *ctx)
{
    return ctx->checksum_updates_default;
}

void EvalContextAddIpAddress(EvalContext *ctx, const char *ip_address)
{
    AppendItem(&ctx->ip_addresses, ip_address, "");
}

void EvalContextDeleteIpAddresses(EvalContext *ctx)
{
    DeleteItemList(ctx->ip_addresses);
    ctx->ip_addresses = NULL;
}

Item *EvalContextGetIpAddresses(const EvalContext *ctx)
{
    return ctx->ip_addresses;
}

void EvalContextSetEvalOption(EvalContext *ctx, EvalContextOption option, bool value)
{
    if (value)
    {
        ctx->eval_options |= option;
    }
    else
    {
        ctx->eval_options &= ~option;
    }
}

bool EvalContextGetEvalOption(EvalContext *ctx, EvalContextOption option)
{
    return !!(ctx->eval_options & option);
}

void EvalContextSetLaunchDirectory(EvalContext *ctx, const char *path)
{
    free(ctx->launch_directory);
    ctx->launch_directory = xstrdup(path);
}

void EvalContextSetIgnoreLocks(EvalContext *ctx, bool ignore)
{
    ctx->ignore_locks = ignore;
}

bool EvalContextIsIgnoringLocks(const EvalContext *ctx)
{
    return ctx->ignore_locks;
}
