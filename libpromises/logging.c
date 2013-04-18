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

#include "logging.h"

#include "env_context.h"
#include "dbm_api.h"
#include "files_names.h"
#include "atexit.h"
#include "scope.h"
#include "cfstream.h"
#include "string_lib.h"
#include "policy.h"
#include "rlist.h"
#include "conversion.h"
#include "syntax.h"
#include "expand.h"
#include "misc_lib.h"
#include "audit.h"
#include "syslog_client.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

static const char *NO_STATUS_TYPES[] = { "vars", "classes", NULL };
static const char *NO_LOG_TYPES[] =
    { "vars", "classes", "insert_lines", "delete_lines", "replace_patterns", "field_edits", NULL };

static void SummarizeTransaction(EvalContext *ctx, TransactionContext tc, const char *logname);

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

/*****************************************************************************/

/*
 * Vars, classes and subordinate promises (like edit_line) do not need to be
 * logged, as they exist to support other promises.
 */

static bool IsPromiseValuableForLogging(const Promise *pp)
{
    return pp && (pp->parent_promise_type->name != NULL) && (!IsStrIn(pp->parent_promise_type->name, NO_LOG_TYPES));
}

/*****************************************************************************/

static void AddAllClasses(EvalContext *ctx, const char *ns, const Rlist *list, bool persist, ContextStatePolicy policy, ContextScope context_scope)
{
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        char *classname = xstrdup(rp->item);

        CanonifyNameInPlace(classname);

        if (EvalContextHeapContainsHard(ctx, classname))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! You cannot use reserved hard class \"%s\" as post-condition class", classname);
            // TODO: ok.. but should we take any action? continue; maybe?
        }

        if (persist > 0)
        {
            if (context_scope != CONTEXT_SCOPE_NAMESPACE)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", "Automatically promoting context scope for '%s' to namespace visibility, due to persistence", classname);
            }

            CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining persistent promise result class %s\n", classname);
            EvalContextHeapPersistentSave(CanonifyName(rp->item), ns, persist, policy);
            EvalContextHeapAddSoft(ctx, classname, ns);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining promise result class %s\n", classname);

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
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! You cannot cancel a reserved hard class \"%s\" in post-condition classes",
                  RlistScalarValue(rp));
        }

        const char *string = (char *) (rp->item);

        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Cancelling class %s\n", string);

        EvalContextHeapPersistentRemove(string);

        EvalContextHeapRemoveSoft(ctx, CanonifyName(string));

        EvalContextStackFrameAddNegated(ctx, CanonifyName(string));
    }
}

#ifdef HAVE_NOVA
static void TrackTotalCompliance(PromiseResult status, const Promise *pp)
{
    char nova_status;

    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        nova_status = 'r';
        break;

    case PROMISE_RESULT_WARN:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_INTERRUPTED:
        nova_status = 'n';
        break;

    case PROMISE_RESULT_NOOP:
        nova_status = 'c';
        break;

    default:
        ProgrammingError("Unexpected status '%c' has been passed to TrackTotalCompliance", status);
    }

    EnterpriseTrackTotalCompliance(pp, nova_status);
}
#endif


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

void UpdatePromiseComplianceStatus(PromiseResult status, const Promise *pp, char *reason)
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

void ClassAuditLog(EvalContext *ctx, const Promise *pp, Attributes attr, PromiseResult status)
{
    if (!IsPromiseValuableForStatus(pp) || EDIT_MODEL)
    {
#ifdef HAVE_NOVA
        TrackTotalCompliance(status, pp);
#endif
        UpdatePromiseCounters(status, attr.transaction);
    }

    SetPromiseOutcomeClasses(status, ctx, pp, attr.classes);
    NotifyDependantPromises(status, ctx, pp);
    DoSummarizeTransaction(ctx, status, pp, attr.transaction);
}

void PromiseBanner(Promise *pp)
{
    char handle[CF_MAXVARSIZE];
    const char *sp;

    if ((sp = PromiseGetHandle(pp)) || (sp = PromiseID(pp)))
    {
        strncpy(handle, sp, CF_MAXVARSIZE - 1);
    }
    else
    {
        strcpy(handle, "(enterprise only)");
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "    .........................................................\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s>     Promise's handle: %s\n", VPREFIX, handle);
        printf("%s>     Promise made by: \"%s\"", VPREFIX, pp->promiser);
    }

    if (pp->promisee.item)
    {
        if (VERBOSE)
        {
            printf("\n%s>     Promise made to (stakeholders): ", VPREFIX);
            RvalShow(stdout, pp->promisee);
        }
    }

    if (VERBOSE)
    {
        printf("\n");
    }

    if (pp->comment)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "    Comment:  %s\n", pp->comment);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "    .........................................................\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

/************************************************************************/

void BannerSubBundle(Bundle *bp, Rlist *params)
{
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");

    if (VERBOSE || DEBUG)
    {
        printf("%s>       BUNDLE %s", VPREFIX, bp->name);
    }

    if (params && (VERBOSE || DEBUG))
    {
        printf("(");
        RlistShow(stdout, params);
        printf(" )\n");
    }
    else
    {
        if (VERBOSE || DEBUG)
            printf("\n");
    }
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "      * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n");
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
}

/************************************************************************/

void FatalError(const EvalContext *ctx, char *s, ...)
{
    if (s)
    {
        va_list ap;
        char buf[CF_BUFSIZE] = "";

        va_start(ap, s);
        vsnprintf(buf, CF_BUFSIZE - 1, s, ap);
        va_end(ap);
        CfOut(OUTPUT_LEVEL_ERROR, "", "Fatal CFEngine error: %s", buf);
    }

    EndAudit(ctx, 0);
    exit(1);
}

static void SummarizeTransaction(EvalContext *ctx, TransactionContext tc, const char *logname)
{
    if (logname && (tc.log_string))
    {
        char buffer[CF_EXPANDSIZE];

        ExpandScalar(ctx, ScopeGetCurrent()->scope, tc.log_string, buffer);

        if (strcmp(logname, "udp_syslog") == 0)
        {
            RemoteSysLog(tc.log_priority, buffer);
        }
        else if (strcmp(logname, "stdout") == 0)
        {
            CfOut(OUTPUT_LEVEL_REPORTING, "", "L: %s\n", buffer);
        }
        else
        {
            FILE *fout = fopen(logname, "a");

            if (fout == NULL)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to open private log %s", logname);
                return;
            }

            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Logging string \"%s\" to %s\n", buffer, logname);
            fprintf(fout, "%s\n", buffer);

            fclose(fout);
        }

        tc.log_string = NULL;     /* To avoid repetition */
    }
}

