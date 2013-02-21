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

#include "verify_exec.h"

#include "promises.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "vars.h"
#include "conversion.h"
#include "instrumentation.h"
#include "attributes.h"
#include "cfstream.h"
#include "pipes.h"
#include "transaction.h"
#include "logging.h"
#include "evalfunction.h"
#include "exec_tools.h"
#include "misc_lib.h"
#include "writer.h"
#include "policy.h"

typedef enum
{
    ACTION_RESULT_OK,
    ACTION_RESULT_TIMEOUT,
    ACTION_RESULT_FAILED,
} ActionResult;

static bool SyntaxCheckExec(Attributes a, Promise *pp);
static bool PromiseKeptExec(Attributes a, Promise *pp);
static char *GetLockNameExec(Attributes a, Promise *pp);
static ActionResult RepairExec(Attributes a, Promise *pp);

static void PreviewProtocolLine(char *line, char *comm);

void VerifyExecPromise(Promise *pp)
{
    Attributes a = { {0} };

    a = GetExecAttributes(pp);

    NewScalar("this", "promiser", pp->promiser, DATA_TYPE_STRING);

    if (!SyntaxCheckExec(a, pp))
    {
        // cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "");
        DeleteScalar("this", "promiser");
        return;
    }

    if (PromiseKeptExec(a, pp))
    {
        // cfPS(OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, a, "");
        DeleteScalar("this", "promiser");
        return;
    }

    char *lock_name = GetLockNameExec(a, pp);
    CfLock thislock = AcquireLock(lock_name, VUQNAME, CFSTARTTIME, a, pp, false);
    free(lock_name);

    if (thislock.lock == NULL)
    {
        // cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, "");
        DeleteScalar("this", "promiser");
        return;
    }

    PromiseBanner(pp);

    switch (RepairExec(a, pp))
    {
    case ACTION_RESULT_OK:
        // cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, "");
        break;

    case ACTION_RESULT_TIMEOUT:
        // cfPS(OUTPUT_LEVEL_ERROR, CF_TIMEX, "", pp, a, "");
        break;

    case ACTION_RESULT_FAILED:
        // cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, "");
        break;

    default:
        ProgrammingError("Unexpected ActionResult value");
    }

    YieldCurrentLock(thislock);
    DeleteScalar("this", "promiser");
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static bool SyntaxCheckExec(Attributes a, Promise *pp)
{
    if ((a.contain.nooutput) && (a.contain.preview))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "no_output and preview are mutually exclusive (broken promise)");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return false;
    }

#ifdef __MINGW32__
    if (a.contain.umask != (mode_t)CF_UNDEFINED)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "contain.umask is ignored on Windows");
    }

    if (a.contain.owner != CF_UNDEFINED)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "contain.exec_owner is ignored on Windows");
    }

    if (a.contain.group != CF_UNDEFINED)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "contain.exec_group is ignored on Windows");
    }

    if (a.contain.chroot != NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "contain.chroot is ignored on Windows");
    }

#else /* !__MINGW32__ */
    if (a.contain.umask == (mode_t)CF_UNDEFINED)
    {
        a.contain.umask = 077;
    }
#endif /* !__MINGW32__ */

    return true;
}

static bool PromiseKeptExec(Attributes a, Promise *pp)
{
    return false;
}

static char *GetLockNameExec(Attributes a, Promise *pp)
{
    Writer *w = StringWriter();
    if (a.args)
    {
        WriterWriteF(w, "%s %s", pp->promiser, a.args);
    }
    else
    {
        WriterWrite(w, pp->promiser);
    }

    return StringWriterClose(w);
}

/*****************************************************************************/

static ActionResult RepairExec(Attributes a, Promise *pp)
{
    char line[CF_BUFSIZE], eventname[CF_BUFSIZE];
    char cmdline[CF_BUFSIZE];
    char comm[20];
    int outsourced, count = 0;
#if !defined(__MINGW32__)
    mode_t maskval = 0;
#endif
    FILE *pfp;
    char cmdOutBuf[CF_BUFSIZE];
    int cmdOutBufPos = 0;
    int lineOutLen;

    if (!IsExecutable(CommandArg0(pp->promiser)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "%s promises to be executable but isn't\n", pp->promiser);

        if (strchr(pp->promiser, ' '))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Paths with spaces must be inside escaped quoutes (e.g. \\\"%s\\\")", pp->promiser);
        }

        return ACTION_RESULT_FAILED;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Promiser string contains a valid executable (%s) - ok\n", CommandArg0(pp->promiser));
    }

    char timeout_str[CF_BUFSIZE];
    if (a.contain.timeout == CF_NOINT)
    {
        snprintf(timeout_str, CF_BUFSIZE, "no timeout");
    }
    else
    {
        snprintf(timeout_str, CF_BUFSIZE, "timeout=%ds", a.contain.timeout);
    }

    char owner_str[CF_BUFSIZE] = "";
    if (a.contain.owner != -1)
    {
        snprintf(owner_str, CF_BUFSIZE, ",uid=%ju", (uintmax_t)a.contain.owner);
    }

    char group_str[CF_BUFSIZE] = "";
    if (a.contain.group != -1)
    {
        snprintf(group_str, CF_BUFSIZE, ",gid=%ju", (uintmax_t)a.contain.group);
    }

    snprintf(cmdline, CF_BUFSIZE, "%s%s%s", pp->promiser, a.args ? " " : "", a.args ? a.args : "");

    CfOut(OUTPUT_LEVEL_INFORM, "", " -> Executing \'%s%s%s\' ... (%s)\n", timeout_str, owner_str, group_str, cmdline);

    BeginMeasure();

    if (DONTDO && (!a.contain.preview))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "-> Would execute script %s\n", cmdline);
        return ACTION_RESULT_OK;
    }
    else if (a.transaction.action != cfa_fix)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Command \"%s\" needs to be executed, but only warning was promised", cmdline);
        return ACTION_RESULT_OK;
    }
    else
    {
        CommandPrefix(cmdline, comm);

        if (a.transaction.background)
        {
#ifdef __MINGW32__
            outsourced = true;
#else
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Backgrounding job %s\n", cmdline);
            outsourced = fork();
#endif
        }
        else
        {
            outsourced = false;
        }

        if (outsourced || (!a.transaction.background))    // work done here: either by child or non-background parent
        {
            if (a.contain.timeout != CF_NOINT)
            {
                SetTimeOut(a.contain.timeout);
            }

#ifndef __MINGW32__
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> (Setting umask to %jo)\n", (uintmax_t)a.contain.umask);
            maskval = umask(a.contain.umask);

            if (a.contain.umask == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Programming %s running with umask 0! Use umask= to set\n", cmdline);
            }
#endif /* !__MINGW32__ */

            if (a.contain.useshell)
            {
                pfp =
                    cf_popen_shsetuid(cmdline, "r", a.contain.owner, a.contain.group, a.contain.chdir, a.contain.chroot,
                                      a.transaction.background);
            }
            else
            {
                pfp =
                    cf_popensetuid(cmdline, "r", a.contain.owner, a.contain.group, a.contain.chdir, a.contain.chroot,
                                   a.transaction.background);
            }

            if (pfp == NULL)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "cf_popen", "!! Couldn't open pipe to command %s\n", cmdline);
                return ACTION_RESULT_FAILED;
            }

            while (!feof(pfp))
            {
                if (ferror(pfp))        /* abortable */
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "ferror", "!! Command pipe %s\n", cmdline);
                    cf_pclose(pfp);
                    return ACTION_RESULT_TIMEOUT;
                }

                if (CfReadLine(line, CF_BUFSIZE - 1, pfp) == -1)
                {
                    FatalError("Error in CfReadLine");
                }

                if (strstr(line, "cfengine-die"))
                {
                    break;
                }

                if (ferror(pfp))        /* abortable */
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "ferror", "!! Command pipe %s\n", cmdline);
                    cf_pclose(pfp);
                    return ACTION_RESULT_TIMEOUT;
                }

                if (a.contain.preview)
                {
                    PreviewProtocolLine(line, cmdline);
                }

                if (a.module)
                {
                    ModuleProtocol(cmdline, line, !a.contain.nooutput, pp->ns);
                }
                else if ((!a.contain.nooutput) && (NonEmptyLine(line)))
                {
                    lineOutLen = strlen(comm) + strlen(line) + 12;

                    // if buffer is to small for this line, output it directly
                    if (lineOutLen > sizeof(cmdOutBuf))
                    {
                        CfOut(OUTPUT_LEVEL_CMDOUT, "", "Q: \"...%s\": %s\n", comm, line);
                    }
                    else
                    {
                        if (cmdOutBufPos + lineOutLen > sizeof(cmdOutBuf))
                        {
                            CfOut(OUTPUT_LEVEL_CMDOUT, "", "%s", cmdOutBuf);
                            cmdOutBufPos = 0;
                        }
                        sprintf(cmdOutBuf + cmdOutBufPos, "Q: \"...%s\": %s\n", comm, line);
                        cmdOutBufPos += (lineOutLen - 1);
                    }
                    count++;
                }
            }
#ifdef __MINGW32__
            if (outsourced)     // only get return value if we waited for command execution
            {
                cf_pclose(pfp);
            }
            else
            {
                cf_pclose_def(pfp, a, pp);
            }
#else /* !__MINGW32__ */
            cf_pclose_def(pfp, a, pp);
#endif
        }

        if (count)
        {
            if (cmdOutBufPos)
            {
                CfOut(OUTPUT_LEVEL_CMDOUT, "", "%s", cmdOutBuf);
            }

            CfOut(OUTPUT_LEVEL_CMDOUT, "", "I: Last %d quoted lines were generated by promiser \"%s\"\n", count, cmdline);
        }

        if (a.contain.timeout != CF_NOINT)
        {
            alarm(0);
            signal(SIGALRM, SIG_DFL);
        }

        CfOut(OUTPUT_LEVEL_INFORM, "", " -> Completed execution of %s\n", cmdline);
#ifndef __MINGW32__
        umask(maskval);
#endif

        snprintf(eventname, CF_BUFSIZE - 1, "Exec(%s)", cmdline);

#ifndef __MINGW32__
        if ((a.transaction.background) && outsourced)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Backgrounded command (%s) is done - exiting\n", cmdline);
            exit(0);
        }
#endif /* !__MINGW32__ */

        return ACTION_RESULT_OK;
    }
}

/*************************************************************/
/* Level                                                     */
/*************************************************************/

void PreviewProtocolLine(char *line, char *comm)
{
    int i;
    char *message = line;

    /*
     * Table matching cfoutputlevel enums to log prefixes.
     */

    char *prefixes[] =
{
        ":silent:",
        ":inform:",
        ":verbose:",
        ":editverbose:",
        ":error:",
        ":logonly:",
    };

    int precount = sizeof(prefixes) / sizeof(char *);

    if (line[0] == ':')
    {
        /*
         * Line begins with colon - see if it matches a log prefix.
         */

        for (i = 0; i < precount; i++)
        {
            int prelen = 0;

            prelen = strlen(prefixes[i]);

            if (strncmp(line, prefixes[i], prelen) == 0)
            {
                /*
                 * Found log prefix - set logging level, and remove the
                 * prefix from the log message.
                 */

                message += prelen;
                break;
            }
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "%s (preview of %s)\n", message, comm);
}
