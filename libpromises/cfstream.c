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

#include "cfstream.h"

#include "files_names.h"
#include "policy.h"
#include "item_lib.h"
#include "scope.h"
#include "mutex.h"
#include "logging.h"
#include "string_lib.h"
#include "misc_lib.h"
#include "rlist.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

#include <stdarg.h>

static void LogListStdout(const Item *messages, bool has_prefix);

#if !defined(__MINGW32__)
static void SystemLog(Item *mess, OutputLevel level);
static void LogPromiseResult(char *promiser, char peeType, void *promisee, char status, OutputLevel log_level,
                             Item *mess);
#endif

#if !defined(__MINGW32__)
static const char *GetErrorStr(void);
#endif

/*****************************************************************************/

void ReportToFile(const char *logfile, const char *message)
{
    FILE *fp = fopen(logfile, "a");
    if (fp == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Could not open log file %s\n", logfile);
        printf("%s\n", message);
    }
    else
    {
        fprintf(fp, "%s\n", message);
        fclose(fp);
    }
}

static void VLog(OutputLevel level, const char *errstr, const char *fmt, va_list args)
{
    char buffer[CF_BUFSIZE], output[CF_BUFSIZE];
    Item *mess = NULL;

    if ((fmt == NULL) || (strlen(fmt) == 0))
    {
        return;
    }

    memset(output, 0, CF_BUFSIZE);
    vsnprintf(buffer, CF_BUFSIZE - 1, fmt, args);

    if (Chop(buffer, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }

    AppendItem(&mess, buffer, NULL);

    if ((errstr == NULL) || (strlen(errstr) > 0))
    {
        snprintf(output, CF_BUFSIZE - 1, " !!! System reports error for %s: \"%s\"", errstr, GetErrorStr());
        AppendItem(&mess, output, NULL);
    }

    switch (level)
    {
    case OUTPUT_LEVEL_INFORM:

        if (INFORM || VERBOSE || DEBUG)
        {
            LogListStdout(mess, VERBOSE);
        }
        break;

    case OUTPUT_LEVEL_VERBOSE:

        if (VERBOSE || DEBUG)
        {
            LogListStdout(mess, VERBOSE);
        }
        break;

    case OUTPUT_LEVEL_ERROR:
    case OUTPUT_LEVEL_REPORTING:
    case OUTPUT_LEVEL_CMDOUT:

        LogListStdout(mess, VERBOSE);
        SystemLog(mess, level);
        break;

    case OUTPUT_LEVEL_LOG:

        if (VERBOSE || DEBUG)
        {
            LogListStdout(mess, VERBOSE);
        }
        SystemLog(mess, OUTPUT_LEVEL_VERBOSE);
        break;

    default:

        ProgrammingError("Report level unknown");
        break;
    }

    DeleteItemList(mess);
}

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VLog(level, errstr, fmt, ap);
    va_end(ap);
}

static void AmendErrorMessageWithPromiseInformation(Item **error_message, const Promise *pp)
{
    Rval retval;
    char *v;
    if (ScopeControlCommonGet(COMMON_CONTROL_VERSION, &retval) != DATA_TYPE_NONE)
    {
        v = (char *) retval.item;
    }
    else
    {
        v = "not specified";
    }

    const char *sp;
    char handle[CF_MAXVARSIZE];
    if ((sp = PromiseGetHandle(pp)) || (sp = PromiseID(pp)))
    {
        strncpy(handle, sp, CF_MAXVARSIZE - 1);
    }
    else
    {
        strcpy(handle, "(unknown)");
    }

    char output[CF_BUFSIZE];
    if (INFORM || VERBOSE || DEBUG)
    {
        snprintf(output, CF_BUFSIZE - 1, "I: Report relates to a promise with handle \"%s\"", handle);
        AppendItem(error_message, output, NULL);
    }

    if (pp && PromiseGetBundle(pp)->source_path)
    {
        snprintf(output, CF_BUFSIZE - 1, "I: Made in version \'%s\' of \'%s\' near line %zu",
                 v, PromiseGetBundle(pp)->source_path, pp->offset.line);
    }
    else
    {
        snprintf(output, CF_BUFSIZE - 1, "I: Promise is made internally by cfengine");
    }

    AppendItem(error_message, output, NULL);

    if (pp != NULL)
    {
        switch (pp->promisee.type)
        {
        case RVAL_TYPE_SCALAR:
            snprintf(output, CF_BUFSIZE - 1, "I: The promise was made to: \'%s\'", (char *) pp->promisee.item);
            AppendItem(error_message, output, NULL);
            break;

        case RVAL_TYPE_LIST:
        {
            Writer *w = StringWriter();
            WriterWriteF(w, "I: The promise was made to (stakeholders): ");
            RlistWrite(w, pp->promisee.item);
            AppendItem(error_message, StringWriterClose(w), NULL);
            break;
        }
        default:
            break;
        }

        if (pp->ref)
        {
            snprintf(output, CF_BUFSIZE - 1, "I: Comment: %s\n", pp->ref);
            AppendItem(error_message, output, NULL);
        }
    }
}

void cfPS(EvalContext *ctx, OutputLevel level, char status, char *errstr, const Promise *pp, Attributes attr, char *fmt, ...)
{
    if ((fmt == NULL) || (strlen(fmt) == 0))
    {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    char buffer[CF_BUFSIZE];
    vsnprintf(buffer, CF_BUFSIZE - 1, fmt, ap);
    va_end(ap);

    if (Chop(buffer, CF_EXPANDSIZE) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
    }

    Item *mess = NULL;
    AppendItem(&mess, buffer, NULL);

    if ((errstr == NULL) || (strlen(errstr) > 0))
    {
        char output[CF_BUFSIZE];
        snprintf(output, CF_BUFSIZE - 1, " !!! System reports error for %s: \"%s\"", errstr, GetErrorStr());
        AppendItem(&mess, output, NULL);
    }

    if (level == OUTPUT_LEVEL_ERROR)
    {
        AmendErrorMessageWithPromiseInformation(&mess, pp);
    }

    int verbose = (attr.transaction.report_level == OUTPUT_LEVEL_VERBOSE) || VERBOSE;

    switch (level)
    {
    case OUTPUT_LEVEL_INFORM:

        if (INFORM || (attr.transaction.report_level == OUTPUT_LEVEL_INFORM)
            || VERBOSE || (attr.transaction.report_level == OUTPUT_LEVEL_VERBOSE)
            || DEBUG)
        {
            LogListStdout(mess, verbose);
        }

        if (attr.transaction.log_level == OUTPUT_LEVEL_INFORM)
        {
            SystemLog(mess, level);
        }
        break;

    case OUTPUT_LEVEL_VERBOSE:

        if (VERBOSE || (attr.transaction.log_level == OUTPUT_LEVEL_VERBOSE)
            || DEBUG)
        {
            LogListStdout(mess, verbose);
        }

        if (attr.transaction.log_level == OUTPUT_LEVEL_VERBOSE)
        {
            SystemLog(mess, level);
        }

        break;

    case OUTPUT_LEVEL_ERROR:

        LogListStdout(mess, verbose);

        if (attr.transaction.log_level == OUTPUT_LEVEL_ERROR)
        {
            SystemLog(mess, level);
        }
        break;

    case OUTPUT_LEVEL_NONE:
        break;

    default:
        ProgrammingError("Unexpected output level (%d) passed to cfPS", level);
    }

    if (pp != NULL)
    {
        LogPromiseResult(pp->promiser, pp->promisee.type, pp->promisee.item, status, attr.transaction.log_level, mess);
    }

/* Now complete the exits status classes and auditing */

    if (pp != NULL)
    {
        ClassAuditLog(ctx, pp, attr, status);
        UpdatePromiseComplianceStatus(status, pp, buffer);
    }

    DeleteItemList(mess);
}

/*********************************************************************************/

static void LogListStdout(const Item *mess, bool has_prefix)
{
    for (const Item *ip = mess; ip != NULL; ip = ip->next)
    {
        if (has_prefix)
        {
            printf("%s> %s\n", VPREFIX, ip->name);
        }
        else
        {
            printf("%s\n", ip->name);
        }
    }
}

#if !defined(__MINGW32__)

static void SystemLog(Item *mess, OutputLevel level)
{
    Item *ip;

    if ((!IsPrivileged()) || DONTDO)
    {
        return;
    }

/* If we can't mutex it could be dangerous to proceed with threaded file descriptors */

    if (!ThreadLock(cft_output))
    {
        return;
    }

    for (ip = mess; ip != NULL; ip = ip->next)
    {
        switch (level)
        {
        case OUTPUT_LEVEL_INFORM:
        case OUTPUT_LEVEL_REPORTING:
        case OUTPUT_LEVEL_CMDOUT:
            syslog(LOG_NOTICE, " %s", ip->name);
            break;

        case OUTPUT_LEVEL_VERBOSE:
            syslog(LOG_INFO, " %s", ip->name);
            break;

        case OUTPUT_LEVEL_ERROR:
            syslog(LOG_ERR, " %s", ip->name);
            break;

        default:
            break;
        }
    }

    ThreadUnlock(cft_output);
}

static void LogPromiseResult(ARG_UNUSED char *promiser, ARG_UNUSED char peeType,
                             ARG_UNUSED void *promisee, ARG_UNUSED char status,
                             ARG_UNUSED OutputLevel log_level, ARG_UNUSED Item *mess)
{
}

#endif

#if !defined(__MINGW32__)
static const char *GetErrorStr(void)
{
    return strerror(errno);
}
#endif
