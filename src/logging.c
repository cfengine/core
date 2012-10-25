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

/*****************************************************************************/
/*                                                                           */
/* File: logging.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include "dbm_api.h"
#include "files_names.h"

static void ExtractOperationLock(char *op);

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

static CF_DB *AUDITDBP;

/*****************************************************************************/

void BeginAudit()
{
    Promise dummyp = { 0 };
    Attributes dummyattr = { {0} };

    if (THIS_AGENT_TYPE != cf_agent)
    {
        return;
    }

    memset(&dummyp, 0, sizeof(dummyp));
    memset(&dummyattr, 0, sizeof(dummyattr));

    ClassAuditLog(&dummyp, dummyattr, "Cfagent starting", CF_NOP, "");
}

/*****************************************************************************/

void EndAudit()
{
    double total;
    char *sp, string[CF_BUFSIZE];
    Rval retval;
    Promise dummyp = { 0 };
    Attributes dummyattr = { {0} };

    if (THIS_AGENT_TYPE != cf_agent)
    {
        return;
    }

    memset(&dummyp, 0, sizeof(dummyp));
    memset(&dummyattr, 0, sizeof(dummyattr));

    if (BooleanControl("control_agent", CFA_CONTROLBODY[cfa_track_value].lval))
    {
        FILE *fout;
        char name[CF_MAXVARSIZE], datestr[CF_MAXVARSIZE];
        time_t now = time(NULL);

        CfOut(cf_inform, "", " -> Recording promise valuations");

        snprintf(name, CF_MAXVARSIZE, "%s/state/%s", CFWORKDIR, CF_VALUE_LOG);
        snprintf(datestr, CF_MAXVARSIZE, "%s", cf_ctime(&now));

        if ((fout = fopen(name, "a")) == NULL)
        {
            CfOut(cf_inform, "", " !! Unable to write to the value log %s\n", name);
            return;
        }

        Chop(datestr);
        fprintf(fout, "%s,%.4lf,%.4lf,%.4lf\n", datestr, VAL_KEPT, VAL_REPAIRED, VAL_NOTKEPT);
        TrackValue(datestr, VAL_KEPT, VAL_REPAIRED, VAL_NOTKEPT);
        fclose(fout);
    }

    total = (double) (PR_KEPT + PR_NOTKEPT + PR_REPAIRED) / 100.0;

    if (GetVariable("control_common", "version", &retval) != cf_notype)
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
        CfOut(cf_verbose, "", "Outcome of version %s: No checks were scheduled\n", sp);
        return;
    }
    else
    {
        snprintf(string, CF_BUFSIZE,
                 "Outcome of version %s (%s-%d): Promises observed to be kept %.0f%%, Promises repaired %.0f%%, Promises not repaired %.0f\%%",
                 sp, THIS_AGENT, CFA_BACKGROUND, (double) PR_KEPT / total, (double) PR_REPAIRED / total,
                 (double) PR_NOTKEPT / total);

        CfOut(cf_verbose, "", "%s", string);
        PromiseLog(string);
    }

    if (strlen(string) > 0)
    {
        ClassAuditLog(&dummyp, dummyattr, string, CF_REPORT, "");
    }

    ClassAuditLog(&dummyp, dummyattr, "Cfagent closing", CF_NOP, "");
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
    return pp && pp->agentsubtype != NULL && !IsStrIn(pp->agentsubtype, NO_STATUS_TYPES);
}

/*****************************************************************************/

/*
 * Vars, classes and subordinate promises (like edit_line) do not need to be
 * logged, as they exist to support other promises.
 */

static bool IsPromiseValuableForLogging(const Promise *pp)
{
    return pp && pp->agentsubtype != NULL && !IsStrIn(pp->agentsubtype, NO_LOG_TYPES);
}

/*****************************************************************************/

void ClassAuditLog(Promise *pp, Attributes attr, char *str, char status, char *reason)
{
    time_t now = time(NULL);
    char date[CF_BUFSIZE], lock[CF_BUFSIZE], key[CF_BUFSIZE], operator[CF_BUFSIZE];
    AuditLog newaudit;
    Audit *ap = pp->audit;
    struct timespec t;
    double keyval;
    int lineno = pp->offset.line;

    CfDebug("ClassAuditLog(%s)\n", str);

    switch (status)
    {
    case CF_CHG:

        if (IsPromiseValuableForStatus(pp))
        {
            if (!EDIT_MODEL)
            {
                PR_REPAIRED++;
                VAL_REPAIRED += attr.transaction.value_repaired;
            }
        }

        AddAllClasses(attr.classes.change, attr.classes.persist, attr.classes.timer);
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
        }

        AddAllClasses(attr.classes.timeout, attr.classes.persist, attr.classes.timer);
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
        }

        AddAllClasses(attr.classes.failure, attr.classes.persist, attr.classes.timer);
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
        }

        AddAllClasses(attr.classes.denied, attr.classes.persist, attr.classes.timer);
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
        }

        AddAllClasses(attr.classes.interrupt, attr.classes.persist, attr.classes.timer);
        DeleteAllClasses(attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, 0.0, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(attr, pp, attr.transaction.log_failed);
        }
        break;

    case CF_UNKNOWN:
    case CF_NOP:

        AddAllClasses(attr.classes.kept, attr.classes.persist, attr.classes.timer);
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
        }

        break;
    }

    if (!(attr.transaction.audit || AUDIT))
    {
        return;
    }

    if (!OpenDB(&AUDITDBP, dbid_audit))
    {
        return;
    }

    if (AUDITDBP == NULL || THIS_AGENT_TYPE != cf_agent)
    {
        CloseDB(AUDITDBP);
        return;
    }

    snprintf(date, CF_BUFSIZE, "%s", cf_ctime(&now));
    Chop(date);

    ExtractOperationLock(lock);
    snprintf(operator, CF_BUFSIZE - 1, "[%s] op %s", date, lock);
    strncpy(newaudit.operator, operator, CF_AUDIT_COMMENT - 1);

    if (clock_gettime(CLOCK_REALTIME, &t) == -1)
    {
        CfOut(cf_verbose, "clock_gettime", "Clock gettime failure during audit transaction");
        return;
    }

// Auditing key needs microsecond precision to separate entries

    keyval = (double) (t.tv_sec) + (double) (t.tv_nsec) / (double) CF_BILLION;
    snprintf(key, CF_BUFSIZE - 1, "%lf", keyval);

    if (DEBUG)
    {
        AuditStatusMessage(stdout, status);
    }

    if (ap != NULL)
    {
        strncpy(newaudit.comment, str, CF_AUDIT_COMMENT - 1);
        strncpy(newaudit.filename, ap->filename, CF_AUDIT_COMMENT - 1);

        if (ap->version == NULL || strlen(ap->version) == 0)
        {
            CfDebug("Promised in %s bundle %s (unamed version last edited at %s) at/before line %d\n",
                    ap->filename, pp->bundle, ap->date, lineno);
            newaudit.version[0] = '\0';
        }
        else
        {
            CfDebug("Promised in %s bundle %s (version %s last edited at %s) at/before line %d\n", ap->filename,
                    pp->bundle, ap->version, ap->date, lineno);
            strncpy(newaudit.version, ap->version, CF_AUDIT_VERSION - 1);
        }

        strncpy(newaudit.date, ap->date, CF_AUDIT_DATE);
        newaudit.line_number = lineno;
    }
    else
    {
        strcpy(newaudit.date, date);
        strncpy(newaudit.comment, str, CF_AUDIT_COMMENT - 1);
        strcpy(newaudit.filename, "schedule");
        strcpy(newaudit.version, "");
        newaudit.line_number = 0;
    }

    newaudit.status = status;

    if (AUDITDBP && (attr.transaction.audit || AUDIT))
    {
        WriteDB(AUDITDBP, key, &newaudit, sizeof(newaudit));
    }

    CloseDB(AUDITDBP);
}

/************************************************************************/

static void ExtractOperationLock(char *op)
{
    char *sp, lastch = 'x';
    int i = 0, dots = 0;
    int offset = strlen("lock...") + strlen(VUQNAME);

/* Use the global copy of the lock from the main serial thread */

    for (sp = CFLOCK + offset; *sp != '\0'; sp++)
    {
        switch (*sp)
        {
        case '_':
            if (lastch == '_')
            {
                break;
            }
            else
            {
                op[i] = '/';
            }
            break;

        case '.':
            dots++;
            op[i] = *sp;
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            dots = 9;
            break;

        default:
            op[i] = *sp;
            break;
        }

        lastch = *sp;
        i++;

        if (dots > 1)
        {
            break;
        }
    }

    op[i] = '\0';
}

/************************************************************************/

void PromiseLog(char *s)
{
    char filename[CF_BUFSIZE];
    time_t now = time(NULL);
    FILE *fout;

    if (s == NULL || strlen(s) == 0)
    {
        return;
    }

    snprintf(filename, CF_BUFSIZE, "%s/%s", CFWORKDIR, CF_PROMISE_LOG);
    MapName(filename);

    if ((fout = fopen(filename, "a")) == NULL)
    {
        CfOut(cf_error, "fopen", "Could not open %s", filename);
        return;
    }

    fprintf(fout, "%jd,%jd: %s\n", (intmax_t) CFSTARTTIME, (intmax_t) now, s);
    fclose(fout);
}

/************************************************************************/

void FatalError(char *s, ...)
{
    CfLock best_guess;

    if (s)
    {
        va_list ap;
        char buf[CF_BUFSIZE] = "";

        va_start(ap, s);
        vsnprintf(buf, CF_BUFSIZE - 1, s, ap);
        va_end(ap);
        CfOut(cf_error, "", "Fatal CFEngine error: %s", buf);
    }

    if (strlen(CFLOCK) > 0)
    {
        best_guess.lock = xstrdup(CFLOCK);
        best_guess.last = xstrdup(CFLAST);
        best_guess.log = xstrdup(CFLOG);
        YieldCurrentLock(best_guess);
    }

    unlink(PIDFILE);
    EndAudit();
    GenericDeInitialize();
    exit(1);
}

/*****************************************************************************/

void AuditStatusMessage(FILE *fp, char status)
{
    switch (status)             /* Reminder */
    {
    case CF_CHG:
        fprintf(fp, "made a system correction");
        break;

    case CF_WARN:
        fprintf(fp, "promise not kept, no action taken");
        break;

    case CF_TIMEX:
        fprintf(fp, "timed out");
        break;

    case CF_FAIL:
        fprintf(fp, "failed to make a correction");
        break;

    case CF_DENIED:
        fprintf(fp, "was denied access to an essential resource");
        break;

    case CF_INTERPT:
        fprintf(fp, "was interrupted\n");
        break;

    case CF_NOP:
        fprintf(fp, "was applied but performed no required actions");
        break;

    case CF_UNKNOWN:
        fprintf(fp, "was applied but status unknown");
        break;

    case CF_REPORT:
        fprintf(fp, "report");
        break;
    }

}
