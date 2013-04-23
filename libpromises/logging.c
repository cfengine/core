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
#include "env_context.h"
#include "string_lib.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

static LogLevel OutputLevelToLogLevel(OutputLevel level)
{
    switch (level)
    {
    case OUTPUT_LEVEL_ERROR: return LOG_LEVEL_NOTICE; /* default level includes warnings and notices */
    case OUTPUT_LEVEL_VERBOSE: return LOG_LEVEL_VERBOSE;
    case OUTPUT_LEVEL_INFORM: return LOG_LEVEL_INFO;
    }

    ProgrammingError("Unknown output level passed to OutputLevelToLogLevel: %d", level);
}

void CfVOut(OutputLevel level, const char *errstr, const char *fmt, va_list ap)
{
    const char *GetErrorStr(void);

    if (strchr(fmt, '\n'))
    {
        char *fmtcopy = xstrdup(fmt);
        Chop(fmtcopy, strlen(fmtcopy));
        VLog(OutputLevelToLogLevel(level), fmtcopy, ap);
        free(fmtcopy);
    }
    else
    {
        VLog(OutputLevelToLogLevel(level), fmt, ap);
    }

    if (errstr && strlen(errstr) > 0)
    {
        Log(OutputLevelToLogLevel(level), " !!! System reports error for %s: \"%s\"", errstr, GetErrorStr());
    }
}

void CfOut(OutputLevel level, const char *errstr, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CfVOut(level, errstr, fmt, ap);
    va_end(ap);
}

typedef struct
{
    const EvalContext *ctx;

    /* For current promise */
    bool in_promise;
    LogLevel log_level;
    LogLevel report_level;
    char *last_message; /* FIXME: there might be a need for stack of last_messages for enclosed promises */

} LoggingContext;

static pthread_once_t log_context_init_once;
static pthread_key_t log_context_key;

static void LoggingInitialize(void)
{
    if (pthread_key_create(&log_context_key, NULL) != 0)
    {
        /* There is no way to signal error out of pthread_once callback.
         * However if pthread_key_create fails we are pretty much guaranteed
         * that nothing else will work. */

        fprintf(stderr, "Unable to initialize logging subsystem\n");
        exit(255);
    }
}

static LogLevel GetGlobalLogLevel(void)
{
    if (VERBOSE)
    {
        return LOG_LEVEL_VERBOSE;
    }

    if (INFORM)
    {
        return LOG_LEVEL_INFO;
    }

    return LOG_LEVEL_NOTICE;
}

static LoggingContext *GetCurrentThreadContext(void)
{
    pthread_once(&log_context_init_once, &LoggingInitialize);
    LoggingContext *lctx = pthread_getspecific(log_context_key);
    if (lctx == NULL)
    {
        lctx = xcalloc(1, sizeof(LoggingContext));
        lctx->log_level = GetGlobalLogLevel();
        lctx->report_level = GetGlobalLogLevel();
        pthread_setspecific(log_context_key, lctx);
    }
    return lctx;
}

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
    LogLevel log_level = GetGlobalLogLevel();

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
    LogLevel report_level = GetGlobalLogLevel();

    if (pp)
    {
        report_level = AdjustLogLevel(report_level, GetLevelForPromise(ctx, pp, "report_level"));
    }

    return report_level;
}

void LoggingInit(const EvalContext *ctx)
{
    LoggingContext *lctx = GetCurrentThreadContext();

    if (lctx->ctx != NULL)
    {
        ProgrammingError("Logging: Still bound to another EvalContext");
    }

    lctx->ctx = ctx;
}

void LoggingPromiseEnter(const EvalContext *ctx, const Promise *pp)
{
    LoggingContext *lctx = GetCurrentThreadContext();

    if (lctx->ctx != ctx)
    {
        ProgrammingError("Logging: Wrong EvalContext in setting current promise");
    }

    if (EvalContextStackGetTopPromise(ctx) != pp)
    {
        /*
         * FIXME: There are still cases where promise passed here is not on top of stack
         */
        /* ProgrammingError("Logging: Attempt to set promise not on top of stack as current"); */
    }

    lctx->in_promise = true;
    lctx->log_level = CalculateLogLevel(ctx, pp);
    lctx->report_level = CalculateReportLevel(ctx, pp);
}

char *LoggingPromiseFinish(const EvalContext *ctx, const Promise *pp)
{
    LoggingContext *lctx = GetCurrentThreadContext();

    if (lctx->ctx == NULL || lctx->ctx != ctx)
    {
        ProgrammingError("Logging: Wrong EvalContext in finishing promise");
    }

    if (EvalContextStackGetTopPromise(ctx) != pp)
    {
        /*
         * FIXME: There are still cases where promise passed here is not on top of stack
         */
        /* ProgrammingError("Logging: Attempt to finish promise not on top of stack"); */
    }

    char *last_message = lctx->last_message;

    lctx->in_promise = false;
    lctx->log_level = GetGlobalLogLevel();
    lctx->report_level = GetGlobalLogLevel();
    lctx->last_message = NULL;

    return last_message;
}

void LoggingFinish(const EvalContext *ctx)
{
    LoggingContext *lctx = GetCurrentThreadContext();

    if (lctx->ctx == NULL || lctx->ctx != ctx)
    {
        ProgrammingError("Logging: Unable to finish, passed EvalContext is wrong");
    }

    if (lctx->in_promise == true)
    {
        ProgrammingError("Logging: Unable to finish, promise is still active");
    }

    lctx->ctx = NULL;
}

/* static const char *LogLevelToString(LogLevel level) */
/* { */
/*     switch (level) */
/*     { */
/*     case LOG_LEVEL_CRIT: return "!"; */
/*     case LOG_LEVEL_ERR: return "E"; */
/*     case LOG_LEVEL_WARNING: return "W"; */
/*     case LOG_LEVEL_NOTICE: return "N"; */
/*     case LOG_LEVEL_INFO: return "I"; */
/*     case LOG_LEVEL_VERBOSE: return "V"; */
/*     case LOG_LEVEL_DEBUG: return "D"; */
/*     } */

/*     ProgrammingError("Unknown log level passed to LogLevelToString: %d", level); */
/* } */

void LogToStdout(const char *msg, ARG_UNUSED LogLevel level)
{
    /* TODO: timestamps, logging levels in error messages */

    /* struct tm now; */
    /* time_t now_seconds = time(NULL); */
    /* localtime_r(&now_seconds, &now); */

    /* /\* 2000-01-01 23:01:01+0300 *\/ */
    /* char formatted_timestamp[25]; */
    /* if (strftime(formatted_timestamp, 25, "%Y-%m-%d %H:%M:%S%z", &now) == 0) */
    /* { */
    /*     /\* There was some massacre formating the timestamp. Wow *\/ */
    /*     strlcpy(formatted_timestamp, "<unknown>", sizeof(formatted_timestamp)); */
    /* } */

    /* const char *string_level = LogLevelToString(level); */

    /* printf("%s> %-24s %1s: %s\n", VPREFIX, formatted_timestamp, string_level, msg); */



    /* FIXME: VPREFIX is only used in verbose mode. Is that ok? */
    if (level >= LOG_LEVEL_VERBOSE)
    {
        printf("%s> %s\n", VPREFIX, msg);
    }
    else
    {
        printf("%s\n", msg);
    }
}

#if !defined(__MINGW32__)
static int LogLevelToSyslogPriority(LogLevel level)
{
    switch (level)
    {
    case LOG_LEVEL_CRIT: return LOG_CRIT;
    case LOG_LEVEL_ERR: return LOG_ERR;
    case LOG_LEVEL_WARNING: return LOG_WARNING;
    case LOG_LEVEL_NOTICE: return LOG_NOTICE;
    case LOG_LEVEL_INFO: return LOG_INFO;
    case LOG_LEVEL_VERBOSE: return LOG_DEBUG; /* FIXME: Do we really want to conflate those levels? */
    case LOG_LEVEL_DEBUG: return LOG_DEBUG;
    }

    ProgrammingError("Unknown log level passed to LogLevelToSyslogPriority: %d", level);
}

void LogToSystemLog(const char *msg, LogLevel level)
{
    syslog(LogLevelToSyslogPriority(level), "%s", msg);
}

const char *GetErrorStr(void)
{
    return strerror(errno);
}
#endif

void VLog(LogLevel level, const char *fmt, va_list ap)
{
    LoggingContext *lctx = GetCurrentThreadContext();

    char *msg = StringVFormat(fmt, ap);

    if (lctx->in_promise)
    {
        free(lctx->last_message);
        lctx->last_message = msg;
    }

    if (level <= lctx->report_level)
    {
        LogToStdout(msg, level);
    }

    if (level <= lctx->log_level)
    {
        LogToSystemLog(msg, level);
    }
}

void Log(LogLevel level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VLog(level, fmt, ap);
    va_end(ap);
}
