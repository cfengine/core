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

#include "string_lib.h"
#include "item_lib.h"
#include "misc_lib.h"
#include "mutex.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

/*
 * Those functions are internal to logging, but they are still used by cfPS in
 * env_context.c, so we don't export them in headers, but keep non-static.
 */
void SystemLog(Item *mess, OutputLevel level);
void LogListStdout(const Item *messages, bool has_prefix);
const char *GetErrorStr(void);


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


/* Temporarily in use by env_context.c */

void LogListStdout(const Item *mess, bool has_prefix)
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
void SystemLog(Item *mess, OutputLevel level)
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

const char *GetErrorStr(void)
{
    return strerror(errno);
}
#endif
