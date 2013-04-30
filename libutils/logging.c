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
#include "logging_priv.h"

#include "alloc.h"
#include "string_lib.h"
#include "misc_lib.h"

/* TODO: get rid of */
int INFORM;
int VERBOSE;
int DEBUG;
char VPREFIX[1024];

void LogToSystemLog(const char *msg, LogLevel level);

typedef struct
{
    LogLevel log_level;
    LogLevel report_level;

    LoggingPrivContext *pctx;
} LoggingContext;

static pthread_once_t log_context_init_once;
static pthread_key_t log_context_key;

static void LoggingInitializeOnce(void)
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

static LoggingContext *GetCurrentThreadContext(void)
{
    pthread_once(&log_context_init_once, &LoggingInitializeOnce);
    LoggingContext *lctx = pthread_getspecific(log_context_key);
    if (lctx == NULL)
    {
        lctx = xcalloc(1, sizeof(LoggingContext));
        lctx->log_level = LoggingPrivGetGlobalLogLevel();
        lctx->report_level = LoggingPrivGetGlobalLogLevel();
        pthread_setspecific(log_context_key, lctx);
    }
    return lctx;
}

LogLevel LoggingPrivGetGlobalLogLevel(void)
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

void LoggingPrivSetContext(LoggingPrivContext *pctx)
{
    LoggingContext *lctx = GetCurrentThreadContext();
    lctx->pctx = pctx;
}

LoggingPrivContext *LoggingPrivGetContext(void)
{
    LoggingContext *lctx = GetCurrentThreadContext();
    return lctx->pctx;
}

void LoggingPrivSetLevels(LogLevel log_level, LogLevel report_level)
{
    LoggingContext *lctx = GetCurrentThreadContext();
    lctx->log_level = log_level;
    lctx->report_level = report_level;
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

    if (lctx->pctx && lctx->pctx->log_hook)
    {
        lctx->pctx->log_hook(lctx->pctx, msg);
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
