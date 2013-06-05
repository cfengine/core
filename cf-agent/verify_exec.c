/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commerical Open Source License
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
#include "pipes.h"
#include "locks.h"
#include "evalfunction.h"
#include "exec_tools.h"
#include "misc_lib.h"
#include "writer.h"
#include "policy.h"
#include "string_lib.h"
#include "scope.h"
#include "ornaments.h"
#include "env_context.h"
#include "retcode.h"

typedef enum
{
    ACTION_RESULT_OK,
    ACTION_RESULT_TIMEOUT,
    ACTION_RESULT_FAILED
} ActionResult;

static bool SyntaxCheckExec(Attributes a, Promise *pp);
static bool PromiseKeptExec(Attributes a, Promise *pp);
static char *GetLockNameExec(Attributes a, Promise *pp);
static ActionResult RepairExec(EvalContext *ctx, Attributes a, Promise *pp);

static void PreviewProtocolLine(char *line, char *comm);

void VerifyExecPromise(EvalContext *ctx, Promise *pp)
{
    Attributes a = { {0} };

    a = GetExecAttributes(ctx, pp);

    ScopeNewSpecial(ctx, "this", "promiser", pp->promiser, DATA_TYPE_STRING);

    if (!SyntaxCheckExec(a, pp))
    {
        // cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "");
        ScopeDeleteSpecial("this", "promiser");
        return;
    }

    if (PromiseKeptExec(a, pp))
    {
        // cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "");
        ScopeDeleteSpecial("this", "promiser");
        return;
    }

    char *lock_name = GetLockNameExec(a, pp);
    CfLock thislock = AcquireLock(ctx, lock_name, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    free(lock_name);

    if (thislock.lock == NULL)
    {
        // cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "");
        ScopeDeleteSpecial("this", "promiser");
        return;
    }

    PromiseBanner(pp);

    switch (RepairExec(ctx, a, pp))
    {
    case ACTION_RESULT_OK:
        // cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "");
        break;

    case ACTION_RESULT_TIMEOUT:
        // cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_TIMEOUT, pp, a, "");
        break;

    case ACTION_RESULT_FAILED:
        // cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "");
        break;

    default:
        ProgrammingError("Unexpected ActionResult value");
    }

    YieldCurrentLock(thislock);
    ScopeDeleteSpecial("this", "promiser");
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static bool SyntaxCheckExec(Attributes a, Promise *pp)
{
    if ((a.contain.nooutput) && (a.contain.preview))
    {
        Log(LOG_LEVEL_ERR, "no_output and preview are mutually exclusive (broken promise)");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

#ifdef __MINGW32__
    if (a.contain.umask != (mode_t)CF_UNDEFINED)
    {
        Log(LOG_LEVEL_VERBOSE, "contain.umask is ignored on Windows");
    }

    if (a.contain.owner != CF_UNDEFINED)
    {
        Log(LOG_LEVEL_VERBOSE, "contain.exec_owner is ignored on Windows");
    }

    if (a.contain.group != CF_UNDEFINED)
    {
        Log(LOG_LEVEL_VERBOSE, "contain.exec_group is ignored on Windows");
    }

    if (a.contain.chroot != NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "contain.chroot is ignored on Windows");
    }

#else /* !__MINGW32__ */
    if (a.contain.umask == (mode_t)CF_UNDEFINED)
    {
        a.contain.umask = 077;
    }
#endif /* !__MINGW32__ */

    return true;
}

static bool PromiseKeptExec(ARG_UNUSED Attributes a, ARG_UNUSED Promise *pp)
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

static ActionResult RepairExec(EvalContext *ctx, Attributes a, Promise *pp)
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

    if (IsAbsoluteFileName(CommandArg0(pp->promiser)) || a.contain.shelltype == SHELL_TYPE_NONE)
    {
        if (!IsExecutable(CommandArg0(pp->promiser)))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "'%s' promises to be executable but isn't", pp->promiser);

            if (strchr(pp->promiser, ' '))
            {
                Log(LOG_LEVEL_VERBOSE, "Paths with spaces must be inside escaped quoutes (e.g. \\\"%s\\\")", pp->promiser);
            }

            return ACTION_RESULT_FAILED;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Promiser string contains a valid executable '%s' - ok", CommandArg0(pp->promiser));
        }
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

    Log(LOG_LEVEL_INFO, "Executing '%s%s%s' ... '%s'", timeout_str, owner_str, group_str, cmdline);

    BeginMeasure();

    if (DONTDO && (!a.contain.preview))
    {
        Log(LOG_LEVEL_ERR, "Would execute script '%s'", cmdline);
        return ACTION_RESULT_OK;
    }

    if (a.transaction.action != cfa_fix)
    {
        Log(LOG_LEVEL_ERR, "Command '%s' needs to be executed, but only warning was promised", cmdline);
        return ACTION_RESULT_OK;
    }

    CommandPrefix(cmdline, comm);

    if (a.transaction.background)
    {
#ifdef __MINGW32__
        outsourced = true;
#else
        Log(LOG_LEVEL_VERBOSE, "Backgrounding job '%s'", cmdline);
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
        Log(LOG_LEVEL_VERBOSE, "(Setting umask to %jo)", (uintmax_t)a.contain.umask);
        maskval = umask(a.contain.umask);

        if (a.contain.umask == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Programming '%s' running with umask 0! Use umask= to set", cmdline);
        }
#endif /* !__MINGW32__ */

        if (a.contain.shelltype == SHELL_TYPE_POWERSHELL)
        {
#ifdef __MINGW32__
            pfp =
                cf_popen_powershell_setuid(cmdline, "r", a.contain.owner, a.contain.group, a.contain.chdir, a.contain.chroot,
                                  a.transaction.background);
#else // !__MINGW32__
            Log(LOG_LEVEL_ERR, "Powershell is only supported on Windows");
            return ACTION_RESULT_FAILED;
#endif // !__MINGW32__
        }
        else if (a.contain.shelltype == SHELL_TYPE_USE)
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
            Log(LOG_LEVEL_ERR, "Couldn't open pipe to command '%s'. (cf_popen: %s)", cmdline, GetErrorStr());
            return ACTION_RESULT_FAILED;
        }

        for (;;)
        {
            ssize_t res = CfReadLine(line, CF_BUFSIZE, pfp);

            if (res == 0)
            {
                break;
            }

            if (res == -1)
            {
                Log(LOG_LEVEL_ERR, "Unable to read output from command '%s'. (fread: %s)", cmdline, GetErrorStr());
                cf_pclose(pfp);
                return ACTION_RESULT_FAILED;
            }

            if (strstr(line, "cfengine-die"))
            {
                break;
            }

            if (a.contain.preview)
            {
                PreviewProtocolLine(line, cmdline);
            }

            if (a.module)
            {
                ModuleProtocol(ctx, cmdline, line, !a.contain.nooutput, PromiseGetNamespace(pp));
            }
            else if ((!a.contain.nooutput) && (!EmptyString(line)))
            {
                lineOutLen = strlen(comm) + strlen(line) + 12;

                // if buffer is to small for this line, output it directly
                if (lineOutLen > sizeof(cmdOutBuf))
                {
                    Log(LOG_LEVEL_NOTICE, "Q: '%s': %s", comm, line);
                }
                else
                {
                    if (cmdOutBufPos + lineOutLen > sizeof(cmdOutBuf))
                    {
                        Log(LOG_LEVEL_NOTICE, "%s", cmdOutBuf);
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
#endif /* __MINGW32__ */
        {
            int ret = cf_pclose(pfp);

            if (ret == -1)
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Finished script '%s' - failed (abnormal termination)", pp->promiser);
            }
            else
            {
                VerifyCommandRetcode(ctx, ret, true, a, pp);
            }
        }
    }

    if (count)
    {
        if (cmdOutBufPos)
        {
            Log(LOG_LEVEL_NOTICE, "%s", cmdOutBuf);
        }

        Log(LOG_LEVEL_INFO, "Last %d quoted lines were generated by promiser '%s'", count, cmdline);
    }

    if (a.contain.timeout != CF_NOINT)
    {
        alarm(0);
        signal(SIGALRM, SIG_DFL);
    }

    Log(LOG_LEVEL_INFO, "Completed execution of '%s'", cmdline);
#ifndef __MINGW32__
    umask(maskval);
#endif

    snprintf(eventname, CF_BUFSIZE - 1, "Exec(%s)", cmdline);

#ifndef __MINGW32__
    if ((a.transaction.background) && outsourced)
    {
        Log(LOG_LEVEL_VERBOSE, "Backgrounded command '%s' is done - exiting", cmdline);
        exit(0);
    }
#endif /* !__MINGW32__ */

    return ACTION_RESULT_OK;
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

    Log(LOG_LEVEL_VERBOSE, "'%s', preview of '%s'", message, comm);
}
