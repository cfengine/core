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

static char SYSLOG_HOST[CF_BUFSIZE] = "localhost";
static uint16_t SYSLOG_PORT = 514;

int FACILITY;

static void SummarizeTransaction(EvalContext *ctx, Attributes attr, const Promise *pp, const char *logname);

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

    {
        Rval track_value_rval = { 0 };
        bool track_value = false;
        if (ScopeGetVariable("control_agent", CFA_CONTROLBODY[AGENT_CONTROL_TRACK_VALUE].lval, &track_value_rval) != DATA_TYPE_NONE)
        {
            track_value = BooleanFromString(retval.item);
        }

        if (track_value)
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
    }

    double total = (double) (PR_KEPT + PR_NOTKEPT + PR_REPAIRED) / 100.0;

    if (ScopeGetVariable("control_common", "version", &retval) != DATA_TYPE_NONE)
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
                NewBundleClass(ctx, classname, THIS_BUNDLE, ns);
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

void ClassAuditLog(EvalContext *ctx, const Promise *pp, Attributes attr, char status, char *reason)
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

        AddAllClasses(ctx, PromiseGetNamespace(pp), attr.classes.change, attr.classes.persist, attr.classes.timer, attr.classes.scope);
        MarkPromiseHandleDone(ctx, pp);
        DeleteAllClasses(ctx, attr.classes.del_change);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, PROMISE_STATE_REPAIRED, reason);
            SummarizeTransaction(ctx, attr, pp, attr.transaction.log_repaired);
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
            NotePromiseCompliance(pp, PROMISE_STATE_NOTKEPT, reason);
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

        AddAllClasses(ctx, PromiseGetNamespace(pp), attr.classes.timeout, attr.classes.persist, attr.classes.timer, attr.classes.scope);
        DeleteAllClasses(ctx, attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(ctx, attr, pp, attr.transaction.log_failed);
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

        AddAllClasses(ctx, PromiseGetNamespace(pp), attr.classes.failure, attr.classes.persist, attr.classes.timer, attr.classes.scope);
        DeleteAllClasses(ctx, attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(ctx, attr, pp, attr.transaction.log_failed);
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

        AddAllClasses(ctx, PromiseGetNamespace(pp), attr.classes.denied, attr.classes.persist, attr.classes.timer, attr.classes.scope);
        DeleteAllClasses(ctx, attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(ctx, attr, pp, attr.transaction.log_failed);
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

        AddAllClasses(ctx, PromiseGetNamespace(pp), attr.classes.interrupt, attr.classes.persist, attr.classes.timer, attr.classes.scope);
        DeleteAllClasses(ctx, attr.classes.del_notkept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, PROMISE_STATE_NOTKEPT, reason);
            SummarizeTransaction(ctx, attr, pp, attr.transaction.log_failed);
        }
        break;

    case CF_UNKNOWN:
    case CF_NOP:

        AddAllClasses(ctx, PromiseGetNamespace(pp), attr.classes.kept, attr.classes.persist, attr.classes.timer, attr.classes.scope);
        DeleteAllClasses(ctx, attr.classes.del_kept);

        if (IsPromiseValuableForLogging(pp))
        {
            NotePromiseCompliance(pp, PROMISE_STATE_ANY, reason);
            SummarizeTransaction(ctx, attr, pp, attr.transaction.log_kept);
        }

        if (IsPromiseValuableForStatus(pp))
        {
            PR_KEPT++;
            VAL_KEPT += attr.transaction.value_kept;

#ifdef HAVE_NOVA
            EnterpriseTrackTotalCompliance(pp, 'c');
#endif
        }

        MarkPromiseHandleDone(ctx, pp);
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

static void SummarizeTransaction(EvalContext *ctx, Attributes attr, const Promise *pp, const char *logname)
{
    if (logname && (attr.transaction.log_string))
    {
        char buffer[CF_EXPANDSIZE];

        ExpandPrivateScalar(ScopeGetCurrent()->scope, attr.transaction.log_string, buffer);

        if (strcmp(logname, "udp_syslog") == 0)
        {
            RemoteSysLog(attr.transaction.log_priority, buffer);
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

        attr.transaction.log_string = NULL;     /* To avoid repetition */
    }
    else if (attr.transaction.log_failed)
    {
        if (logname && (strcmp(logname, attr.transaction.log_failed) == 0))
        {
            cfPS(ctx, OUTPUT_LEVEL_LOG, CF_NOP, "", pp, attr, "%s", attr.transaction.log_string);
        }
    }
}

void SetSyslogFacility(int facility)
{
    FACILITY = facility;
}

void SetSyslogHost(const char *host)
{
    strlcpy(SYSLOG_HOST, host, CF_BUFSIZE);
}

void SetSyslogPort(uint16_t port)
{
    SYSLOG_PORT = port;
}

void RemoteSysLog(int log_priority, const char *log_string)
{
    int sd, rfc3164_len = 1024;
    char message[CF_BUFSIZE];
    time_t now = time(NULL);
    int pri = log_priority | FACILITY;

#if defined(HAVE_GETADDRINFO)
    int err;
    struct addrinfo query, *response, *ap;
    char strport[CF_MAXVARSIZE];

    snprintf(strport, CF_MAXVARSIZE - 1, "%u", (unsigned) SYSLOG_PORT);
    memset(&query, 0, sizeof(struct addrinfo));
    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_DGRAM;

    if ((err = getaddrinfo(SYSLOG_HOST, strport, &query, &response)) != 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Unable to find syslog_host or service: (%s/%s) %s", SYSLOG_HOST, strport,
              gai_strerror(err));
        return;
    }

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Connect to syslog %s = %s on port %s\n", SYSLOG_HOST, sockaddr_ntop(ap->ai_addr),
              strport);

        if ((sd = socket(ap->ai_family, ap->ai_socktype, IPPROTO_UDP)) == -1)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "socket", "Couldn't open a socket");
            continue;
        }
        else
        {
            char timebuffer[26];

            snprintf(message, rfc3164_len, "<%u>%.15s %s %s", pri, cf_strtimestamp_local(now, timebuffer) + 4, VFQNAME,
                     log_string);
            if (sendto(sd, message, strlen(message), 0, ap->ai_addr, ap->ai_addrlen) == -1)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "sendto", " -> Couldn't send \"%s\" to syslog server \"%s\"\n", message, SYSLOG_HOST);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Syslog message: \"%s\" to server \"%s\"\n", message, SYSLOG_HOST);
            }
            close(sd);
            return;
        }
    }

#else
    struct sockaddr_in addr;
    char timebuffer[26];

    sockaddr_pton(AF_INET, SYSLOG_HOST, &addr);
    addr.sin_port = htons(SYSLOG_PORT);

    if ((sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "sendto", " !! Unable to send syslog datagram");
        return;
    }

    snprintf(message, rfc3164_len, "<%u>%.15s %s %s", pri, cf_strtimestamp_local(now, timebuffer) + 4, VFQNAME,
             log_string);

    if (sendto(sd, message, strlen(message), 0, (struct sockaddr *) &addr, sizeof(addr)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "sendto", " !! Unable to send syslog datagram");
        return;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Syslog message: \"%s\" to server %s\n", message, SYSLOG_HOST);
    close(sd);
#endif
}
