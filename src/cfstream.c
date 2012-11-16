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
#include "constraints.h"
#include "item_lib.h"
#include "unix.h"

#include <stdarg.h>

/*****************************************************************************/

/*
 * Log a list of strings into provided FILE
 */
static void LogList(FILE *fh, const Item *messages, bool has_prefix);

static void FileReport(const Item *mess, bool has_prefix, const char *filename);

#if !defined(__MINGW32__)
static void MakeLog(Item *mess, enum cfreport level);
static void LogPromiseResult(char *promiser, char peeType, void *promisee, char status, enum cfreport log_level,
                             Item *mess);
#endif

/*****************************************************************************/

/*
 * Common functionality of CfFOut and CfOut.
 */
static void VLog(FILE *fh, enum cfreport level, const char *errstr, const char *fmt, va_list args)
{
    char buffer[CF_BUFSIZE], output[CF_BUFSIZE];
    Item *mess = NULL;

    if ((fmt == NULL) || (strlen(fmt) == 0))
    {
        return;
    }

    memset(output, 0, CF_BUFSIZE);
    vsnprintf(buffer, CF_BUFSIZE - 1, fmt, args);
    Chop(buffer);
    AppendItem(&mess, buffer, NULL);

    if ((errstr == NULL) || (strlen(errstr) > 0))
    {
        snprintf(output, CF_BUFSIZE - 1, " !!! System reports error for %s: \"%s\"", errstr, GetErrorStr());
        AppendItem(&mess, output, NULL);
    }

    switch (level)
    {
    case cf_inform:

        if (INFORM || VERBOSE || DEBUG)
        {
            LogList(fh, mess, VERBOSE);
        }
        break;

    case cf_verbose:

        if (VERBOSE || DEBUG)
        {
            LogList(fh, mess, VERBOSE);
        }
        break;

    case cf_error:
    case cf_reporting:
    case cf_cmdout:

        LogList(fh, mess, VERBOSE);
        MakeLog(mess, level);
        break;

    case cf_log:

        if (VERBOSE || DEBUG)
        {
            LogList(fh, mess, VERBOSE);
        }
        MakeLog(mess, cf_verbose);
        break;

    default:

        FatalError("Report level unknown");
        break;
    }

    DeleteItemList(mess);
}

void CfFOut(char *filename, enum cfreport level, char *errstr, char *fmt, ...)
{
    FILE *fp = fopen(filename, "a");
    if (fp == NULL)
    {
        CfOut(cf_error, "fopen", "Could not open log file %s\n", filename);
        fp = stdout;
    }

    va_list ap;
    va_start(ap, fmt);

    VLog(fp, level, errstr, fmt, ap);

    va_end(ap);

    if (fp != stdout)
    {
        fclose(fp);
    }
}

void CfOut(enum cfreport level, const char *errstr, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VLog(stdout, level, errstr, fmt, ap);
    va_end(ap);
}

/*****************************************************************************/

void cfPS(enum cfreport level, char status, char *errstr, const Promise *pp, Attributes attr, char *fmt, ...)
{
    va_list ap;
    char buffer[CF_BUFSIZE], output[CF_BUFSIZE], *v, handle[CF_MAXVARSIZE];
    const char *sp;
    Item *ip, *mess = NULL;
    int verbose;
    Rval retval;

    if ((fmt == NULL) || (strlen(fmt) == 0))
    {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(buffer, CF_BUFSIZE - 1, fmt, ap);
    va_end(ap);
    Chop(buffer);
    AppendItem(&mess, buffer, NULL);

    if ((errstr == NULL) || (strlen(errstr) > 0))
    {
        snprintf(output, CF_BUFSIZE - 1, " !!! System reports error for %s: \"%s\"", errstr, GetErrorStr());
        AppendItem(&mess, output, NULL);
    }

    if (level == cf_error)
    {
        if (GetVariable("control_common", "version", &retval) != cf_notype)
        {
            v = (char *) retval.item;
        }
        else
        {
            v = "not specified";
        }

        if ((sp = GetConstraintValue("handle", pp, CF_SCALAR)) || (sp = PromiseID(pp)))
        {
            strncpy(handle, sp, CF_MAXVARSIZE - 1);
        }
        else
        {
            strcpy(handle, "(unknown)");
        }

        if (INFORM || VERBOSE || DEBUG)
        {
            snprintf(output, CF_BUFSIZE - 1, "I: Report relates to a promise with handle \"%s\"", handle);
            AppendItem(&mess, output, NULL);
        }

        if (pp && (pp->audit))
        {
            snprintf(output, CF_BUFSIZE - 1, "I: Made in version \'%s\' of \'%s\' near line %zu",
                     v, pp->audit->filename, pp->offset.line);
        }
        else
        {
            snprintf(output, CF_BUFSIZE - 1, "I: Promise is made internally by cfengine");
        }

        AppendItem(&mess, output, NULL);

        if (pp != NULL)
        {
            switch (pp->promisee.rtype)
            {
            case CF_SCALAR:
                snprintf(output, CF_BUFSIZE - 1, "I: The promise was made to: \'%s\'", (char *) pp->promisee.item);
                AppendItem(&mess, output, NULL);
                break;

            case CF_LIST:
                
                snprintf(output, CF_BUFSIZE - 1, "I: The promise was made to (stakeholders): ");
                PrintRlist(output+strlen(output), CF_BUFSIZE, (Rlist *)pp->promisee.item);
                AppendItem(&mess, output, NULL);
                break;
            }
            


            if (pp->ref)
            {
                snprintf(output, CF_BUFSIZE - 1, "I: Comment: %s\n", pp->ref);
                AppendItem(&mess, output, NULL);
            }
        }
    }

    verbose = (attr.transaction.report_level == cf_verbose) || VERBOSE;

    switch (level)
    {
    case cf_inform:

        if (INFORM || verbose || DEBUG || (attr.transaction.report_level == cf_inform))
        {
            LogList(stdout, mess, verbose);
        }

        if (attr.transaction.log_level == cf_inform)
        {
            MakeLog(mess, level);
        }
        break;

    case cf_reporting:
    case cf_cmdout:

        if (attr.report.to_file)
        {
            FileReport(mess, verbose, attr.report.to_file);
        }
        else
        {
            LogList(stdout, mess, verbose);
        }

        if (attr.transaction.log_level == cf_inform)
        {
            MakeLog(mess, level);
        }
        break;

    case cf_verbose:

        if (verbose || DEBUG)
        {
            LogList(stdout, mess, verbose);
        }

        if (attr.transaction.log_level == cf_verbose)
        {
            MakeLog(mess, level);
        }

        break;

    case cf_error:

        if (attr.report.to_file)
        {
            FileReport(mess, verbose, attr.report.to_file);
        }
        else
        {
            LogList(stdout, mess, verbose);
        }

        if (attr.transaction.log_level == cf_error)
        {
            MakeLog(mess, level);
        }
        break;

    case cf_log:

        MakeLog(mess, level);
        break;

    default:
        break;
    }

    if (pp != NULL)
    {
        LogPromiseResult(pp->promiser, pp->promisee.rtype, pp->promisee.item, status, attr.transaction.log_level, mess);
    }

/* Now complete the exits status classes and auditing */

    if (pp != NULL)
    {
        for (ip = mess; ip != NULL; ip = ip->next)
        {
            ClassAuditLog(pp, attr, ip->name, status, buffer);
        }
    }

    DeleteItemList(mess);
}

/*********************************************************************************/

void CfFile(FILE *fp, char *fmt, ...)
{
    va_list ap;
    char buffer[CF_BUFSIZE];

    if ((fmt == NULL) || (strlen(fmt) == 0))
    {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(buffer, CF_BUFSIZE - 1, fmt, ap);
    va_end(ap);

    if (!ThreadLock(cft_output))
    {
        return;
    }

    fprintf(fp, "%s> %s", VPREFIX, buffer);

    ThreadUnlock(cft_output);
}

/*********************************************************************************/
/* Level                                                                         */
/*********************************************************************************/

static void LogList(FILE *fh, const Item *mess, bool has_prefix)
{
    for (const Item *ip = mess; ip != NULL; ip = ip->next)
    {
        ThreadLock(cft_report);

        if (has_prefix)
        {
            fprintf(fh, "%s> %s\n", VPREFIX, ip->name);
        }
        else
        {
            fprintf(fh, "%s\n", ip->name);
        }

        ThreadUnlock(cft_report);
    }
}

static void FileReport(const Item *mess, bool has_prefix, const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "a")) == NULL)
    {
        CfOut(cf_error, "fopen", "Could not open log file %s\n", filename);
        fp = stdout;
    }

    LogList(fp, mess, has_prefix);

    if (fp != stdout)
    {
        fclose(fp);
    }
}

/*********************************************************************************/

#if !defined(__MINGW32__)

static void MakeLog(Item *mess, enum cfreport level)
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
        case cf_inform:
        case cf_reporting:
        case cf_cmdout:
            syslog(LOG_NOTICE, " %s", ip->name);
            break;

        case cf_verbose:
            syslog(LOG_INFO, " %s", ip->name);
            break;

        case cf_error:
            syslog(LOG_ERR, " %s", ip->name);
            break;

        default:
            break;
        }
    }

    ThreadUnlock(cft_output);
}

static void LogPromiseResult(char *promiser, char peeType, void *promisee, char status, enum cfreport log_level,
                             Item *mess)
{
}

#endif
