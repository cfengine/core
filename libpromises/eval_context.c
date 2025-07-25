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

#include <eval_context.h>

#include <files_names.h>
#include <logic_expressions.h>
#include <syntax.h>
#include <item_lib.h>
#include <ornaments.h>
#include <expand.h>                                    /* ExpandPrivateRval */
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
#include <regex.h>
#include <map.h>
#include <conversion.h>                               /* DataTypeIsIterable */
#include <cleanup.h>

/* If we need to put a scoped variable into a special scope, use the string
 * below to replace the original scope separator.
 * (e.g. "config.var" -> "this.config___var" ) */
#define NESTED_SCOPE_SEP "___"

static const char *STACK_FRAME_TYPE_STR[STACK_FRAME_TYPE_MAX] = {
    "BUNDLE",
    "BODY",
    "PROMISE_TYPE",
    "PROMISE",
    "PROMISE_ITERATION"
};


/**
   Define FuncCacheMap.
   Key:   an Rlist (which is linked list of Rvals)
          listing all the argument of the function
   Value: an Rval, the result of the function
 */

static void RvalDestroy2(void *p)
{
    Rval *rv = p;
    RvalDestroy(*rv);
    free(rv);
}

TYPED_MAP_DECLARE(FuncCache, Rlist *, Rval *)

TYPED_MAP_DEFINE(FuncCache, Rlist *, Rval *,
                 RlistHash_untyped,
                 RlistEqual_untyped,
                 RlistDestroy_untyped,
                 RvalDestroy2)

/**
   Define RemoteVarsPromisesMap.
   Key:   bundle name (char *)
   Value: a sequence of promises (const *Promise), only the container
          (sequence) should be deallocated)
 */

static void SeqDestroy_untyped(void *p)
{
    Seq *s = p;
    SeqDestroy(s);
}

TYPED_MAP_DECLARE(RemoteVarPromises, char *, Seq *)

TYPED_MAP_DEFINE(RemoteVarPromises, char *, Seq *,
                 StringHash_untyped,
                 StringEqual_untyped,
                 free,
                 SeqDestroy_untyped)


static Regex *context_expression_whitespace_rx = NULL;

#include <policy.h>

static bool BundleAborted(const EvalContext *ctx);
static void SetBundleAborted(EvalContext *ctx);
static void SetEvalAborted(EvalContext *ctx);

static bool EvalContextStackFrameContainsSoft(const EvalContext *ctx, const char *context);
static bool EvalContextHeapContainsSoft(const EvalContext *ctx, const char *ns, const char *name);
static bool EvalContextHeapContainsHard(const EvalContext *ctx, const char *name);
static bool EvalContextClassPut(EvalContext *ctx, const char *ns, const char *name,
                                bool is_soft, ContextScope scope,
                                const char *tags, const char *comment);
static const char *EvalContextCurrentNamespace(const EvalContext *ctx);
static ClassRef IDRefQualify(const EvalContext *ctx, const char *id);

/**
 * Every agent has only one EvalContext from process start to finish.
 */
struct EvalContext_
{
    const GenericAgentConfig *config;

    int eval_options;
    bool bundle_aborted;
    bool eval_aborted;
    bool checksum_updates_default;
    Item *ip_addresses;
    bool ignore_locks;

    int pass;
    Rlist *args;

    Rlist *restrict_keys;

    Item *heap_abort;
    Item *heap_abort_current_bundle;

    Seq *stack;

    ClassTable *global_classes;
    VariableTable *global_variables;

    VariableTable *match_variables;

    StringSet *promise_lock_cache;
    StringSet *dependency_handles;
    FuncCacheMap *function_cache;

    uid_t uid;
    uid_t gid;
    pid_t pid;
    pid_t ppid;

    // Full path to directory that the binary was launched from.
    char *launch_directory;

    char *entry_point;

    /* new package promise evaluation context */
    PackagePromiseContext *package_promise_context;

    /* select_end_match_eof context*/
    bool select_end_match_eof;

    /* List if all classes set during policy evaluation */
    StringSet *all_classes;

    /* Negated classes (persistent classes that should not be defined). */
    StringSet *negated_classes;

    /* These following two fields are needed for remote variable injection
     * detection (CFE-1915) */
    /* Names of all bundles */
    StringSet *bundle_names;

    /* Promises possibly remotely-injecting variables */
    /* ONLY INITIALIZED WHEN NON-EMPTY, OTHERWISE NULL */
    RemoteVarPromisesMap *remote_var_promises;

    bool dump_reports;
};

void EvalContextSetConfig(EvalContext *ctx, const GenericAgentConfig *config)
{
    assert(ctx != NULL);
    ctx->config = config;
}

const GenericAgentConfig *EvalContextGetConfig(EvalContext *ctx)
{
    assert(ctx != NULL);
    return ctx->config;
}

bool EvalContextGetSelectEndMatchEof(const EvalContext *ctx)
{
    return ctx->select_end_match_eof;
}

void EvalContextSetSelectEndMatchEof(EvalContext *ctx, bool value)
{
    ctx->select_end_match_eof = value;
}

Rlist *EvalContextGetRestrictKeys(const EvalContext *ctx)
{
    assert(ctx != NULL);
    return ctx->restrict_keys;
}

void EvalContextSetRestrictKeys(EvalContext *ctx, const Rlist *restrict_keys)
{
    assert(ctx != NULL);
    ctx->restrict_keys = RlistCopy(restrict_keys);
}

void AddDefaultPackageModuleToContext(const EvalContext *ctx, char *name)
{
    assert(ctx);
    assert(ctx->package_promise_context);

    free(ctx->package_promise_context->control_package_module);
    ctx->package_promise_context->control_package_module =
            SafeStringDuplicate(name);
}

void AddDefaultInventoryToContext(const EvalContext *ctx, Rlist *inventory)
{
    assert(ctx);
    assert(ctx->package_promise_context);

    RlistDestroy(ctx->package_promise_context->control_package_inventory);
    ctx->package_promise_context->control_package_inventory =
            RlistCopy(inventory);
}

static
int PackageManagerSeqCompare(const void *a, const void *b, ARG_UNUSED void *data)
{
    return StringSafeCompare((char*)a, ((PackageModuleBody*)b)->name);
}

void AddPackageModuleToContext(const EvalContext *ctx, PackageModuleBody *pm)
{
    assert(ctx != NULL);
    assert(pm != NULL);

    /* First check if the body is there added from previous pre-evaluation
     * iteration. If it is there update it as we can have new expanded variables. */
    Seq *const bodies = ctx->package_promise_context->package_modules_bodies;
    ssize_t index = SeqIndexOf(bodies, pm->name, PackageManagerSeqCompare);
    if (index != -1)
    {
        SeqRemove(bodies, index);
    }
    SeqAppend(bodies, pm);
}

PackageModuleBody *GetPackageModuleFromContext(const EvalContext *ctx,
        const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }

    for (size_t i = 0;
         i < SeqLength(ctx->package_promise_context->package_modules_bodies);
         i++)
    {
        PackageModuleBody *pm =
                SeqAt(ctx->package_promise_context->package_modules_bodies, i);
        if (strcmp(name, pm->name) == 0)
        {
            return pm;
        }
    }
    return NULL;
}

PackageModuleBody *GetDefaultPackageModuleFromContext(const EvalContext *ctx)
{
    char *def_pm_name = ctx->package_promise_context->control_package_module;
    return GetPackageModuleFromContext(ctx, def_pm_name);
}

Rlist *GetDefaultInventoryFromContext(const EvalContext *ctx)
{
    return ctx->package_promise_context->control_package_inventory;
}

PackagePromiseContext *GetPackagePromiseContext(const EvalContext *ctx)
{
    return ctx->package_promise_context;
}


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
    LogLevel global_log_level = LogGetGlobalLevel();
    LogLevel system_log_level = LogGetGlobalSystemLogLevel();

    LogLevel log_level = (system_log_level != LOG_LEVEL_NOTHING ?
                          system_log_level : global_log_level);

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

const char *EvalContextStackToString(EvalContext *ctx)
{
    StackFrame *last_frame = LastStackFrame(ctx, 0);
    if (last_frame)
    {
        return last_frame->path;
    }
    return "";
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
                return cls->name;
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
        Log(LOG_LEVEL_ERR, "Bundle '%s' aborted on defined class '%s'", frame.owner->name, copy);
        SetBundleAborted(ctx);
    }

    if (IsRegexItemIn(ctx, ctx->heap_abort, copy))
    {
        Log(LOG_LEVEL_NOTICE, "cf-agent aborted on defined class '%s'", copy);
        SetEvalAborted(ctx);
    }

    if (EvalContextStackFrameContainsSoft(ctx, copy))
    {
        return;
    }

    ClassTablePut(frame.classes, frame.owner->ns, context, true,
                  CONTEXT_SCOPE_BUNDLE,
                  NULL_OR_EMPTY(tags) ? NULL : StringSetFromString(tags, ','),
                  NULL);

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
                return EXPRESSION_VALUE_TRUE;
            }
        }
    }
    else
    {
        if (EvalContextHeapContainsHard(ctx, ref.name))
        {
            ClassRefDestroy(ref);
            return EXPRESSION_VALUE_TRUE;
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
    return (ExpressionValue) classy; // ExpressionValue extends bool
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

bool ClassCharIsWhitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

ExpressionValue CheckClassExpression(const EvalContext *ctx, const char *context)
{
    assert(context != NULL);
    ParseResult res;

    if (!context)
    {
        // TODO: Remove this, seems like a hack
        return EXPRESSION_VALUE_TRUE;
    }

    if (context_expression_whitespace_rx == NULL)
    {
        context_expression_whitespace_rx = CompileRegex(CFENGINE_REGEX_WHITESPACE_IN_CONTEXTS);
    }

    if (context_expression_whitespace_rx == NULL)
    {
        Log(LOG_LEVEL_ERR, "The context expression whitespace regular expression could not be compiled, aborting.");
        return EXPRESSION_VALUE_ERROR;
    }

    if (StringMatchFullWithPrecompiledRegex(context_expression_whitespace_rx, context))
    {
        Log(LOG_LEVEL_ERR, "class expressions can't be separated by whitespace without an intervening operator in expression '%s'", context);
        return EXPRESSION_VALUE_ERROR;
    }

    Buffer *condensed = BufferNewFrom(context, strlen(context));
    BufferRewrite(condensed, &ClassCharIsWhitespace, true);
    res = ParseExpression(BufferData(condensed), 0, BufferSize(condensed));
    BufferDestroy(condensed);

    if (!res.result)
    {
        Log(LOG_LEVEL_ERR, "Couldn't find any class matching '%s'", context);
        return EXPRESSION_VALUE_ERROR;
    }
    else
    {
        ExpressionValue r = EvalExpression(res.result,
                                           &EvalTokenAsClass, &EvalVarRef,
                                           (void *)ctx); // controlled cast. None of these should modify EvalContext

        FreeExpression(res.result);
        return r;
    }
}

/**********************************************************************/

static ExpressionValue EvalTokenFromList(const char *token, void *param)
{
    StringSet *set = param;
    return (ExpressionValue) StringSetContains(set, token); // EV extends bool
}

/**********************************************************************/

bool EvalWithTokenFromList(const char *expr, StringSet *token_set)
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
        return r == EXPRESSION_VALUE_TRUE;
    }
}

/**********************************************************************/

/* Process result expression */

bool EvalProcessResult(const char *process_result, StringSet *proc_attr)
{
    assert(process_result != NULL);
    if (StringEqual(process_result, ""))
    {
        /* nothing to evaluate */
        return false;
    }
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
        char *db_path = DBIdToPath(dbid_state);
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
                    Log(LOG_LEVEL_VERBOSE, "Persistent class '%s' is already in a preserved state --  %jd minutes to go",
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
    assert(ctx != NULL);

    time_t now = time(NULL);

    Log(LOG_LEVEL_VERBOSE, "Loading persistent classes");

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
               info_size < (int) sizeof(info) ? info_size : (int) sizeof(info));

        const char *tags = NULL;
        if (info_size > (int) sizeof(PersistentClassInfo))
        {
            /* This is char pointer, it can point to unaligned data. */
            tags = ((PersistentClassInfo *) info_p)->tags;
        }
        else
        {
            tags = "";                                          /* no tags */
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
            if ((ctx->negated_classes != NULL) && StringSetContains(ctx->negated_classes, key))
            {
                Log(LOG_LEVEL_VERBOSE,
                    "Not adding persistent class '%s' due to match in -N/--negate", key);
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Adding persistent class '%s'", key);

                ClassRef ref = ClassRefParse(key);
                EvalContextClassPut(ctx, ref.ns, ref.name, true, CONTEXT_SCOPE_NAMESPACE, tags, NULL);

                StringSet *tag_set = EvalContextClassTags(ctx, ref.ns, ref.name);
                assert(tag_set);

                StringSetAdd(tag_set, xstrdup("source=persistent"));

                ClassRefDestroy(ref);
            }
        }
    }

    DeleteDBCursor(dbcp);
    CloseDB(dbp);
}

void EvalContextSetNegatedClasses(EvalContext *ctx, StringSet *negated_classes)
{
    assert(ctx != NULL);
    ctx->negated_classes = negated_classes;
}


bool BundleAbort(EvalContext *ctx)
{
    assert(ctx != NULL);
    if (ctx->bundle_aborted)
    {
        ctx->bundle_aborted = false;
        return true;
    }

    return false;
}

static bool BundleAborted(const EvalContext* ctx)
{
    assert(ctx != NULL);
    return ctx->bundle_aborted;
}

static void SetBundleAborted(EvalContext *ctx)
{
    assert(ctx != NULL);
    ctx->bundle_aborted = true;
}

static void SetEvalAborted(EvalContext *ctx)
{
    assert(ctx != NULL);
    ctx->eval_aborted = true;
}

bool EvalAborted(const EvalContext *ctx)
{
    assert(ctx != NULL);
    return ctx->eval_aborted;
}

void EvalContextHeapAddAbort(EvalContext *ctx, const char *context, const char *activated_on_context)
{
    assert(ctx != NULL);
    if (!IsItemIn(ctx->heap_abort, context))
    {
        AppendItem(&ctx->heap_abort, context, activated_on_context);
    }

    const char *aborting_context = GetAgentAbortingContext(ctx);

    if (aborting_context)
    {
        Log(LOG_LEVEL_NOTICE, "cf-agent aborted on defined class '%s'", aborting_context);
        SetEvalAborted(ctx);
    }
}

void EvalContextHeapAddAbortCurrentBundle(EvalContext *ctx, const char *context, const char *activated_on_context)
{
    assert(ctx != NULL);
    if (!IsItemIn(ctx->heap_abort_current_bundle, context))
    {
        AppendItem(&ctx->heap_abort_current_bundle, context, activated_on_context);
    }
}

/*****************************************************************************/

bool MissingDependencies(EvalContext *ctx, const Promise *pp)
{
    assert(ctx != NULL);
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
            Log(LOG_LEVEL_VERBOSE, "Skipping promise '%s', as promise dependency '%s' has not yet been kept",
                    pp->promiser, RlistScalarValue(rp));
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

        case STACK_FRAME_TYPE_BUNDLE_SECTION:
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

static
void FreePackageManager(PackageModuleBody *manager)
{
    assert(manager != NULL);

    free(manager->name);
    free(manager->interpreter);
    free(manager->module_path);
    RlistDestroy(manager->options);
    free(manager);
}

static
PackagePromiseContext *PackagePromiseConfigNew()
{
    PackagePromiseContext *package_promise_defaults =
            xmalloc(sizeof(PackagePromiseContext));
    package_promise_defaults->control_package_module = NULL;
    package_promise_defaults->control_package_inventory = NULL;
    package_promise_defaults->package_modules_bodies =
            SeqNew(5, FreePackageManager);

    return package_promise_defaults;
}

static
void FreePackagePromiseContext(PackagePromiseContext *pp_ctx)
{
    SeqDestroy(pp_ctx->package_modules_bodies);
    RlistDestroy(pp_ctx->control_package_inventory);
    free(pp_ctx->control_package_module);
    free(pp_ctx);
}

/* Keeps the last 5 messages of each promise in a ring buffer in the
 * EvalContext, which are written to a JSON file from the Enterprise function
 * EvalContextLogPromiseIterationOutcome() at the end of each promise. */
char *MissionPortalLogHook(LoggingPrivContext *pctx, LogLevel level, const char *message)
{
    const EvalContext *ctx = pctx->param;

    StackFrame *last_frame = LastStackFrame(ctx, 0);
    if (last_frame
        && last_frame->type == STACK_FRAME_TYPE_PROMISE_ITERATION
        && level <= LOG_LEVEL_INFO)
    {
        RingBufferAppend(last_frame->data.promise_iteration.log_messages, xstrdup(message));
    }
    return xstrdup(message);
}

ENTERPRISE_VOID_FUNC_1ARG_DEFINE_STUB(void, EvalContextSetupMissionPortalLogHook,
                                      ARG_UNUSED EvalContext *, ctx)
{
}

EvalContext *EvalContextNew(void)
{
    EvalContext *ctx = xcalloc(1, sizeof(EvalContext));

    ctx->eval_options = EVAL_OPTION_FULL;
    ctx->stack = SeqNew(10, StackFrameDestroy);
    ctx->global_classes = ClassTableNew();
    ctx->global_variables = VariableTableNew();
    ctx->match_variables = VariableTableNew();
    ctx->dependency_handles = StringSetNew();

    ctx->uid = getuid();
    ctx->gid = getgid();
    ctx->pid = getpid();

#ifndef __MINGW32__
    ctx->ppid = getppid();
#endif

    ctx->promise_lock_cache = StringSetNew();
    ctx->function_cache = FuncCacheMapNew();

    EvalContextSetupMissionPortalLogHook(ctx);

    ctx->package_promise_context = PackagePromiseConfigNew();

    ctx->all_classes = NULL;
    ctx->negated_classes = NULL;
    ctx->bundle_names = StringSetNew();
    ctx->remote_var_promises = NULL;

    ctx->select_end_match_eof = false;

    ctx->dump_reports = false;

    return ctx;
}

void EvalContextDestroy(EvalContext *ctx)
{
    if (ctx)
    {
        free(ctx->launch_directory);
        free(ctx->entry_point);

        // Freeing logging context doesn't belong here...
        {
            LoggingPrivContext *pctx = LoggingPrivGetContext();
            free(pctx);
            LoggingPrivSetContext(NULL);
        }
        LoggingFreeCurrentThreadContext();

        EvalContextDeleteIpAddresses(ctx);

        DeleteItemList(ctx->heap_abort);
        DeleteItemList(ctx->heap_abort_current_bundle);

        RlistDestroy(ctx->args);

        SeqDestroy(ctx->stack);

        ClassTableDestroy(ctx->global_classes);
        VariableTableDestroy(ctx->global_variables);
        VariableTableDestroy(ctx->match_variables);

        StringSetDestroy(ctx->dependency_handles);
        StringSetDestroy(ctx->promise_lock_cache);

        FuncCacheMapDestroy(ctx->function_cache);

        FreePackagePromiseContext(ctx->package_promise_context);

        StringSetDestroy(ctx->all_classes);
        StringSetDestroy(ctx->negated_classes);
        StringSetDestroy(ctx->bundle_names);
        if (ctx->remote_var_promises != NULL)
        {
            RemoteVarPromisesMapDestroy(ctx->remote_var_promises);
            ctx->remote_var_promises = NULL;
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
    FuncCacheMapClear(ctx->function_cache);
}

Rlist *EvalContextGetPromiseCallerMethods(EvalContext *ctx) {
    Rlist *callers_promisers = NULL;

    for (size_t i = 0; i < SeqLength(ctx->stack); i++)
    {
        StackFrame *frame = SeqAt(ctx->stack, i);
        switch (frame->type)
        {
        case STACK_FRAME_TYPE_BODY:
            break;

        case STACK_FRAME_TYPE_BUNDLE:
            break;

        case STACK_FRAME_TYPE_PROMISE_ITERATION:
            break;

        case STACK_FRAME_TYPE_PROMISE:
            if (strcmp(frame->data.promise.owner->parent_section->promise_type, "methods") == 0) {
                RlistAppendScalar(&callers_promisers, frame->data.promise.owner->promiser);
            }
            break;

        case STACK_FRAME_TYPE_BUNDLE_SECTION:
            break;

        default:
            ProgrammingError("Unhandled stack frame type");
        }
    }
    return callers_promisers;
}

JsonElement *EvalContextGetPromiseCallers(EvalContext *ctx) {
    JsonElement *callers = JsonArrayCreate(4);
    size_t depth = SeqLength(ctx->stack);

    for (size_t i = 0; i < depth; i++)
    {
        StackFrame *frame = SeqAt(ctx->stack, i);
        JsonElement *f = JsonObjectCreate(10);
        JsonObjectAppendInteger(f, "frame", depth-i);
        JsonObjectAppendInteger(f, "depth", i);

        switch (frame->type)
        {
        case STACK_FRAME_TYPE_BODY:
            JsonObjectAppendString(f, "type", "body");
            JsonObjectAppendObject(f, "body", BodyToJson(frame->data.body.owner));
            break;

        case STACK_FRAME_TYPE_BUNDLE:
            JsonObjectAppendString(f, "type", "bundle");
            JsonObjectAppendObject(f, "bundle", BundleToJson(frame->data.bundle.owner));
            break;

        case STACK_FRAME_TYPE_PROMISE_ITERATION:
            JsonObjectAppendString(f, "type", "iteration");
            JsonObjectAppendInteger(f, "iteration_index", frame->data.promise_iteration.index);

            break;

        case STACK_FRAME_TYPE_PROMISE:
            JsonObjectAppendString(f, "type", "promise");
            JsonObjectAppendString(f, "promise_type", frame->data.promise.owner->parent_section->promise_type);
            JsonObjectAppendString(f, "promiser", frame->data.promise.owner->promiser);
            JsonObjectAppendString(f, "promise_classes", frame->data.promise.owner->classes);
            JsonObjectAppendString(f, "promise_comment",
                                   (frame->data.promise.owner->comment == NULL) ? "" : frame->data.promise.owner->comment);
            break;

        case STACK_FRAME_TYPE_BUNDLE_SECTION:
            JsonObjectAppendString(f, "type", "promise_type");
            JsonObjectAppendString(f, "promise_type", frame->data.bundle_section.owner->promise_type);
            break;

        default:
            ProgrammingError("Unhandled stack frame type");
        }

        JsonArrayAppendObject(callers, f);
    }

    return callers;
}

void EvalContextSetBundleArgs(EvalContext *ctx, const Rlist *args)
{
    if (ctx->args)
    {
        RlistDestroy(ctx->args);
    }

    ctx->args = RlistCopy(args);
}

Rlist *EvalContextGetBundleArgs(EvalContext *ctx)
{
    return (Rlist *) ctx->args;
}

void EvalContextSetPass(EvalContext *ctx, int pass)
{
    ctx->pass = pass;
}

int EvalContextGetPass(EvalContext *ctx)
{
    return ctx->pass;
}

static StackFrame *StackFrameNew(StackFrameType type, bool inherit_previous)
{
    StackFrame *frame = xmalloc(sizeof(StackFrame));

    frame->type = type;
    frame->inherits_previous = inherit_previous;
    frame->path = NULL;
    frame->override_immutable = false;

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

static StackFrame *StackFrameNewBundleSection(const BundleSection *owner)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_BUNDLE_SECTION, true);

    frame->data.bundle_section.owner = owner;

    return frame;
}

static StackFrame *StackFrameNewPromise(const Promise *owner)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_PROMISE, true);

    frame->data.promise.owner = owner;

    return frame;
}

static StackFrame *StackFrameNewPromiseIteration(Promise *owner, const PromiseIterator *iter_ctx)
{
    StackFrame *frame = StackFrameNew(STACK_FRAME_TYPE_PROMISE_ITERATION, true);

    frame->data.promise_iteration.owner = owner;
    frame->data.promise_iteration.iter_ctx = iter_ctx;
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
            LogLevel global_log_level = LogGetGlobalLevel();
            LogLevel system_log_level = LogGetGlobalSystemLogLevel();
            LoggingPrivSetLevels(system_log_level != LOG_LEVEL_NOTHING ? system_log_level : global_log_level,
                                 global_log_level);
        }
    }

    SeqAppend(ctx->stack, frame);

    assert(!frame->path);
    frame->path = EvalContextStackPath(ctx);

    LogDebug(LOG_MOD_EVALCTX, "PUSHED FRAME (type %s)",
             STACK_FRAME_TYPE_STR[frame->type]);
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
            Rval var_rval = VariableGetRval(var, true);
            Rval retval = ExpandPrivateRval(ctx, owner->ns, owner->name, var_rval.item, var_rval.type);
            VariableSetRval(var, retval);
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
        assert(last_frame->type == STACK_FRAME_TYPE_BUNDLE_SECTION);
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

void EvalContextStackPushBundleSectionFrame(EvalContext *ctx, const BundleSection *owner)
{
    assert(LastStackFrame(ctx, 0) && LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_BUNDLE);

    StackFrame *frame = StackFrameNewBundleSection(owner);
    EvalContextStackPushFrame(ctx, frame);
}

void EvalContextStackPushPromiseFrame(EvalContext *ctx, const Promise *owner)
{
    assert(LastStackFrame(ctx, 0));
    assert(LastStackFrame(ctx, 0)->type == STACK_FRAME_TYPE_BUNDLE_SECTION);

    EvalContextVariableClearMatch(ctx);

    StackFrame *frame = StackFrameNewPromise(owner);

    EvalContextStackPushFrame(ctx, frame);

    // create an empty table
    frame->data.promise.vars = VariableTableNew();

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
        char number[PRINTSIZE(uintmax_t)];
        xsnprintf(number, CF_SMALLBUF, "%ju", (uintmax_t) owner->offset.line);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promise_linenumber", number, CF_DATA_TYPE_STRING, "source=promise");
    }

    char v[PRINTSIZE(int)];
    xsnprintf(v, sizeof(v), "%d", (int) ctx->uid);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_uid", v, CF_DATA_TYPE_INT, "source=agent");
    xsnprintf(v, sizeof(v), "%d", (int) ctx->gid);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_gid", v, CF_DATA_TYPE_INT, "source=agent");
    xsnprintf(v, sizeof(v), "%d", (int) ctx->pid);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_pid", v, CF_DATA_TYPE_INT, "source=agent");
    xsnprintf(v, sizeof(v), "%d", (int) ctx->ppid);
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_ppid", v, CF_DATA_TYPE_INT, "source=agent");

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "bundle", PromiseGetBundle(owner)->name, CF_DATA_TYPE_STRING, "source=promise");
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "namespace", PromiseGetNamespace(owner), CF_DATA_TYPE_STRING, "source=promise");

    // Recompute `with`
    for (size_t i = 0; i < SeqLength(owner->conlist); i++)
    {
        Constraint *cp = SeqAt(owner->conlist, i);
        if (StringEqual(cp->lval, "with"))
        {
            Rval final = EvaluateFinalRval(ctx, PromiseGetPolicy(owner), NULL,
                                           "this", cp->rval, false, owner);
            if (final.type == RVAL_TYPE_SCALAR &&
                ((EvalContextGetPass(ctx) == CF_DONEPASSES - 1) || !IsCf3VarString(RvalScalarValue(final))))
            {
                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "with", RvalScalarValue(final), CF_DATA_TYPE_STRING, "source=promise_iteration/with");
            }
            RvalDestroy(final);
        }
    }
}

Promise *EvalContextStackPushPromiseIterationFrame(EvalContext *ctx, const PromiseIterator *iter_ctx)
{
    const StackFrame *last_frame = LastStackFrame(ctx, 0);

    assert(last_frame       != NULL);
    assert(last_frame->type == STACK_FRAME_TYPE_PROMISE);

    /* Evaluate all constraints by calling functions etc. */
    bool excluded;
    Promise *pexp = ExpandDeRefPromise(ctx, last_frame->data.promise.owner,
                                       &excluded);
    if (excluded || !pexp)
    {
        PromiseDestroy(pexp);
        return NULL;
    }

    EvalContextStackPushFrame(ctx, StackFrameNewPromiseIteration(pexp, iter_ctx));
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
        {
            LogLevel global_log_level = LogGetGlobalLevel();
            LogLevel system_log_level = LogGetGlobalSystemLogLevel();
            LoggingPrivSetLevels(system_log_level != LOG_LEVEL_NOTHING ? system_log_level : global_log_level,
                                 global_log_level);
        }
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

    LogDebug(LOG_MOD_EVALCTX, "POPPED FRAME (type %s)",
             STACK_FRAME_TYPE_STR[last_frame_type]);
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

static bool EvalContextClassPutTagsSet(EvalContext *ctx, const char *ns, const char *name, bool is_soft,
                                       ContextScope scope, StringSet *tags, const char *comment)
{
    {
        char context_copy[2 * CF_MAXVARSIZE];
        char canonified_context[CF_MAXVARSIZE];


        /* Redmine #7013
         * Fix for classes names longer than CF_MAXVARSIZE. */
        if (strlen(name) >= sizeof(canonified_context))
        {
            Log(LOG_LEVEL_WARNING, "Skipping adding class [%s] as its name "
                "is equal or longer than %zu", name, sizeof(canonified_context));
            return false;
        }

        strlcpy(canonified_context, name, sizeof(canonified_context));

        if (Chop(canonified_context, CF_EXPANDSIZE) == -1)
        {
            Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
        }
        CanonifyNameInPlace(canonified_context);

        if (ns && strcmp(ns, "default") != 0)
        {
            snprintf(context_copy, sizeof(context_copy), "%s:%s", ns, canonified_context);
        }
        else
        {
            strlcpy(context_copy, canonified_context, sizeof(context_copy));
        }

        if (strlen(context_copy) == 0)
        {
            return false;
        }

        if (IsRegexItemIn(ctx, ctx->heap_abort_current_bundle, context_copy))
        {
            const Bundle *bundle = EvalContextStackCurrentBundle(ctx);
            if (bundle != NULL)
            {
                Log(LOG_LEVEL_ERR, "Bundle '%s' aborted on defined class '%s'", bundle->name, context_copy);
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Bundle (unknown) aborted on defined class '%s'", context_copy);
            }
            SetBundleAborted(ctx);
        }

        if (IsRegexItemIn(ctx, ctx->heap_abort, context_copy))
        {
            Log(LOG_LEVEL_NOTICE, "cf-agent aborted on defined class '%s'", context_copy);
            SetEvalAborted(ctx);
        }
    }

    Class *existing_class = EvalContextClassGet(ctx, ns, name);
    if (existing_class && existing_class->scope == scope)
    {
        return false;
    }

    Nova_ClassHistoryAddContextName(ctx->all_classes, name);

    switch (scope)
    {
    case CONTEXT_SCOPE_BUNDLE:
        {
            StackFrame *frame = LastStackFrameByType(ctx, STACK_FRAME_TYPE_BUNDLE);
            if (!frame)
            {
                ProgrammingError("Attempted to add bundle class '%s' while not evaluating a bundle", name);
            }
            ClassTablePut(frame->data.bundle.classes, ns, name, is_soft, scope, tags, comment);
        }
        break;

    case CONTEXT_SCOPE_NAMESPACE:
        ClassTablePut(ctx->global_classes, ns, name, is_soft, scope, tags, comment);
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

static bool EvalContextClassPut(EvalContext *ctx, const char *ns, const char *name, bool is_soft,
                                ContextScope scope, const char *tags, const char *comment)
{
    StringSet *tags_set = (NULL_OR_EMPTY(tags) ? NULL : StringSetFromString(tags, ','));
    bool ret = EvalContextClassPutTagsSet(ctx, ns, name, is_soft, scope, tags_set, comment);
    if (!ret)
    {
        StringSetDestroy(tags_set);
    }
    return ret;
}

static const char *EvalContextCurrentNamespace(const EvalContext *ctx)
{
    size_t i = SeqLength(ctx->stack);
    while (i > 0)
    {
        i--;
        StackFrame *frame = SeqAt(ctx->stack, i);
        switch (frame->type)
        {
        case STACK_FRAME_TYPE_BUNDLE:
            return frame->data.bundle.owner->ns;
        case STACK_FRAME_TYPE_BODY:
            return frame->data.body.owner->ns;
        default:
            break; /* out of the switch but not the loop ! */
        }
    }

    return NULL;
}

bool EvalContextClassPutHard(EvalContext *ctx, const char *name, const char *tags)
{
    return EvalContextClassPut(ctx, NULL, name, false, CONTEXT_SCOPE_NAMESPACE, tags, NULL);
}

bool EvalContextClassPutSoft(EvalContext *ctx, const char *name, ContextScope scope, const char *tags)
{
    StringSet *tags_set = (NULL_OR_EMPTY(tags) ? NULL : StringSetFromString(tags, ','));
    bool ret = EvalContextClassPutSoftTagsSet(ctx, name, scope, tags_set);
    if (!ret)
    {
        StringSetDestroy(tags_set);
    }
    return ret;
}

bool EvalContextClassPutSoftTagsSet(EvalContext *ctx, const char *name, ContextScope scope, StringSet *tags)
{
    return EvalContextClassPutSoftTagsSetWithComment(ctx, name, scope, tags, NULL);
}

bool EvalContextClassPutSoftTagsSetWithComment(EvalContext *ctx, const char *name, ContextScope scope,
                                               StringSet *tags, const char *comment)
{
    bool ret;
    char *ns = NULL;
    char *delim = strchr(name, ':');

    if (delim)
    {
        ns = xstrndup(name, delim - name);
    }

    ret = EvalContextClassPutTagsSet(ctx, ns ? ns : EvalContextCurrentNamespace(ctx),
                                     ns ? delim + 1 : name, true, scope, tags, comment);
    free(ns);
    return ret;
}

bool EvalContextClassPutSoftNS(EvalContext *ctx, const char *ns, const char *name,
                               ContextScope scope, const char *tags)
{
    return EvalContextClassPut(ctx, ns, name, true, scope, tags, NULL);
}

/**
 * Takes over #tags in case of success.
 */
bool EvalContextClassPutSoftNSTagsSet(EvalContext *ctx, const char *ns, const char *name,
                                      ContextScope scope, StringSet *tags)
{
    return EvalContextClassPutSoftNSTagsSetWithComment(ctx, ns, name, scope, tags, NULL);
}

bool EvalContextClassPutSoftNSTagsSetWithComment(EvalContext *ctx, const char *ns, const char *name,
                                                 ContextScope scope, StringSet *tags, const char *comment)
{
    return EvalContextClassPutTagsSet(ctx, ns, name, true, scope, tags, comment);
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



/**
 * @brief Concatenate string #str to #buf, replacing mangling
 *        characters '*' and '#' with their visible counterparts.
 */
static void BufferAppendPromiseStr(Buffer *buf, const char *str)
{
    for (const char *ch = str; *ch != '\0'; ch++)
    {
        switch (*ch)
        {
        case CF_MANGLED_NS:
            BufferAppendChar(buf, ':');
            break;

        case CF_MANGLED_SCOPE:
            BufferAppendChar(buf, '.');
            break;

        default:
            BufferAppendChar(buf, *ch);
            break;
        }
    }
}

/**
 * @brief Like @c BufferAppendPromiseStr, but if @c str contains newlines
 *   and is longer than 2*N+3, then only copy an abbreviated version
 *   consisting of the first and last N characters, separated by @c `...`
 *
 * @param buffer Buffer to be used.
 * @param promiser Constant string to append
 * @param N      Max. length of initial/final segment of @c promiser to keep
 * @note 2*N+3 is the maximum length of the appended string (excl. terminating NULL)
 *
 */
static void BufferAppendAbbreviatedStr(Buffer *buf,
                                       const char *promiser, const int N)
{
    /* check if `promiser` contains a new line (may happen for "insert_lines") */
    const char *const nl = strchr(promiser, '\n');
    if (NULL == nl)
    {
        BufferAppendPromiseStr(buf, promiser);
    }
    else
    {
        /* `promiser` contains a newline: abbreviate it by taking the first and last few characters */
        static const char sep[] = "...";
        char abbr[sizeof(sep) + 2 * N];
        const int head = (nl > promiser + N) ? N : (nl - promiser);
        const char * last_line = strrchr(promiser, '\n') + 1;
        assert(last_line); /* not NULL, we know we have at least one '\n' */
        const int tail = strlen(last_line);
        if (tail > N)
        {
            last_line += tail - N;
        }
        memcpy(abbr, promiser, head);
        strcpy(abbr + head, sep);
        strcat(abbr, last_line);
        BufferAppendPromiseStr(buf, abbr);
    }
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

        case STACK_FRAME_TYPE_BUNDLE_SECTION:
            BufferAppendChar(path, '/');
            BufferAppend(path, frame->data.bundle_section.owner->promise_type, CF_BUFSIZE);

        case STACK_FRAME_TYPE_PROMISE:
            break;

        case STACK_FRAME_TYPE_PROMISE_ITERATION:
            BufferAppendChar(path, '/');
            BufferAppendChar(path, '\'');
            BufferAppendAbbreviatedStr(path, frame->data.promise_iteration.owner->promiser, CF_MAXFRAGMENT);
            BufferAppendChar(path, '\'');
            if (i == SeqLength(ctx->stack) - 1  &&
                /* For some reason verify_packages.c is adding NULL iteration
                 * frames all over the place; TODO fix. */
                frame->data.promise_iteration.iter_ctx != NULL)
            {
                BufferAppendF(path, "[%zu]",
                              PromiseIteratorIndex(frame->data.promise_iteration.iter_ctx));
            }
            break;

            default:
                ProgrammingError("Unhandled stack frame type");
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

/**
 * We cannot have double-scoped variables (e.g. "this.config.var1"), so if we
 * want to put a scoped variable into a special scope, we need to mangle the
 * name like this:
 *   "config.var1" -> "config___var1"
 */
static inline char *MangleScopedVarNameIntoSpecialScopeName(const char *scope, const char *var_name)
{
    const size_t var_name_len = strlen(var_name);

    /* Replace '.' with NESTED_SCOPE_SEP */
    char *new_var_name = xmalloc(var_name_len + sizeof(NESTED_SCOPE_SEP));
    memcpy(new_var_name, var_name, var_name_len + 1 /* including '\0' */);

    /* Make sure we only replace the "scope." string, not all dots. */
    char *scope_with_dot = StringConcatenate(2, scope, ".");
    char *scope_with_underscores = StringConcatenate(2, scope, NESTED_SCOPE_SEP);

    /* Only replace the first "scope." occurrence (there might be "scope."
     * inside square brackets). */
    NDEBUG_UNUSED ssize_t ret = StringReplaceN(new_var_name, var_name_len + sizeof(NESTED_SCOPE_SEP),
                                               scope_with_dot, scope_with_underscores, 1);
    assert(ret == (var_name_len + sizeof(NESTED_SCOPE_SEP) - 2));

    free(scope_with_dot);
    free(scope_with_underscores);

    return new_var_name;
}

/*
 * Copies value, so you need to free your own copy afterwards.
 */
bool EvalContextVariablePutSpecial(EvalContext *ctx, SpecialScope scope, const char *lval, const void *value, DataType type, const char *tags)
{
    StringSet *tags_set = (NULL_OR_EMPTY(tags) ? NULL : StringSetFromString(tags, ','));
    bool ret = EvalContextVariablePutSpecialTagsSet(ctx, scope, lval, value, type, tags_set);
    if (!ret)
    {
        StringSetDestroy(tags_set);
    }
    return ret;
}

/**
 * Copies value, so you need to free your own copy afterwards, EXCEPT FOR THE
 * 'tags' SET which is taken over as-is IF THE VARIABLE IS SUCCESSFULLY ADDED.
 */
bool EvalContextVariablePutSpecialTagsSet(EvalContext *ctx, SpecialScope scope,
                                          const char *lval, const void *value,
                                          DataType type, StringSet *tags)
{
    return EvalContextVariablePutSpecialTagsSetWithComment(ctx, scope, lval, value, type, tags, NULL);
}

bool EvalContextVariablePutSpecialTagsSetWithComment(EvalContext *ctx, SpecialScope scope,
                                                     const char *lval, const void *value,
                                                     DataType type, StringSet *tags,
                                                     const char *comment)
{
    char *new_lval = NULL;
    if (strchr(lval, '.') != NULL)
    {
        VarRef *ref = VarRefParse(lval);
        if (ref->scope != NULL)
        {
            new_lval = MangleScopedVarNameIntoSpecialScopeName(ref->scope, lval);
        }
        VarRefDestroy(ref);
    }
    if (strchr(lval, '['))
    {
        // dealing with (legacy) array reference in lval, must parse
        VarRef *ref = VarRefParseFromScope(new_lval ? new_lval : lval, SpecialScopeToString(scope));
        bool ret = EvalContextVariablePutTagsSetWithComment(ctx, ref, value, type, tags, comment);
        free(new_lval);
        VarRefDestroy(ref);
        return ret;
    }
    else
    {
        // plain lval, skip parsing
        const VarRef ref = VarRefConst(NULL, SpecialScopeToString(scope), new_lval ? new_lval : lval);
        bool ret = EvalContextVariablePutTagsSetWithComment(ctx, &ref, value, type, tags, comment);
        free(new_lval);
        return ret;
    }
}

const void *EvalContextVariableGetSpecial(
    const EvalContext *const ctx,
    const SpecialScope scope,
    const char *const varname,
    DataType *const type_out)
{
    VarRef *const ref = VarRefParseFromScope(
        varname, SpecialScopeToString(scope));
    const void *const result = EvalContextVariableGet(ctx, ref, type_out);
    VarRefDestroy(ref);

    return result;
}

/**
 * @note Only use this when you know the variable is a string
 * @see EvalContextVariableGetSpecial()
 */
const char *EvalContextVariableGetSpecialString(
    const EvalContext *const ctx,
    const SpecialScope scope,
    const char *const varname)
{
    DataType type_out;
    const void *const result = EvalContextVariableGetSpecial(
        ctx, scope, varname, &type_out);
    assert(type_out == CF_DATA_TYPE_STRING); // Programming error if not string
    return (type_out == CF_DATA_TYPE_STRING) ? result : NULL;
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
                                               NDEBUG_UNUSED const char *ns, /* only used in assertions ... */
                                               const char *scope)
{
    assert(ctx != NULL);

    switch (SpecialScopeFromString(scope))
    {
    case SPECIAL_SCOPE_DEF:
        /* 'def.' is not as special as the other scopes below. (CFE-3668) */
        return ctx->global_variables;

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

    // "this" variables can be in local or global variable table (when this is used for non-special
    // varables), so return local as VariableResolve will try global table anyway.
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

    case STACK_FRAME_TYPE_BUNDLE_SECTION:
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
        VarRefQualify(ref,
                      last_frame->data.bundle.owner->ns,
                      last_frame->data.bundle.owner->name);
        break;

    case STACK_FRAME_TYPE_PROMISE:
    case STACK_FRAME_TYPE_PROMISE_ITERATION:
        // Allow special "this" variables to work when used without "this"
        VarRefQualify(ref, NULL, SpecialScopeToString(SPECIAL_SCOPE_THIS));
        break;

    default:
        ProgrammingError("Unhandled stack frame type");
    }
}

/*
 * Copies value, so you need to free your own copy afterwards.
 */
bool EvalContextVariablePut(EvalContext *ctx,
                            const VarRef *ref, const void *value,
                            DataType type, const char *tags)
{
    StringSet *tags_set = (NULL_OR_EMPTY(tags) ? NULL : StringSetFromString(tags, ','));
    bool ret = EvalContextVariablePutTagsSet(ctx, ref, value, type, tags_set);
    if (!ret)
    {
        StringSetDestroy(tags_set);
    }
    return ret;
}

/**
 * Copies value, so you need to free your own copy afterwards, EXCEPT FOR THE
 * 'tags' SET which is taken over as-is IF THE VARIABLE IS SUCCESSFULLY ADDED.
 */
bool EvalContextVariablePutTagsSet(EvalContext *ctx,
                                   const VarRef *ref, const void *value,
                                   DataType type, StringSet *tags)
{
    return EvalContextVariablePutTagsSetWithComment(ctx, ref, value, type, tags, NULL);
}

bool EvalContextVariablePutTagsSetWithComment(EvalContext *ctx,
                                              const VarRef *ref, const void *value,
                                              DataType type, StringSet *tags,
                                              const char *comment)
{
    assert(type != CF_DATA_TYPE_NONE);
    assert(ref);
    assert(ref->lval);

    /* The only possible way to get a NULL value is if it's an empty linked
     * list (Rlist usually). */
    assert(value != NULL || DataTypeIsIterable(type));

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
    VariableTablePut(table, ref, &rval, type, tags, SafeStringDuplicate(comment), pp ? pp->org_pp : pp);
    return true;
}

/**
 * Change ref for e.g. 'config.var1' to 'this.config___var1'
 *
 * @see MangleScopedVarNameIntoSpecialScopeName()
 */
static inline VarRef *MangledThisScopedRef(const VarRef *ref)
{
    VarRef *mangled_this_ref = VarRefCopy(ref);
    char *scope_underscores_lval = StringConcatenate(3, mangled_this_ref->scope,
                                                     NESTED_SCOPE_SEP,
                                                     mangled_this_ref->lval);
    free(mangled_this_ref->lval);
    mangled_this_ref->lval = scope_underscores_lval;
    free(mangled_this_ref->scope);
    mangled_this_ref->scope = xstrdup("this");

    return mangled_this_ref;
}

static Variable *VariableResolve2(const EvalContext *ctx, const VarRef *ref)
{
    assert(ref != NULL);

    // Get the variable table associated to the scope
    VariableTable *table = GetVariableTableForScope(ctx, ref->ns, ref->scope);

    Variable *var;
    if (table)
    {
        /* NOTE: The 'this.' scope should ignore namespaces because it contains
         *       iteration variables that don't have the namespace in their ref
         *       string and so VariableTableGet() would fail to find them with
         *       the namespace. And similar logic applies to other special
         *       scopes except for 'def.' which is actually not so special. */
        if ((SpecialScopeFromString(ref->scope) != SPECIAL_SCOPE_NONE) &&
            (SpecialScopeFromString(ref->scope) != SPECIAL_SCOPE_DEF) &&
            (ref->ns != NULL))
        {
            VarRef *ref2 = VarRefCopy(ref);
            free(ref2->ns);
            ref2->ns = NULL;
            var = VariableTableGet(table, ref2);
            VarRefDestroy(ref2);
        }
        else
        {
            var = VariableTableGet(table, ref);
        }
        if (var)
        {
            return var;
        }
        else if (ref->num_indices > 0)
        {
            /* Iteration over slists creates special variables in the 'this.'
             * scope with the slist variable replaced by the individual
             * values. However, if a scoped variable is part of the variable
             * reference, e.g. 'config.data[$(list)]', the special iteration
             * variables use mangled names to avoid having two scopes
             * (e.g. 'this.config___data[list_item1]' instead of
             * 'this.config.data[list_item1]').
             *
             * If the ref we are looking for has indices and it has a scope, it
             * might be the case described above. Let's give it a try before
             * falling back to the indexless container lookup described below
             * (which will not have the list-iteration variables expanded). */
            if (ref->scope != NULL)
            {
                VariableTable *this_table = GetVariableTableForScope(ctx, ref->ns,
                                                                     SpecialScopeToString(SPECIAL_SCOPE_THIS));
                if (this_table != NULL)
                {
                    VarRef *mangled_this_ref = MangledThisScopedRef(ref);
                    var = VariableTableGet(this_table, mangled_this_ref);
                    VarRefDestroy(mangled_this_ref);
                    if (var != NULL)
                    {
                        return var;
                    }
                }
            }

            /* If the lookup with indices (the [idx1][idx2]... part of the
             * variable reference) fails, there might still be a container
             * variable where the indices actually refer to child objects inside
             * the container structure. */
            VarRef *base_ref = VarRefCopyIndexless(ref);
            var = VariableTableGet(table, base_ref);
            VarRefDestroy(base_ref);

            if (var && (VariableGetType(var) == CF_DATA_TYPE_CONTAINER))
            {
                return var;
            }
        }
    }

    return NULL;
}

/*
 * Looks up a variable in the the context of the 'current scope'. This
 * basically means that an unqualified reference will be looked up in the
 * context of the top stack frame.
 *
 * Note that when evaluating a promise, this
 * will qualify a reference to 'this' scope and when evaluating a body, it
 * will qualify a reference to 'body' scope.
 */
static Variable *VariableResolve(const EvalContext *ctx, const VarRef *ref)
{
    assert(ref->lval);

    /* We will make a first lookup that works in almost all cases: will look
     * for local or global variables, depending of the current scope. */

    Variable *ret_var = VariableResolve2(ctx, ref);
    if (ret_var != NULL)
    {
        return ret_var;
    }

    /* Try to qualify non-scoped vars to the scope:
       "this" for promises, "body" for bodies, current bundle for bundles. */
    VarRef *scoped_ref = NULL;
    if (!VarRefIsQualified(ref))
    {
        scoped_ref = VarRefCopy(ref);
        VarRefStackQualify(ctx, scoped_ref);
        ret_var = VariableResolve2(ctx, scoped_ref);
        if (ret_var != NULL)
        {
            VarRefDestroy(scoped_ref);
            return ret_var;
        }
        ref = scoped_ref;              /* continue with the scoped variable */
    }

    const Bundle *last_bundle = EvalContextStackCurrentBundle(ctx);

    /* If we are in a promise or a body, the variable might be coming from the
     * last bundle. So try a last lookup with "this" or "body" special scopes
     * replaced with the last bundle. */

    if ((SpecialScopeFromString(ref->scope) == SPECIAL_SCOPE_THIS  ||
         SpecialScopeFromString(ref->scope) == SPECIAL_SCOPE_BODY)
        &&  last_bundle != NULL)
    {
        VarRef *ref2 = VarRefCopy(ref);
        VarRefQualify(ref2, last_bundle->ns, last_bundle->name);
        ret_var = VariableResolve2(ctx, ref2);

        VarRefDestroy(scoped_ref);
        VarRefDestroy(ref2);
        return ret_var;
    }
    VarRefDestroy(scoped_ref);

    return NULL;
}

/**
 *
 * @NOTE NULL is a valid return value if #type_out is of list type and the
 *       list is empty. To check if the variable didn't resolve, check if
 *       #type_out was set to CF_DATA_TYPE_NONE.
 */
const void *EvalContextVariableGet(const EvalContext *ctx, const VarRef *ref, DataType *type_out)
{
    Variable *var = VariableResolve(ctx, ref);
    if (var)
    {
        const VarRef *var_ref = VariableGetRef(var);
        DataType var_type = VariableGetType(var);
        Rval var_rval = VariableGetRval(var, true);

        if (var_ref->num_indices == 0    &&
                 ref->num_indices > 0     &&
            var_type == CF_DATA_TYPE_CONTAINER)
        {
            JsonElement *child = JsonSelect(RvalContainerValue(var_rval),
                                            ref->num_indices, ref->indices);
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
                *type_out = var_type;
            }
            return var_rval.item;
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
    return var ? VariableGetPromise(var) : NULL;
}

StringSet *EvalContextClassTags(const EvalContext *ctx, const char *ns, const char *name)
{
    Class *cls = EvalContextClassGet(ctx, ns, name);
    if (!cls)
    {
        return NULL;
    }

    assert(cls->tags != NULL);
    return cls->tags;
}

StringSet *EvalContextVariableTags(const EvalContext *ctx, const VarRef *ref)
{
    Variable *var = VariableResolve(ctx, ref);
    if (!var)
    {
        return NULL;
    }

    StringSet *var_tags = VariableGetTags(var);
    return var_tags;
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


VariableTableIterator *EvalContextVariableTableFromRefIteratorNew(const EvalContext *ctx, const VarRef *ref)
{
    assert(ref);
    VariableTable *table = ref->scope ? GetVariableTableForScope(ctx, ref->ns, ref->scope) : ctx->global_variables;
    return table ? VariableTableIteratorNewFromVarRef(table, ref) : NULL;
}

const void *EvalContextVariableControlCommonGet(const EvalContext *ctx, CommonControl lval)
{
    assert(lval >= 0 && lval < COMMON_CONTROL_MAX);

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
            !StringEqual(curr_bp->ns, ref.ns))
        {
            continue;
        }

        bp = curr_bp;
        break;
    }

    ClassRefDestroy(ref);
    return bp;
}

const Body *EvalContextFindFirstMatchingBody(const Policy *policy, const char *type,
                                             const char *namespace, const char *name)
{
    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        const Body *curr_bp = SeqAt(policy->bodies, i);
        if ((strcmp(curr_bp->type, type) == 0) &&
            (strcmp(curr_bp->name, name) == 0) &&
            StringEqual(curr_bp->ns, namespace))
        {
            return curr_bp;
        }
    }

    return NULL;
}

void EvalContextAppendBodyParentsAndArgs(const EvalContext *ctx, const Policy *policy,
                                         Seq* chain, const Body *bp, const char *callee_type,
                                         int depth)
{
    if (depth > 30) // sanity check
    {
        Log(LOG_LEVEL_ERR, "EvalContextAppendBodyParentsAndArgs: body inheritance chain depth %d in body %s is too much, aborting", depth, bp->name);
        DoCleanupAndExit(EXIT_FAILURE);
    }

    for (size_t k = 0; bp->conlist && k < SeqLength(bp->conlist); k++)
    {
        Constraint *scp = SeqAt(bp->conlist, k);
        if (strcmp("inherit_from", scp->lval) == 0)
        {
            char* call = NULL;

            if (RVAL_TYPE_SCALAR == scp->rval.type)
            {
                call = RvalScalarValue(scp->rval);
            }
            else if (RVAL_TYPE_FNCALL == scp->rval.type)
            {
                call = RvalFnCallValue(scp->rval)->name;
            }

            ClassRef parent_ref = IDRefQualify(ctx, call);

            // We don't do a more detailed check for circular
            // inheritance because the depth check above will catch it
            if (strcmp(parent_ref.name, bp->name) == 0)
            {
                Log(LOG_LEVEL_ERR, "EvalContextAppendBodyParentsAndArgs: self body inheritance in %s->%s, aborting", bp->name, parent_ref.name);
                DoCleanupAndExit(EXIT_FAILURE);
            }

            const Body *parent = EvalContextFindFirstMatchingBody(policy, callee_type, parent_ref.ns, parent_ref.name);
            if (parent)
            {
                SeqAppend(chain, (void *)parent);
                SeqAppend(chain, &(scp->rval));
                EvalContextAppendBodyParentsAndArgs(ctx, policy, chain, parent, callee_type, depth+1);
            }
            ClassRefDestroy(parent_ref);
        }
    }
}

Seq *EvalContextResolveBodyExpression(const EvalContext *ctx, const Policy *policy,
                                      const char *callee_reference, const char *callee_type)
{
    ClassRef ref = IDRefQualify(ctx, callee_reference);
    Seq *bodies = NULL;

    const Body *bp = EvalContextFindFirstMatchingBody(policy, callee_type, ref.ns, ref.name);
    if (bp)
    {
        bodies = SeqNew(2, NULL);
        SeqAppend(bodies, (void *)bp);
        SeqAppend(bodies, (void *)NULL);
        EvalContextAppendBodyParentsAndArgs(ctx, policy, bodies, bp, callee_type, 1);
    }

    ClassRefDestroy(ref);
    return bodies;
}

bool EvalContextPromiseLockCacheContains(const EvalContext *ctx, const char *key)
{
    return StringSetContains(ctx->promise_lock_cache, key);
}

void EvalContextPromiseLockCachePut(EvalContext *ctx, const char *key)
{
    StringSetAdd(ctx->promise_lock_cache, xstrdup(key));
}

void EvalContextPromiseLockCacheRemove(EvalContext *ctx, const char *key)
{
    StringSetRemove(ctx->promise_lock_cache, key);
}

bool EvalContextFunctionCacheGet(const EvalContext *ctx,
                                 const FnCall *fp ARG_UNUSED,
                                 const Rlist *args, Rval *rval_out)
{
    assert(fp != NULL);
    assert(fp->name != NULL);
    assert(ctx != NULL);

    if (!(ctx->eval_options & EVAL_OPTION_CACHE_SYSTEM_FUNCTIONS))
    {
        return false;
    }

    // The cache key is made of the function name and all args values
    Rlist *args_copy = RlistCopy(args);
    Rlist *key = RlistPrepend(&args_copy, fp->name, RVAL_TYPE_SCALAR);
    Rval *rval = FuncCacheMapGet(ctx->function_cache, key);
    RlistDestroy(key);
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

void EvalContextFunctionCachePut(EvalContext *ctx,
                                 const FnCall *fp ARG_UNUSED,
                                 const Rlist *args, const Rval *rval)
{
    assert(fp != NULL);
    assert(fp->name != NULL);
    assert(ctx != NULL);

    if (!(ctx->eval_options & EVAL_OPTION_CACHE_SYSTEM_FUNCTIONS))
    {
        return;
    }

    Rval *rval_copy = xmalloc(sizeof(Rval));
    *rval_copy = RvalCopy(*rval);

    Rlist *args_copy = RlistCopy(args);
    Rlist *key = RlistPrepend(&args_copy, fp->name, RVAL_TYPE_SCALAR);

    FuncCacheMapInsert(ctx->function_cache, key, rval_copy);
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
    return pp && (PromiseGetPromiseType(pp) != NULL) && (!IsStrIn(PromiseGetPromiseType(pp), NO_STATUS_TYPES));
}

/*
 * Vars, classes and subordinate promises (like edit_line) do not need to be
 * logged, as they exist to support other promises.
 */

static bool IsPromiseValuableForLogging(const Promise *pp)
{
    return pp && (PromiseGetPromiseType(pp) != NULL) && (!IsStrIn(PromiseGetPromiseType(pp), NO_LOG_TYPES));
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

            Log(LOG_LEVEL_VERBOSE, "C:    + persistent outcome class '%s'", classname);
            EvalContextHeapPersistentSave(ctx, classname, persistence_ttl, policy, "");
            EvalContextClassPutSoft(ctx, classname, CONTEXT_SCOPE_NAMESPACE, "");
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "C:    + promise outcome class '%s'", classname);

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
            return;
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

void SetPromiseOutcomeClasses(EvalContext *ctx, PromiseResult status, const DefineClasses *dc)
{
    Rlist *add_classes = NULL;
    Rlist *del_classes = NULL;

    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        add_classes = dc->change;
        del_classes = dc->del_change;
        break;

    case PROMISE_RESULT_TIMEOUT:
        add_classes = dc->timeout;
        del_classes = dc->del_notkept;
        break;

    case PROMISE_RESULT_WARN:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_INTERRUPTED:
        add_classes = dc->failure;
        del_classes = dc->del_notkept;
        break;

    case PROMISE_RESULT_DENIED:
        add_classes = dc->denied;
        del_classes = dc->del_notkept;
        break;

    case PROMISE_RESULT_NOOP:
        add_classes = dc->kept;
        del_classes = dc->del_kept;
        break;

    default:
        ProgrammingError("Unexpected status '%c' has been passed to SetPromiseOutcomeClasses", status);
    }

    AddAllClasses(ctx, add_classes, dc->persist, dc->timer, dc->scope);
    DeleteAllClasses(ctx, del_classes);
}

static void SummarizeTransaction(EvalContext *ctx, const TransactionContext *tc, const char *logname)
{
    if (logname && (tc->log_string))
    {
        Buffer *buffer = BufferNew();
        ExpandScalar(ctx, NULL, NULL, tc->log_string, buffer);

        if (strcmp(logname, "udp_syslog") == 0)
        {
            RemoteSysLog(tc->log_priority, BufferData(buffer));
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
                    Log(LOG_LEVEL_VERBOSE,
                        "Created log file '%s' with requested permissions %jo",
                        logname, (intmax_t) filemode);
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
        // FIXME: This was overwriting a local copy, with no side effects.
        // The intention was clearly to skip this function if called
        // repeatedly. Try to introduce this change:
        // tc.log_string = NULL;     /* To avoid repetition */
    }
}

static void DoSummarizeTransaction(EvalContext *ctx, PromiseResult status, const Promise *pp, const TransactionContext *tc)
{
    if (!IsPromiseValuableForLogging(pp))
    {
        return;
    }

    char *log_name = NULL;

    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        log_name = tc->log_repaired;
        break;

    case PROMISE_RESULT_WARN:
        /* FIXME: nothing? */
        return;

    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_INTERRUPTED:
        log_name = tc->log_failed;
        break;

    case PROMISE_RESULT_NOOP:
        log_name = tc->log_kept;
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

void ClassAuditLog(EvalContext *ctx, const Promise *pp, const Attributes *attr, PromiseResult status)
{
    assert(attr != NULL);
    if (IsPromiseValuableForStatus(pp))
    {
        TrackTotalCompliance(status, pp);
        UpdatePromiseCounters(status);
    }

    SetPromiseOutcomeClasses(ctx, status, &(attr->classes));
    DoSummarizeTransaction(ctx, status, pp, &(attr->transaction));
}

static void LogPromiseContext(const EvalContext *ctx, const Promise *pp)
{
    if (!WouldLog(LOG_LEVEL_VERBOSE))
    {
        return;
    }

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

void cfPS(EvalContext *ctx, LogLevel level, PromiseResult status, const Promise *pp, const Attributes *attr, const char *fmt, ...)
{
    assert(pp != NULL);
    assert(attr != NULL);

    /* Either logging something (based on 'fmt') or logging nothing. */
    assert(!NULL_OR_EMPTY(fmt) || (level == LOG_LEVEL_NOTHING));

    if (!NULL_OR_EMPTY(fmt))
    {
        if (level >= LOG_LEVEL_VERBOSE)
        {
            LogPromiseContext(ctx, pp);
        }

        va_list ap;
        va_start(ap, fmt);
        VLog(level, fmt, ap);
        va_end(ap);
    }

    /* Now complete the exits status classes and auditing */

    if (status != PROMISE_RESULT_SKIPPED)
    {
        ClassAuditLog(ctx, pp, attr, status);
    }
}

void RecordChange(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...)
{
    assert(ctx != NULL);
    assert(pp != NULL);
    assert(attr != NULL);

    LogPromiseContext(ctx, pp);

    va_list ap;
    va_start(ap, fmt);
    VLog(LOG_LEVEL_INFO, fmt, ap);
    va_end(ap);

    SetPromiseOutcomeClasses(ctx, PROMISE_RESULT_CHANGE, &(attr->classes));
}

void RecordNoChange(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...)
{
    assert(ctx != NULL);
    assert(pp != NULL);
    assert(attr != NULL);

    LogPromiseContext(ctx, pp);

    va_list ap;
    va_start(ap, fmt);
    VLog(LOG_LEVEL_VERBOSE, fmt, ap);
    va_end(ap);

    SetPromiseOutcomeClasses(ctx, PROMISE_RESULT_NOOP, &(attr->classes));
}

void RecordFailure(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...)
{
    assert(ctx != NULL);
    assert(pp != NULL);
    assert(attr != NULL);

    LogPromiseContext(ctx, pp);

    va_list ap;
    va_start(ap, fmt);
    VLog(LOG_LEVEL_ERR, fmt, ap);
    va_end(ap);

    SetPromiseOutcomeClasses(ctx, PROMISE_RESULT_FAIL, &(attr->classes));
}

void RecordWarning(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...)
{
    assert(ctx != NULL);
    assert(pp != NULL);
    assert(attr != NULL);

    LogPromiseContext(ctx, pp);

    va_list ap;
    va_start(ap, fmt);
    VLog(LOG_LEVEL_WARNING, fmt, ap);
    va_end(ap);

    SetPromiseOutcomeClasses(ctx, PROMISE_RESULT_WARN, &(attr->classes));
}

void RecordDenial(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...)
{
    assert(ctx != NULL);
    assert(pp != NULL);
    assert(attr != NULL);

    LogPromiseContext(ctx, pp);

    va_list ap;
    va_start(ap, fmt);
    VLog(LOG_LEVEL_ERR, fmt, ap);
    va_end(ap);

    SetPromiseOutcomeClasses(ctx, PROMISE_RESULT_DENIED, &(attr->classes));
}

void RecordInterruption(EvalContext *ctx, const Promise *pp, const Attributes *attr, const char *fmt, ...)
{
    assert(ctx != NULL);
    assert(pp != NULL);
    assert(attr != NULL);

    LogPromiseContext(ctx, pp);

    va_list ap;
    va_start(ap, fmt);
    VLog(LOG_LEVEL_ERR, fmt, ap);
    va_end(ap);

    SetPromiseOutcomeClasses(ctx, PROMISE_RESULT_INTERRUPTED, &(attr->classes));
}

bool MakingChanges(EvalContext *ctx, const Promise *pp, const Attributes *attr,
                   PromiseResult *result, const char *change_desc_fmt, ...)
{
    assert(attr != NULL);

    if ((EVAL_MODE != EVAL_MODE_DRY_RUN) && (attr->transaction.action != cfa_warn))
    {
        return true;
    }
    /* else */
    char *fmt = NULL;
    if (attr->transaction.action == cfa_warn)
    {
        xasprintf(&fmt, "Should %s, but only warning promised", change_desc_fmt);
    }
    else
    {
        xasprintf(&fmt, "Should %s", change_desc_fmt);
    }

    LogPromiseContext(ctx, pp);

    va_list ap;
    va_start(ap, change_desc_fmt);
    VLog(LOG_LEVEL_WARNING, fmt, ap);
    va_end(ap);

    free(fmt);

    SetPromiseOutcomeClasses(ctx, PROMISE_RESULT_WARN, &(attr->classes));

    if (result != NULL)
    {
        *result = PROMISE_RESULT_WARN;
    }

    return false;
}

bool MakingInternalChanges(EvalContext *ctx, const Promise *pp, const Attributes *attr,
                           PromiseResult *result, const char *change_desc_fmt, ...)
{
    assert(attr != NULL);

    if ((EVAL_MODE == EVAL_MODE_NORMAL) && (attr->transaction.action != cfa_warn))
    {
        return true;
    }
    /* else */
    char *fmt = NULL;
    if (attr->transaction.action == cfa_warn)
    {
        xasprintf(&fmt, "Should %s, but only warning promised", change_desc_fmt);
    }
    else
    {
        xasprintf(&fmt, "Should %s", change_desc_fmt);
    }

    LogPromiseContext(ctx, pp);

    va_list ap;
    va_start(ap, change_desc_fmt);
    VLog(LOG_LEVEL_WARNING, fmt, ap);
    va_end(ap);

    free(fmt);

    SetPromiseOutcomeClasses(ctx, PROMISE_RESULT_WARN, &(attr->classes));

    if (result != NULL)
    {
        *result = PROMISE_RESULT_WARN;
    }

    return false;
}

void SetChecksumUpdatesDefault(EvalContext *ctx, bool enabled)
{
    ctx->checksum_updates_default = enabled;
}

bool GetChecksumUpdatesDefault(const EvalContext *ctx)
{
    return ctx->checksum_updates_default;
}

void EvalContextAddIpAddress(EvalContext *ctx, const char *ip_address, const char *iface)
{
    AppendItem(&ctx->ip_addresses, ip_address,
               (iface == NULL) ? "" : iface);
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
    return ((ctx->eval_options & option) != 0);
}

void EvalContextSetLaunchDirectory(EvalContext *ctx, const char *path)
{
    free(ctx->launch_directory);
    ctx->launch_directory = xstrdup(path);
}

void EvalContextSetEntryPoint(
    EvalContext *const ctx, const char *const entry_point)
{
    assert(ctx != NULL);
    free(ctx->entry_point);
    ctx->entry_point = SafeStringDuplicate(entry_point);
}

const char *EvalContextGetEntryPoint(EvalContext *const ctx)
{
    assert(ctx != NULL);
    return ctx->entry_point;
}

void EvalContextSetIgnoreLocks(EvalContext *ctx, bool ignore)
{
    ctx->ignore_locks = ignore;
}

bool EvalContextIsIgnoringLocks(const EvalContext *ctx)
{
    return ctx->ignore_locks;
}

StringSet *ClassesMatchingLocalRecursive(
    const EvalContext *ctx,
    const char *regex,
    const Rlist *tags,
    bool first_only,
    size_t stack_index)
{
    assert(ctx != NULL);
    StackFrame *frame = SeqAt(ctx->stack, stack_index);
    StringSet *matches;
    if (frame->type == STACK_FRAME_TYPE_BUNDLE)
    {
        ClassTableIterator *iter = ClassTableIteratorNew(
            frame->data.bundle.classes,
            frame->data.bundle.owner->ns,
            false,
            true); // from EvalContextClassTableIteratorNewLocal()
        matches = ClassesMatching(ctx, iter, regex, tags, first_only);
        ClassTableIteratorDestroy(iter);
    }
    else
    {
        matches = StringSetNew(); // empty for passing up the recursion chain
    }

    if (stack_index > 0 && frame->inherits_previous)
    {
        StringSet *parent_matches = ClassesMatchingLocalRecursive(
            ctx, regex, tags, first_only, stack_index - 1);
        StringSetJoin(matches, parent_matches, xstrdup);
        StringSetDestroy(parent_matches);
    }

    return matches;
}

StringSet *ClassesMatchingLocal(
    const EvalContext *ctx,
    const char *regex,
    const Rlist *tags,
    bool first_only)
{
    assert(ctx != NULL);
    return ClassesMatchingLocalRecursive(
        ctx, regex, tags, first_only, SeqLength(ctx->stack) - 1);
}

StringSet *ClassesMatchingGlobal(
    const EvalContext *ctx,
    const char *regex,
    const Rlist *tags,
    bool first_only)
{
    ClassTableIterator *iter =
        EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
    StringSet *matches = ClassesMatching(ctx, iter, regex, tags, first_only);
    ClassTableIteratorDestroy(iter);
    return matches;
}
StringSet *ClassesMatching(const EvalContext *ctx, ClassTableIterator *iter, const char* regex, const Rlist *tags, bool first_only)
{
    StringSet *matching = StringSetNew();

    Regex *rx = CompileRegex(regex);

    Class *cls;
    while ((cls = ClassTableIteratorNext(iter)))
    {
        char *expr = ClassRefToString(cls->ns, cls->name);

        /* FIXME: review this strcmp. Moved out from StringMatch */
        if (!strcmp(regex, expr) ||
            (rx && StringMatchFullWithPrecompiledRegex(rx, expr)))
        {
            bool pass = false;
            StringSet *tagset = EvalContextClassTags(ctx, cls->ns, cls->name);

            if (tags)
            {
                for (const Rlist *arg = tags; arg; arg = arg->next)
                {
                    const char *tag_regex = RlistScalarValue(arg);
                    const char *element;
                    StringSetIterator it = StringSetIteratorInit(tagset);
                    while ((element = StringSetIteratorNext(&it)))
                    {
                        /* FIXME: review this strcmp. Moved out from StringMatch */
                        if (strcmp(tag_regex, element) == 0 ||
                            StringMatchFull(tag_regex, element))
                        {
                            pass = true;
                            break;
                        }
                    }
                }
            }
            else                        // without any tags queried, accept class
            {
                pass = true;
            }

            if (pass)
            {
                StringSetAdd(matching, expr);
            }
            else
            {
                free(expr);
            }
        }
        else
        {
            free(expr);
        }

        if (first_only && StringSetSize(matching) > 0)
        {
            break;
        }
    }

    if (rx)
    {
        RegexDestroy(rx);
    }

    return matching;
}

JsonElement* JsonExpandElement(EvalContext *ctx, const JsonElement *source)
{
    if (JsonGetElementType(source) == JSON_ELEMENT_TYPE_PRIMITIVE)
    {
        Buffer *expbuf;
        JsonElement *expanded_json;

        if (JsonGetPrimitiveType(source) == JSON_PRIMITIVE_TYPE_STRING)
        {
            expbuf = BufferNew();
            ExpandScalar(ctx, NULL, "this", JsonPrimitiveGetAsString(source), expbuf);
            expanded_json = JsonStringCreate(BufferData(expbuf));
            BufferDestroy(expbuf);
            return expanded_json;
        }
        else
        {
            return JsonCopy(source);
        }
    }
    else if (JsonGetElementType(source) == JSON_ELEMENT_TYPE_CONTAINER)
    {
        if (JsonGetContainerType(source) == JSON_CONTAINER_TYPE_OBJECT)
        {
            JsonElement *dest = JsonObjectCreate(JsonLength(source));
            JsonIterator iter = JsonIteratorInit(source);
            const char *key;
            while ((key = JsonIteratorNextKey(&iter)))
            {
                Buffer *expbuf = BufferNew();
                ExpandScalar(ctx, NULL, "this", key, expbuf);
                JsonObjectAppendElement(dest, BufferData(expbuf), JsonExpandElement(ctx, JsonObjectGet(source, key)));
                BufferDestroy(expbuf);
            }

            return dest;
        }
        else
        {
            JsonElement *dest = JsonArrayCreate(JsonLength(source));
            for (size_t i = 0; i < JsonLength(source); i++)
            {
                JsonArrayAppendElement(dest, JsonExpandElement(ctx, JsonArrayGet(source, i)));
            }
            return dest;
        }
    }

    ProgrammingError("JsonExpandElement: unexpected container type");
    return NULL;
}

const StringSet *EvalContextAllClassesGet(const EvalContext *ctx)
{
    assert (ctx);
    return ctx->all_classes;
}

void EvalContextAllClassesLoggingEnable(EvalContext *ctx, bool enable)
{
    assert (ctx);
    Nova_ClassHistoryEnable(&(ctx->all_classes), enable);
}

void EvalContextPushBundleName(const EvalContext *ctx, const char *bundle_name)
{
    assert (ctx);
    StringSetAdd(ctx->bundle_names, xstrdup(bundle_name));
}

const StringSet *EvalContextGetBundleNames(const EvalContext *ctx)
{
    assert (ctx);
    return ctx->bundle_names;
}

void EvalContextPushRemoteVarPromise(EvalContext *ctx, const char *bundle_name, const Promise *pp)
{
    assert (ctx);

    /* initiliaze the map if needed */
    if (ctx->remote_var_promises == NULL)
    {
        ctx->remote_var_promises = RemoteVarPromisesMapNew();
    }

    Seq *promises = RemoteVarPromisesMapGet(ctx->remote_var_promises, bundle_name);
    if (promises == NULL)
    {
        /* initialize the sequence if needed */
        /* ItemDestroy == NULL because we need to store the exact pointers not
         * copies */
        promises = SeqNew(10, NULL);
        RemoteVarPromisesMapInsert(ctx->remote_var_promises, xstrdup(bundle_name), promises);
    }
    /* intentionally not making a copy here, we need the exact pointer */
    SeqAppend(promises, (void *) pp);
}

const Seq *EvalContextGetRemoteVarPromises(const EvalContext *ctx, const char *bundle_name)
{
    assert (ctx);
    if (ctx->remote_var_promises == NULL)
    {
        return NULL;
    }
    return RemoteVarPromisesMapGet(ctx->remote_var_promises, bundle_name);
}

void EvalContextSetDumpReports(EvalContext *ctx, bool dump_reports)
{
    assert(ctx != NULL);
    ctx->dump_reports = dump_reports;
    if (dump_reports)
    {
        Log(LOG_LEVEL_VERBOSE, "Report dumping is enabled");
    }
}

bool EvalContextGetDumpReports(EvalContext *ctx)
{
    assert(ctx != NULL);

    return ctx->dump_reports;
}

void EvalContextUpdateDumpReports(EvalContext *ctx)
{
    assert(ctx != NULL);

    char enable_file_path[PATH_MAX];
    snprintf(
        enable_file_path,
        PATH_MAX,
        "%s%cenable_report_dumps",
        GetWorkDir(),
        FILE_SEPARATOR);
    EvalContextSetDumpReports(ctx, (access(enable_file_path, F_OK) == 0));
}

static char chrooted_path[PATH_MAX + 1] = {0};
static size_t chroot_len = 0;
void SetChangesChroot(const char *chroot)
{
    assert(chroot != NULL);

    /* This function should only be called once. */
    assert(chroot_len == 0);

    chroot_len = SafeStringLength(chroot);

    memcpy(chrooted_path, chroot, chroot_len);

    /* Make sure there is a file separator at the end. */
    if (!IsFileSep(chroot[chroot_len - 1]))
    {
        chroot_len++;
        chrooted_path[chroot_len - 1] = FILE_SEPARATOR;
    }
}

const char *ToChangesChroot(const char *orig_path)
{
    /* SetChangesChroot() should be called first. */
    assert(chroot_len != 0);

    assert(orig_path != NULL);
    assert(IsAbsPath(orig_path));
    assert(strlen(orig_path) <= (PATH_MAX - chroot_len - 1));

    size_t offset = 0;
#ifdef __MINGW32__
    /* On Windows, absolute path starts with the drive letter and colon followed
     * by '\'. Let's replace the ":\" with just "\" so that each drive has its
     * own directory tree in the chroot. */
    if ((orig_path[0] > 'A') && ((orig_path[0] < 'Z')) && (orig_path[1] == ':'))
    {
        chrooted_path[chroot_len] = orig_path[0];
        chrooted_path[chroot_len + 1] = FILE_SEPARATOR;
        orig_path += 2;
        offset += 2;
    }
#endif

    while (orig_path[0] == FILE_SEPARATOR)
    {
        orig_path++;
    }

    /* Adds/copies the NUL-byte at the end of the string. */
    strncpy(chrooted_path + chroot_len + offset, orig_path, (PATH_MAX - chroot_len - offset - 1));

    return chrooted_path;
}

const char *ToNormalRoot(const char *orig_path)
{
    assert(strncmp(orig_path, chrooted_path, chroot_len) == 0);

    return orig_path + chroot_len - 1;
}

void EvalContextOverrideImmutableSet(EvalContext *ctx, bool should_override)
{
    assert(ctx != NULL);
    StackFrame *stack_frame = LastStackFrame(ctx, 0);
    assert(stack_frame != NULL);
    stack_frame->override_immutable = should_override;
}

bool EvalContextOverrideImmutableGet(EvalContext *ctx)
{
    assert(ctx != NULL);
    StackFrame *stack_frame = LastStackFrame(ctx, 0);
    assert(stack_frame != NULL);
    return stack_frame->override_immutable;
}
