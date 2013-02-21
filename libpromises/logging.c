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
#include "vars.h"
#include "cfstream.h"
#include "string_lib.h"
#include "transaction.h"
#include "policy.h"
#include "rlist.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

#define CF_VALUE_LOG      "cf_value.log"

static const char *NO_STATUS_TYPES[] = { "vars", "classes", NULL };
static const char *NO_LOG_TYPES[] =
    { "vars", "classes", "insert_lines", "delete_lines", "replace_patterns", "field_edits", NULL };

/*****************************************************************************/

static double VAL_KEPT;
static double VAL_REPAIRED;
static double VAL_NOTKEPT;

int PR_KEPT;
int PR_REPAIRED;
int PR_NOTKEPT;

static bool END_AUDIT_REQUIRED = false;

/*****************************************************************************/

void BeginAudit()
{
    END_AUDIT_REQUIRED = true;
}

/*****************************************************************************/

void EndAudit(int background_tasks)
{
    if (!END_AUDIT_REQUIRED)
    {
        return;
    }

    char *sp, string[CF_BUFSIZE];
    Rval retval;
    Promise dummyp = { 0 };
    Attributes dummyattr = { {0} };

    memset(&dummyp, 0, sizeof(dummyp));
    memset(&dummyattr, 0, sizeof(dummyattr));

    if (BooleanControl("control_agent", CFA_CONTROLBODY[AGENT_CONTROL_TRACK_VALUE].lval))
    {
        FILE *fout;
        char name[CF_MAXVARSIZE], datestr[CF_MAXVARSIZE];
        time_t now = time(NULL);

        CfOut(OUTPUT_LEVEL_INFORM, "", " -> Recording promise valuations");

        snprintf(name, CF_MAXVARSIZE, "%s/state/%s", CFWORKDIR, CF_VALUE_LOG);
        snprintf(datestr, CF_MAXVARSIZE, "%s", cf_ctime(&now));

        if ((fout = fopen(name, "a")) == NULL)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! Unable to write to the value log %s\n", name);
            return;
        }

        if (Chop(datestr, CF_EXPANDSIZE) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
        }
        fprintf(fout, "%s,%.4lf,%.4lf,%.4lf\n", datestr, VAL_KEPT, VAL_REPAIRED, VAL_NOTKEPT);
        TrackValue(datestr, VAL_KEPT, VAL_REPAIRED, VAL_NOTKEPT);
        fclose(fout);
    }

    double total = (double) (PR_KEPT + PR_NOTKEPT + PR_REPAIRED) / 100.0;

    if (GetVariable("control_common", "version", &retval) != DATA_TYPE_NONE)
    {
        sp = (char *) retval.item;
    }
    else
    {
        sp = "(not specified)";
    }

    if (total == 0)
    {
        *string = '\0';
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Outcome of version %s: No checks were scheduled\n", sp);
        return;
    }
    else
    {
        LogTotalCompliance(sp, background_tasks);
    }
}

/*****************************************************************************/

/*
 * Vars, classes and similar promises which do not affect the system itself (but
 * just support evalution) do not need to be counted as repaired/failed, as they
 * may change every iteration and introduce lot of churn in reports without
 * giving any value.
 */
static bool IsPromiseValuableForStatus(const Promise *pp)
{
    return pp && (pp->agentsubtype != NULL) && (!IsStrIn(pp->agentsubtype, NO_STATUS_TYPES));
}

/*****************************************************************************/

/*
 * Vars, classes and subordinate promises (like edit_line) do not need to be
 * logged, as they exist to support other promises.
 */

static bool IsPromiseValuableForLogging(const Promise *pp)
{
    return pp && (pp->agentsubtype != NULL) && (!IsStrIn(pp->agentsubtype, NO_LOG_TYPES));
}

/*****************************************************************************/

void ClassAuditLog(const Promise *pp, Attributes attr, char status, char *reason)
{
    switch (status)
    {
    case CF_CHG:

        if (IsPromiseValuableForStatus(pp))
        {
            if (!EDIT_MODEL)
            {
                PR_REPAIRED++;
                VAL_REPAIRED += attr.transaction.value_repaired;

#ifdef HAVE_NOVA
                EnterpriseTrackTotalCompliance(pp, 'r');
#endif
            }
        }

        AddAllClasses(pp->ns, attr.classes.change, attr.classes.persist, attr.classes.timer);
        MarkPromiseHandleDone(pp);
        DeleteAllClasses(attr.classes.del_change);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, 0.5, PROMISE_STATE_REPAIRED, reason);
            SummarizeTransaction(attr, pp, attr.transaction.log_repaired);
        }
        break;

    case CF_WARN:

        if (IsPromiseValuableForStatus(pp))
        {
            PR_NOTKEPT++;
            VAL_NOTKEPT += attr.transaction.value_notkept;

#ifdef HAVE_NOVA
            EnterpriseTrackTotalCompliance(pp, 'n');
#endif
        }

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, 1.0, PROMISE_STATE_NOTKEPT, reason);
        }
        break;

    case CF_TIMEX:

        if (IsPromiseValuableForStatus(pp))
        {
            PR_NOTKEPT++;
            VAL_NOTKEPT += attr.transaction.value_notkept;

#ifdef HAVE_NOVA
            EnterpriseTrackTotalCompliance(pp, 'n');
#endif
        }

        AddAllClasses(pp->ns, attr.classes.timeout, attr.classes.persist, attr.classes.timer);
        DeleteAllClasses(attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, 0.0, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(attr, pp, attr.transaction.log_failed);
        }
        break;

    case CF_FAIL:

        if (IsPromiseValuableForStatus(pp))
        {
            PR_NOTKEPT++;
            VAL_NOTKEPT += attr.transaction.value_notkept;

#ifdef HAVE_NOVA
            EnterpriseTrackTotalCompliance(pp, 'n');
#endif
        }

        AddAllClasses(pp->ns, attr.classes.failure, attr.classes.persist, attr.classes.timer);
        DeleteAllClasses(attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, 0.0, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(attr, pp, attr.transaction.log_failed);
        }
        break;

    case CF_DENIED:

        if (IsPromiseValuableForStatus(pp))
        {
            PR_NOTKEPT++;
            VAL_NOTKEPT += attr.transaction.value_notkept;

#ifdef HAVE_NOVA
            EnterpriseTrackTotalCompliance(pp, 'n');
#endif
        }

        AddAllClasses(pp->ns, attr.classes.denied, attr.classes.persist, attr.classes.timer);
        DeleteAllClasses(attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, 0.0, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(attr, pp, attr.transaction.log_failed);
        }
        break;

    case CF_INTERPT:

        if (IsPromiseValuableForStatus(pp))
        {
            PR_NOTKEPT++;
            VAL_NOTKEPT += attr.transaction.value_notkept;

#ifdef HAVE_NOVA
            EnterpriseTrackTotalCompliance(pp, 'n');
#endif
        }

        AddAllClasses(pp->ns, attr.classes.interrupt, attr.classes.persist, attr.classes.timer);
        DeleteAllClasses(attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, 0.0, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(attr, pp, attr.transaction.log_failed);
        }
        break;

    case CF_UNKNOWN:
    case CF_NOP:

        AddAllClasses(pp->ns, attr.classes.kept, attr.classes.persist, attr.classes.timer);
        DeleteAllClasses(attr.classes.del_kept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, 1.0, PROMISE_STATE_ANY, reason);
            SummarizeTransaction(attr, pp, attr.transaction.log_kept);
        }

        if (IsPromiseValuableForStatus(pp))
        {
            PR_KEPT++;
            VAL_KEPT += attr.transaction.value_kept;

#ifdef HAVE_NOVA
            EnterpriseTrackTotalCompliance(pp, 'c');
#endif
        }

        MarkPromiseHandleDone(pp);
        break;
    }
}

/************************************************************************/

void PromiseLog(char *s)
{
    char filename[CF_BUFSIZE];
    time_t now = time(NULL);
    FILE *fout;

    if ((s == NULL) || (strlen(s) == 0))
    {
        return;
    }

    snprintf(filename, CF_BUFSIZE, "%s/%s", CFWORKDIR, CF_PROMISE_LOG);
    MapName(filename);

    if ((fout = fopen(filename, "a")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Could not open %s", filename);
        return;
    }

    fprintf(fout, "%" PRIdMAX ",%" PRIdMAX ": %s\n", (intmax_t)CFSTARTTIME, (intmax_t)now, s);
    fclose(fout);
}

/************************************************************************/

void PromiseBanner(Promise *pp)
{
    char handle[CF_MAXVARSIZE];
    const char *sp;

    if ((sp = ConstraintGetRvalValue("handle", pp, RVAL_TYPE_SCALAR)) || (sp = PromiseID(pp)))
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

    if (pp->ref)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "    Comment:  %s\n", pp->ref);
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

void FatalError(char *s, ...)
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

    EndAudit(0);
    exit(1);
}
