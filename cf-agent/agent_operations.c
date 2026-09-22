/*
  Copyright 2026 Northern.tech AS

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

/* ScheduleAgentOperations() and the promise actuator it uses: this is what
 * cf-agent does with a single bundle (evaluate its promises in
 * AGENT_TYPESEQUENCE order, keeping each one with KeepAgentPromise()). It is
 * kept separate from cf-agent.c so that other components (e.g. cf-reactor,
 * running the bundle named in an events promise's "then") can run a bundle
 * the same way cf-agent does, without linking cf-agent's main(). */

#include <platform.h>
#include <agent_operations.h>

#include <eval_context.h>
#include <expand.h>                     /* ExpandPromise() */
#include <ornaments.h>                  /* SpecialTypeBanner() */
#include <prototypes3.h>
#include <promises.h>
#include <item_lib.h>
#include <matching.h>                   /* IsRegexItemIn() */
#include <match_scope.h>                /* FullTextMatch() */
#include <instrumentation.h>
#include <processes_select.h>
#include <signals.h>
#include <syntax.h>                     /* IsBuiltInPromiseType() */
#include <mod_custom.h>                 /* EvaluateCustomPromise() */
#include <string_lib.h>
#include <nfs.h>
#include <verify_classes.h>
#include <verify_databases.h>
#include <verify_environments.h>
#include <verify_exec.h>
#include <verify_files.h>
#include <verify_methods.h>
#include <verify_packages.h>
#include <verify_processes.h>
#include <verify_services.h>
#include <verify_storage.h>
#include <verify_users.h>
#include <verify_vars.h>

extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

int CFA_BACKGROUND = 0; /* GLOBAL_X */
int CFA_BACKGROUND_LIMIT = 1; /* GLOBAL_P */

Item *PROCESSREFRESH = NULL; /* GLOBAL_P */

static const char *const AGENT_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "defaults",
    "classes",                  /* Maelstrom order 2 */
    "users",
    "files",
    "packages",
    "guest_environments",
    "methods",
    "processes",
    "services",
    "commands",
    "storage",
    "databases",
    "reports",
    NULL
};

static PromiseResult KeepAgentPromise(EvalContext *ctx, const Promise *pp, void *param);
static void NewTypeContext(TypeSequence type);
static void DeleteTypeContext(EvalContext *ctx, TypeSequence type);
static PromiseResult ParallelFindAndVerifyFilesPromises(EvalContext *ctx, const Promise *pp);
static void BannerStatusEnd(PromiseResult status, const char *type, char *name);
static void BannerStatusBegin(const char *type, char *name);
static PromiseResult DefaultVarPromise(EvalContext *ctx, const Promise *pp);
static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept, struct timespec start);

/**
 @brief
 Wrapper around DefaultVarPromise to silence cast-function-type compiler warning in ScheduleAgentOperations
 */
static PromiseResult DefaultVarPromiseWrapper(EvalContext *ctx, const Promise *pp, void *param) {
    UNUSED(param);
    return DefaultVarPromise(ctx, pp);
}

PromiseResult ScheduleAgentOperations(EvalContext *ctx, const Bundle *bp)
// NB - this function can be called recursively through "methods"
{
    if (EvalContextIsClassicOrder(ctx, bp))
    {
        return ScheduleAgentOperationsNormalOrder(ctx, bp);
    }
    return ScheduleAgentOperationsTopDownOrder(ctx, bp);
}

PromiseResult ScheduleAgentOperationsNormalOrder(EvalContext *ctx, const Bundle *bp)
{
    assert(bp != NULL);

    int save_pr_kept = PR_KEPT;
    int save_pr_repaired = PR_REPAIRED;
    int save_pr_notkept = PR_NOTKEPT;
    struct timespec start = BeginMeasure();

    if (PROCESSREFRESH == NULL || (PROCESSREFRESH && IsRegexItemIn(ctx, PROCESSREFRESH, bp->name)))
    {
        ClearProcessTable();
    }

    PromiseResult result = PROMISE_RESULT_SKIPPED;

    for (int pass = 1; pass < CF_DONEPASSES; pass++)
    {
        // Evaluate built-in (non-custom) promise types, according to type sequence (normal order):
        for (TypeSequence type = 0; AGENT_TYPESEQUENCE[type] != NULL; type++)
        {
            const BundleSection *sp = BundleGetSection((Bundle *)bp, AGENT_TYPESEQUENCE[type]);

            if (!sp || SeqLength(sp->promises) == 0)
            {
                continue;
            }

            NewTypeContext(type);

            SpecialTypeBanner(type, pass);
            EvalContextStackPushBundleSectionFrame(ctx, sp);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                EvalContextSetPass(ctx, pass);

                PromiseResult promise_result = ExpandPromise(ctx, pp, KeepAgentPromise, NULL);
                result = PromiseResultUpdate(result, promise_result);

                if (EvalAborted(ctx) || BundleAbort(ctx))
                {
                    DeleteTypeContext(ctx, type);
                    EvalContextStackPopFrame(ctx);
                    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
                    return result;
                }
            }

            DeleteTypeContext(ctx, type);
            EvalContextStackPopFrame(ctx);

            if (type == TYPE_SEQUENCE_CONTEXTS)
            {
                BundleResolve(ctx, bp);
                BundleResolvePromiseType(ctx, bp, "defaults", DefaultVarPromiseWrapper);
            }
        }

        // Custom promises are evaluated at the end of an evaluation pass:
        const size_t sections = SeqLength(bp->custom_sections);
        for (size_t i = 0; i < sections; ++i)
        {
            BundleSection *section = SeqAt(bp->custom_sections, i);

            EvalContextStackPushBundleSectionFrame(ctx, section);

            const size_t promises = SeqLength(section->promises);
            for (size_t ppi = 0; ppi < promises; ppi++)
            {
                Promise *pp = SeqAt(section->promises, ppi);

                EvalContextSetPass(ctx, pass);

                PromiseResult promise_result = ExpandPromise(ctx, pp, KeepAgentPromise, NULL);
                result = PromiseResultUpdate(result, promise_result);

                if (EvalAborted(ctx) || BundleAbort(ctx))
                {
                    EvalContextStackPopFrame(ctx);
                    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
                    return result;
                }
            }
            EvalContextStackPopFrame(ctx);
        }
    }

    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
    return result;
}

PromiseResult ScheduleAgentOperationsTopDownOrder(EvalContext *ctx, const Bundle *bp)
{
    assert(bp != NULL);

    int save_pr_kept = PR_KEPT;
    int save_pr_repaired = PR_REPAIRED;
    int save_pr_notkept = PR_NOTKEPT;
    struct timespec start = BeginMeasure();

    if (PROCESSREFRESH == NULL || (PROCESSREFRESH && IsRegexItemIn(ctx, PROCESSREFRESH, bp->name)))
    {
        ClearProcessTable();
    }

    PromiseResult result = PROMISE_RESULT_SKIPPED;
    for (int pass = 1; pass < CF_DONEPASSES; pass++)
    {
        const char *last_promise_type = "";
        for (size_t ppi = 0; ppi < SeqLength(bp->all_promises); ppi++)
        {
            EvalContextSetPass(ctx, pass);
            Promise *pp = SeqAt(bp->all_promises, ppi);
            BundleSection *parent_section = pp->parent_section;

            if (!StringEqual(last_promise_type, parent_section->promise_type))
            {
                SpecialTypeBannerFromString(parent_section->promise_type, pass);
            }
            last_promise_type = parent_section->promise_type;

            EvalContextStackPushBundleSectionFrame(ctx, parent_section);

            PromiseResult promise_result = ExpandPromise(ctx, pp, KeepAgentPromise, NULL);
            result = PromiseResultUpdate(result, promise_result);
            if (EvalAborted(ctx) || BundleAbort(ctx))
            {
                EvalContextStackPopFrame(ctx);
                NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
                return result;
            }
            EvalContextStackPopFrame(ctx);
        }
    }

    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
    return result;
}

/*********************************************************************/

static PromiseResult DefaultVarPromise(EvalContext *ctx, const Promise *pp)
{
    char *regex = PromiseGetConstraintAsRval(pp, "if_match_regex", RVAL_TYPE_SCALAR);
    bool okay = true;


    DataType value_type = CF_DATA_TYPE_NONE;
    const void *value = NULL;
    {
        VarRef *ref = VarRefParseFromScope(pp->promiser, "this");
        value = EvalContextVariableGetPlaintext(ctx, ref, &value_type);
        VarRefDestroy(ref);
    }

    switch (value_type)
    {
    case CF_DATA_TYPE_STRING:
    case CF_DATA_TYPE_INT:
    case CF_DATA_TYPE_REAL:
        if (regex && !FullTextMatch(ctx, regex, value))
        {
            return PROMISE_RESULT_NOOP;
        }

        if (regex == NULL)
        {
            return PROMISE_RESULT_NOOP;
        }
        break;

    case CF_DATA_TYPE_STRING_LIST:
    case CF_DATA_TYPE_INT_LIST:
    case CF_DATA_TYPE_REAL_LIST:
        if (regex)
        {
            for (const Rlist *rp = value; rp != NULL; rp = rp->next)
            {
                if (FullTextMatch(ctx, regex, RlistScalarValue(rp)))
                {
                    okay = false;
                    break;
                }
            }

            if (okay)
            {
                return PROMISE_RESULT_NOOP;
            }
        }
        break;

    default:
        break;
    }

    {
        VarRef *ref = VarRefParseFromBundle(pp->promiser, PromiseGetBundle(pp));
        EvalContextVariableRemove(ctx, ref);
        VarRefDestroy(ref);
    }

    return VerifyVarPromise(ctx, pp, NULL);
}

static void LogVariableValue(const EvalContext *ctx, const Promise *pp)
{
    VarRef *ref = VarRefParseFromBundle(pp->promiser, PromiseGetBundle(pp));
    char *out = NULL;

    DataType type;
    const void *var = EvalContextVariableGetPlaintext(ctx, ref, &type);
    switch (type)
    {
        case CF_DATA_TYPE_INT:
        case CF_DATA_TYPE_REAL:
        case CF_DATA_TYPE_STRING:
            out = xstrdup((char *) var);
            break;
        case CF_DATA_TYPE_INT_LIST:
        case CF_DATA_TYPE_REAL_LIST:
        case CF_DATA_TYPE_STRING_LIST:
        {
            size_t siz = CF_BUFSIZE;
            size_t len = 0;
            out = xcalloc(1, CF_BUFSIZE);

            for (Rlist *rp = (Rlist *) var; rp != NULL; rp = rp->next)
            {
                const char *s = (char *) rp->val.item;

                if (strlen(s) + len + 3  >= siz)                // ", " + NULL
                {
                    out = xrealloc(out, siz + CF_BUFSIZE);
                    siz += CF_BUFSIZE;
                }

                if (len > 0)
                {
                    len += strlcat(out, ", ", siz);
                }

                len += strlcat(out, s, siz);
            }
            break;
        }
        case CF_DATA_TYPE_CONTAINER:
        {
            Writer *w = StringWriter();
            JsonWriteCompact(w, (JsonElement *) var);
            out = StringWriterClose(w);
            break;
        }
        default:
            /* TODO is CF_DATA_TYPE_NONE acceptable? Today all meta variables
             * are of this type. */
            /* UnexpectedError("Variable '%s' is of unknown type %d", */
            /*                 pp->promiser, type); */
            out = xstrdup("NONE");
            break;
    }

    Log(LOG_LEVEL_DEBUG, "V: '%s' => '%s'", pp->promiser, out);
    free(out);
    VarRefDestroy(ref);
}

static PromiseResult KeepAgentPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);
    assert(pp != NULL);

    BannerStatusBegin(PromiseGetPromiseType(pp), pp->promiser);
    struct timespec start = BeginMeasure();
    PromiseResult result = PROMISE_RESULT_NOOP;

    if (strcmp("meta", PromiseGetPromiseType(pp)) == 0 ||
        strcmp("vars", PromiseGetPromiseType(pp)) == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "V:     Computing value of '%s'", pp->promiser);

        result = VerifyVarPromise(ctx, pp, NULL);
        if (result != PROMISE_RESULT_FAIL)
        {
            if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
            {
                LogVariableValue(ctx, pp);
            }
        }
    }
    else if (strcmp("defaults", PromiseGetPromiseType(pp)) == 0)
    {
        result = DefaultVarPromise(ctx, pp);
    }
    else if (strcmp("classes", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyClassPromise(ctx, pp, NULL);
    }
    else if (strcmp("processes", PromiseGetPromiseType(pp)) == 0)
    {
        if (!LoadProcessTable())
        {
            Log(LOG_LEVEL_ERR, "Unable to read the process table - cannot keep processes: type promises");
            return PROMISE_RESULT_FAIL;
        }
        result = VerifyProcessesPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("storage", PromiseGetPromiseType(pp)) == 0)
    {
        result = FindAndVerifyStoragePromises(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("packages", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyPackagesPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("users", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyUsersPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }

    else if (strcmp("files", PromiseGetPromiseType(pp)) == 0)
    {
        result = ParallelFindAndVerifyFilesPromises(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("commands", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyExecPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("databases", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyDatabasePromises(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("methods", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyMethodsPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("services", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyServicesPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("guest_environments", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyEnvironmentsPromise(ctx, pp);
        if (result != PROMISE_RESULT_SKIPPED)
        {
            EndMeasurePromise(start, pp);
        }
    }
    else if (strcmp("reports", PromiseGetPromiseType(pp)) == 0)
    {
        result = VerifyReportPromise(ctx, pp);
    }
    else if (!IsBuiltInPromiseType(PromiseGetPromiseType(pp)))
    {
        result = EvaluateCustomPromise(ctx, pp);
    }
    else
    {
        result = PROMISE_RESULT_NOOP;
    }

    BannerStatusEnd(result, PromiseGetPromiseType(pp), pp->promiser);
    EvalContextLogPromiseIterationOutcome(ctx, pp, result);
    return result;
}

static void BannerStatusBegin(const char *type, char *name)
{
    if (StringEqual(type, "vars") || StringEqual(type, "classes"))
    {
        return;
    }
    Log(LOG_LEVEL_VERBOSE, "P: BEGIN %s promise (%.30s%s)",
        type, name,
        (strlen(name) > 30) ? "..." : "");
}

static void BannerStatusEnd(PromiseResult status, const char *type, char *name)
{
    if ((strcmp(type, "vars") == 0) || (strcmp(type, "classes") == 0))
    {
        return;
    }

    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        Log(LOG_LEVEL_VERBOSE, "A: Promise REPAIRED");
        break;

    case PROMISE_RESULT_TIMEOUT:
        Log(LOG_LEVEL_VERBOSE, "A: Promise TIMED-OUT");
        break;

    case PROMISE_RESULT_WARN:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_INTERRUPTED:
        Log(LOG_LEVEL_VERBOSE, "A: Promise NOT KEPT!");
        break;

    case PROMISE_RESULT_DENIED:
        Log(LOG_LEVEL_VERBOSE, "A: Promise NOT KEPT - denied");
        break;

    case PROMISE_RESULT_NOOP:
        Log(LOG_LEVEL_VERBOSE, "A: Promise was KEPT");
        break;
    default:
        return;
        break;
    }

    Log(LOG_LEVEL_VERBOSE, "P: END %s promise (%.30s%s)",
        type, name,
        (strlen(name) > 30) ? "..." : "");
}

/*********************************************************************/
/* Type context                                                      */
/*********************************************************************/

static void NewTypeContext(TypeSequence type)
{
// get maxconnections

    switch (type)
    {
    case TYPE_SEQUENCE_ENVIRONMENTS:
        NewEnvironmentsContext();
        break;

    case TYPE_SEQUENCE_FILES:
        break;

    case TYPE_SEQUENCE_PROCESSES:
        break;

    case TYPE_SEQUENCE_STORAGE:
#ifndef __MINGW32__                   // TODO: Run if implemented on Windows
        if (SeqLength(GetGlobalMountedFSList()))
        {
            DeleteMountInfo(GetGlobalMountedFSList());
            SeqClear(GetGlobalMountedFSList());
        }
#endif /* !__MINGW32__ */
        break;

    default:
        break;
    }

    return;
}

/*********************************************************************/

static void DeleteTypeContext(EvalContext *ctx, TypeSequence type)
{
    switch (type)
    {
    case TYPE_SEQUENCE_ENVIRONMENTS:
        DeleteEnvironmentsContext();
        break;

    case TYPE_SEQUENCE_FILES:
        break;

    case TYPE_SEQUENCE_PROCESSES:
        break;

    case TYPE_SEQUENCE_STORAGE:
        DeleteStorageContext();
        break;

    case TYPE_SEQUENCE_PACKAGES:
        ExecuteScheduledPackages(ctx);
        CleanScheduledPackages();
        break;

    default:
        break;
    }
}

/**************************************************************/
/* Thread context                                             */
/**************************************************************/

#ifdef __MINGW32__

static PromiseResult ParallelFindAndVerifyFilesPromises(EvalContext *ctx, const Promise *pp)
{
    int background = PromiseGetConstraintAsBoolean(ctx, "background", pp);

    if (background)
    {
        Log(LOG_LEVEL_VERBOSE, "Background processing of files promises is not supported on Windows");
    }

    return FindAndVerifyFilesPromises(ctx, pp);
}

#else /* !__MINGW32__ */

static PromiseResult ParallelFindAndVerifyFilesPromises(EvalContext *ctx, const Promise *pp)
{
    int background = PromiseGetConstraintAsBoolean(ctx, "background", pp);
    pid_t child = 1;
    PromiseResult result = PROMISE_RESULT_SKIPPED;

    if (background)
    {
        if (CFA_BACKGROUND < CFA_BACKGROUND_LIMIT)
        {
            CFA_BACKGROUND++;
            Log(LOG_LEVEL_VERBOSE, "Spawning new process...");
            child = fork();

            if (child == 0)
            {
                ALARM_PID = -1;

                result = PromiseResultUpdate(result, FindAndVerifyFilesPromises(ctx, pp));

                Log(LOG_LEVEL_VERBOSE, "Exiting backgrounded promise");
                PromiseRef(LOG_LEVEL_VERBOSE, pp);
                _exit(EXIT_SUCCESS);
                // TODO: need to solve this
            }
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Promised parallel execution promised but exceeded the max number of promised background tasks, so serializing");
            background = 0;
        }
    }
    else
    {
        result = PromiseResultUpdate(result, FindAndVerifyFilesPromises(ctx, pp));
    }

    return result;
}

#endif /* !__MINGW32__ */

/*********************************************************************/
/* Compliance comp                                                   */
/*********************************************************************/

static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept, struct timespec start)
{
    double delta_pr_kept, delta_pr_repaired, delta_pr_notkept;
    double bundle_compliance = 0.0;

    delta_pr_kept = (double) (PR_KEPT - save_pr_kept);
    delta_pr_notkept = (double) (PR_NOTKEPT - save_pr_notkept);
    delta_pr_repaired = (double) (PR_REPAIRED - save_pr_repaired);

    Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
    Log(LOG_LEVEL_VERBOSE, "A: Bundle Accounting Summary for '%s' in namespace %s", bundle->name, bundle->ns);

    if (delta_pr_kept + delta_pr_notkept + delta_pr_repaired <= 0)
    {
        Log(LOG_LEVEL_VERBOSE, "A: Zero promises executed for bundle '%s'", bundle->name);
        Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
        return PROMISE_RESULT_NOOP;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "A: Promises kept in '%s' = %.0lf", bundle->name, delta_pr_kept);
        Log(LOG_LEVEL_VERBOSE, "A: Promises not kept in '%s' = %.0lf", bundle->name, delta_pr_notkept);
        Log(LOG_LEVEL_VERBOSE, "A: Promises repaired in '%s' = %.0lf", bundle->name, delta_pr_repaired);

        bundle_compliance = (delta_pr_kept + delta_pr_repaired) / (delta_pr_kept + delta_pr_notkept + delta_pr_repaired);

        Log(LOG_LEVEL_VERBOSE, "A: Aggregate compliance (promises kept/repaired) for bundle '%s' = %.1lf%%",
          bundle->name, bundle_compliance * 100.0);

        if (LogGetGlobalLevel() >= LOG_LEVEL_INFO)
        {
            char name[CF_MAXVARSIZE];
            snprintf(name, CF_MAXVARSIZE, "%s:%s", bundle->ns, bundle->name);
            EndMeasure(name, start);
        }
        else
        {
            EndMeasure(NULL, start);
        }
        Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
    }

    // return the worst case for the bundle status

    if (delta_pr_notkept > 0)
    {
        return PROMISE_RESULT_FAIL;
    }

    if (delta_pr_repaired > 0)
    {
        return PROMISE_RESULT_CHANGE;
    }

    return PROMISE_RESULT_NOOP;
}
