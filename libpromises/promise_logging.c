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

#include "promise_logging.h"

#include "logging.h"
#include "logging_priv.h"
#include "misc_lib.h"
#include "string_lib.h"
#include "env_context.h"

#include <assert.h>

typedef struct
{
    const EvalContext *eval_context;
    int promise_level;

    char *stack_path;
    char *last_message;
} PromiseLoggingContext;

static LogLevel AdjustLogLevel(LogLevel base, LogLevel adjust)
{
    if (adjust == -1)
    {
        return base;
    }
    else
    {
        return MAX(base, adjust);
    }
}

static LogLevel StringToLogLevel(const char *value)
{
    if (value)
    {
        if (!strcmp(value, "verbose"))
        {
            return LOG_LEVEL_VERBOSE;
        }
        if (!strcmp(value, "inform"))
        {
            return LOG_LEVEL_INFO;
        }
        if (!strcmp(value, "error"))
        {
            return LOG_LEVEL_NOTICE; /* Error level includes warnings and notices */
        }
    }
    return -1;
}

static LogLevel GetLevelForPromise(const EvalContext *ctx, const Promise *pp, const char *attr_name)
{
    return StringToLogLevel(ConstraintGetRvalValue(ctx, attr_name, pp, RVAL_TYPE_SCALAR));
}

static LogLevel CalculateLogLevel(const EvalContext *ctx, const Promise *pp)
{
    LogLevel log_level = LogGetGlobalLevel();

    if (pp)
    {
        log_level = AdjustLogLevel(log_level, GetLevelForPromise(ctx, pp, "log_level"));
    }

    /* Disable system log for dry-runs and for non-root agent */
    /* FIXME: do we really need it? */
    if (!IsPrivileged() || DONTDO)
    {
        log_level = -1;
    }

    return log_level;
}

static LogLevel CalculateReportLevel(const EvalContext *ctx, const Promise *pp)
{
    LogLevel report_level = LogGetGlobalLevel();

    if (pp)
    {
        report_level = AdjustLogLevel(report_level, GetLevelForPromise(ctx, pp, "report_level"));
    }

    return report_level;
}

static const char *LogHook(LoggingPrivContext *pctx, const char *message)
{
    PromiseLoggingContext *plctx = pctx->param;

    if (plctx->promise_level)
    {
        free(plctx->last_message);
        if (LEGACY_OUTPUT)
        {
            plctx->last_message = xstrdup(message);
        }
        else
        {
            plctx->last_message = StringConcatenate(3, plctx->stack_path, ": ", message);
        }

        return plctx->last_message;
    }
    else
    {
        return message;
    }
}

void PromiseLoggingInit(const EvalContext *eval_context)
{
    LoggingPrivContext *pctx = LoggingPrivGetContext();

    if (pctx != NULL)
    {
        ProgrammingError("Promise logging: Still bound to another EvalContext");
    }

    PromiseLoggingContext *plctx = xcalloc(1, sizeof(PromiseLoggingContext));
    plctx->eval_context = eval_context;

    pctx = xcalloc(1, sizeof(LoggingPrivContext));
    pctx->param = plctx;
    pctx->log_hook = &LogHook;

    LoggingPrivSetContext(pctx);
}

void PromiseLoggingPromiseEnter(const EvalContext *eval_context, const Promise *pp)
{
    LoggingPrivContext *pctx = LoggingPrivGetContext();

    if (pctx == NULL)
    {
        ProgrammingError("Promise logging: Unable to enter promise, not bound to EvalContext");
    }

    PromiseLoggingContext *plctx = pctx->param;

    if (plctx->eval_context != eval_context)
    {
        ProgrammingError("Promise logging: Unable to enter promise, bound to EvalContext different from passed one");
    }

    if (EvalContextStackGetTopPromise(eval_context) != pp)
    {
        /*
         * FIXME: There are still cases where promise passed here is not on top of stack
         */
        /* ProgrammingError("Logging: Attempt to set promise not on top of stack as current"); */
    }

    plctx->promise_level++;
    plctx->stack_path = EvalContextStackPath(eval_context);

    LoggingPrivSetLevels(CalculateLogLevel(eval_context, pp), CalculateReportLevel(eval_context, pp));
}

char *PromiseLoggingPromiseFinish(const EvalContext *eval_context, const Promise *pp)
{
    LoggingPrivContext *pctx = LoggingPrivGetContext();

    if (pctx == NULL)
    {
        ProgrammingError("Promise logging: Unable to finish promise, not bound to EvalContext");
    }

    PromiseLoggingContext *plctx = pctx->param;

    if (plctx->eval_context != eval_context)
    {
        ProgrammingError("Promise logging: Unable to finish promise, bound to EvalContext different from passed one");
    }

    if (EvalContextStackGetTopPromise(eval_context) != pp)
    {
        /*
         * FIXME: There are still cases where promise passed here is not on top of stack
         */
        /* ProgrammingError("Logging: Attempt to finish promise not on top of stack"); */
    }

    char *last_message = plctx->last_message;

    plctx->promise_level--;
    plctx->last_message = NULL;
    free(plctx->stack_path);

    LoggingPrivSetLevels(LogGetGlobalLevel(), LogGetGlobalLevel());

    return last_message;
}

void PromiseLoggingFinish(const EvalContext *eval_context)
{
    LoggingPrivContext *pctx = LoggingPrivGetContext();

    if (pctx == NULL)
    {
        ProgrammingError("Promise logging: Unable to finish, PromiseLoggingInit was not called before");
    }

    PromiseLoggingContext *plctx = pctx->param;

    if (plctx->eval_context != eval_context)
    {
        ProgrammingError("Promise logging: Unable to finish, passed EvalContext does not correspond to current one");
    }

    if (plctx->promise_level > 0)
    {
        ProgrammingError("Promise logging: Unable to finish, promise is still active");
    }

    assert(plctx->last_message == NULL);

    LoggingPrivSetContext(NULL);

    free(plctx);
    free(pctx);
}
