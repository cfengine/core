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

#include "cf3.defs.h"

#include "promises.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "vars.h"
#include "conversion.h"
#include "instrumentation.h"
#include "attributes.h"
#include "cfstream.h"
#include "pipes.h"

static int ExecSanityChecks(Attributes a, Promise *pp);
static void PreviewProtocolLine(char *line, char *comm);
static void VerifyExec(Attributes a, Promise *pp);

/*****************************************************************************/

void VerifyExecPromise(Promise *pp)
{
    Attributes a = { {0} };

    a = GetExecAttributes(pp);
    ExecSanityChecks(a, pp);
    VerifyExec(a, pp);
    DeleteScalar("this", "promiser");
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static int ExecSanityChecks(Attributes a, Promise *pp)
{
    if ((a.contain.nooutput) && (a.contain.preview))
    {
        CfOut(cf_error, "", "no_output and preview are mutually exclusive (broken promise)");
        PromiseRef(cf_error, pp);
        return false;
    }

#ifdef MINGW
    if (a.contain.umask != CF_UNDEFINED)        // TODO: Always true (077 != -1?, compare positive and negative number), make false when umask not set
    {
        CfOut(cf_verbose, "", "contain.umask is ignored on Windows");
    }

    if (a.contain.owner != CF_UNDEFINED)
    {
        CfOut(cf_verbose, "", "contain.exec_owner is ignored on Windows");
    }

    if (a.contain.group != CF_UNDEFINED)
    {
        CfOut(cf_verbose, "", "contain.exec_group is ignored on Windows");
    }

    if (a.contain.chroot != NULL)
    {
        CfOut(cf_verbose, "", "contain.chroot is ignored on Windows");
    }

#else /* NOT MINGW */
    if (a.contain.umask == CF_UNDEFINED)
    {
        a.contain.umask = 077;
    }
#endif /* NOT MINGW */

    return true;
}

/*****************************************************************************/

static void VerifyExec(Attributes a, Promise *pp)
{
    CfLock thislock;
    char line[CF_BUFSIZE], eventname[CF_BUFSIZE];
    char comm[20];
    char execstr[CF_EXPANDSIZE];
    int outsourced, count = 0;
#if !defined(__MINGW32__)
    mode_t maskval = 0;
#endif
    FILE *pfp;
    char cmdOutBuf[CF_BUFSIZE];
    int cmdOutBufPos = 0;
    int lineOutLen;

    if (!IsExecutable(GetArg0(pp->promiser)))
    {
        cfPS(cf_error, CF_FAIL, "", pp, a, "%s promises to be executable but isn't\n", pp->promiser);

        if (strchr(pp->promiser, ' '))
        {
            CfOut(cf_verbose, "", "Paths with spaces must be inside escaped quoutes (e.g. \\\"%s\\\")", pp->promiser);
        }

        return;
    }
    else
    {
        CfOut(cf_verbose, "", " -> Promiser string contains a valid executable (%s) - ok\n", GetArg0(pp->promiser));
    }

    DeleteScalar("this", "promiser");
    NewScalar("this", "promiser", pp->promiser, cf_str);

    if (a.args)
    {
        snprintf(execstr, CF_EXPANDSIZE - 1, "%s %s", pp->promiser, a.args);
    }
    else
    {
        strncpy(execstr, pp->promiser, CF_BUFSIZE);
    }

    thislock = AcquireLock(execstr, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

    PromiseBanner(pp);

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


    CfOut(cf_inform, "", " -> Executing \'%s\' ... (%s%s%s)\n", execstr,
          timeout_str, owner_str, group_str);

    BeginMeasure();

    if (DONTDO && (!a.contain.preview))
    {
        CfOut(cf_error, "", "-> Would execute script %s\n", execstr);
    }
    else if (a.transaction.action != cfa_fix)
    {
        cfPS(cf_error, CF_WARN, "", pp, a, " !! Command \"%s\" needs to be executed, but only warning was promised",
             execstr);
    }
    else
    {
        CommPrefix(execstr, comm);

        if (a.transaction.background)
        {
#ifdef MINGW
            outsourced = true;
#else
            CfOut(cf_verbose, "", " -> Backgrounding job %s\n", execstr);
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

#ifndef MINGW
            CfOut(cf_verbose, "", " -> (Setting umask to %jo)\n", (uintmax_t)a.contain.umask);
            maskval = umask(a.contain.umask);

            if (a.contain.umask == 0)
            {
                CfOut(cf_verbose, "", " !! Programming %s running with umask 0! Use umask= to set\n", execstr);
            }
#endif /* NOT MINGW */

            if (a.contain.useshell)
            {
                pfp =
                    cf_popen_shsetuid(execstr, "r", a.contain.owner, a.contain.group, a.contain.chdir, a.contain.chroot,
                                      a.transaction.background);
            }
            else
            {
                pfp =
                    cf_popensetuid(execstr, "r", a.contain.owner, a.contain.group, a.contain.chdir, a.contain.chroot,
                                   a.transaction.background);
            }

            if (pfp == NULL)
            {
                cfPS(cf_error, CF_FAIL, "cf_popen", pp, a, "!! Couldn't open pipe to command %s\n", execstr);
                YieldCurrentLock(thislock);
                return;
            }

            while (!feof(pfp))
            {
                if (ferror(pfp))        /* abortable */
                {
                    cfPS(cf_error, CF_TIMEX, "ferror", pp, a, "!! Command pipe %s\n", execstr);
                    cf_pclose(pfp);
                    YieldCurrentLock(thislock);
                    return;
                }

                CfReadLine(line, CF_BUFSIZE - 1, pfp);

                if (strstr(line, "cfengine-die"))
                {
                    break;
                }

                if (ferror(pfp))        /* abortable */
                {
                    cfPS(cf_error, CF_TIMEX, "ferror", pp, a, "!! Command pipe %s\n", execstr);
                    cf_pclose(pfp);
                    YieldCurrentLock(thislock);
                    return;
                }

                if (a.contain.preview)
                {
                    PreviewProtocolLine(line, execstr);
                }

                if (a.module)
                {
                    ModuleProtocol(execstr, line, !a.contain.nooutput, pp->namespace);
                }
                else if ((!a.contain.nooutput) && (NonEmptyLine(line)))
                {
                    lineOutLen = strlen(comm) + strlen(line) + 12;

                    // if buffer is to small for this line, output it directly
                    if (lineOutLen > sizeof(cmdOutBuf))
                    {
                        CfOut(cf_cmdout, "", "Q: \"...%s\": %s\n", comm, line);
                    }
                    else
                    {
                        if (cmdOutBufPos + lineOutLen > sizeof(cmdOutBuf))
                        {
                            CfOut(cf_cmdout, "", "%s", cmdOutBuf);
                            cmdOutBufPos = 0;
                        }
                        sprintf(cmdOutBuf + cmdOutBufPos, "Q: \"...%s\": %s\n", comm, line);
                        cmdOutBufPos += (lineOutLen - 1);
                    }
                    count++;
                }
            }
#ifdef MINGW
            if (outsourced)     // only get return value if we waited for command execution
            {
                cf_pclose(pfp);
            }
            else
            {
                cf_pclose_def(pfp, a, pp);
            }
#else /* NOT MINGW */
            cf_pclose_def(pfp, a, pp);
#endif
        }

        if (count)
        {
            if (cmdOutBufPos)
            {
                CfOut(cf_cmdout, "", "%s", cmdOutBuf);
            }

            CfOut(cf_cmdout, "", "I: Last %d quoted lines were generated by promiser \"%s\"\n", count, execstr);
        }

        if (a.contain.timeout != CF_NOINT)
        {
            alarm(0);
            signal(SIGALRM, SIG_DFL);
        }

        CfOut(cf_inform, "", " -> Completed execution of %s\n", execstr);
#ifndef MINGW
        umask(maskval);
#endif
        YieldCurrentLock(thislock);

        snprintf(eventname, CF_BUFSIZE - 1, "Exec(%s)", execstr);

#ifndef MINGW
        if ((a.transaction.background) && outsourced)
        {
            CfOut(cf_verbose, "", " -> Backgrounded command (%s) is done - exiting\n", execstr);
            exit(0);
        }
#endif /* NOT MINGW */
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

    CfOut(cf_verbose, "", "%s (preview of %s)\n", message, comm);
}
