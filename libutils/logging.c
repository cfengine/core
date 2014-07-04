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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <logging.h>
#include <logging_priv.h>

#include <alloc.h>
#include <string_lib.h>
#include <misc_lib.h>

char VPREFIX[1024] = ""; /* GLOBAL_C */
bool LEGACY_OUTPUT = false; /* GLOBAL_A */

typedef struct
{
    LogLevel log_level;
    LogLevel report_level;
    bool color;

    LoggingPrivContext *pctx;
} LoggingContext;

static LogLevel global_level = LOG_LEVEL_NOTICE; /* GLOBAL_X */

static pthread_once_t log_context_init_once = PTHREAD_ONCE_INIT; /* GLOBAL_T */
static pthread_key_t log_context_key; /* GLOBAL_T, initialized by pthread_key_create */

static void LoggingInitializeOnce(void)
{
    if (pthread_key_create(&log_context_key, &free) != 0)
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
        lctx->log_level = global_level;
        lctx->report_level = global_level;
        pthread_setspecific(log_context_key, lctx);
    }
    return lctx;
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

const char *LogLevelToString(LogLevel level)
{
    switch (level)
    {
    case LOG_LEVEL_CRIT:
        return "critical";
    case LOG_LEVEL_ERR:
        return "error";
    case LOG_LEVEL_WARNING:
        return "warning";
    case LOG_LEVEL_NOTICE:
        return "notice";
    case LOG_LEVEL_INFO:
        return "info";
    case LOG_LEVEL_VERBOSE:
        return "verbose";
    case LOG_LEVEL_DEBUG:
        return "debug";
    default:
        ProgrammingError("LogLevelToString: Unexpected log level %d", level);
    }
}

static const char *LogLevelToColor(LogLevel level)
{

    switch (level)
    {
    case LOG_LEVEL_CRIT:
    case LOG_LEVEL_ERR:
        return "\x1b[31m"; // red

    case LOG_LEVEL_WARNING:
        return "\x1b[33m"; // yellow

    case LOG_LEVEL_NOTICE:
    case LOG_LEVEL_INFO:
        return "\x1b[32m"; // green

    case LOG_LEVEL_VERBOSE:
    case LOG_LEVEL_DEBUG:
        return "\x1b[34m"; // blue

    default:
        ProgrammingError("LogLevelToColor: Unexpected log level %d", level);
    }
}

bool LoggingFormatTimestamp(char dest[64], size_t n, struct tm *timestamp)
{
    if (strftime(dest, n, "%Y-%m-%dT%H:%M:%S%z", timestamp) == 0)
    {
        strlcpy(dest, "<unknown>", n);
        return false;
    }
    return true;
}

static void LogToConsole(const char *msg, LogLevel level, bool color)
{
    FILE *output_file = (level <= LOG_LEVEL_WARNING) ? stderr : stdout;

    if (LEGACY_OUTPUT)
    {
        if (level >= LOG_LEVEL_VERBOSE)
        {
            fprintf(stdout, "%s> %s\n", VPREFIX, msg);
        }
        else
        {
            fprintf(stdout, "%s\n", msg);
        }
    }
    else
    {
        struct tm now;
        time_t now_seconds = time(NULL);
        localtime_r(&now_seconds, &now);

        char formatted_timestamp[64];
        LoggingFormatTimestamp(formatted_timestamp, 64, &now);

        const char *string_level = LogLevelToString(level);

        if (color)
        {
            fprintf(output_file, "%s%s %8s: %s\x1b[0m\n", LogLevelToColor(level),
                    formatted_timestamp, string_level, msg);
        }
        else
        {
            fprintf(output_file, "%s %8s: %s\n", formatted_timestamp, string_level, msg);
        }
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
    default:
        ProgrammingError("LogLevelToSyslogPriority: Unexpected log level %d",
                         level);
    }

}

void LogToSystemLog(const char *msg, LogLevel level)
{
    syslog(LogLevelToSyslogPriority(level), "%s", msg);
}

const char *GetErrorStrFromCode(int error_code)
{
    return strerror(error_code);
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
    char *hooked_msg = NULL;

    if (lctx->pctx && lctx->pctx->log_hook)
    {
        hooked_msg = lctx->pctx->log_hook(lctx->pctx, level, msg);
    }
    else
    {
        hooked_msg = xstrdup(msg);
    }

    if (level <= lctx->report_level)
    {
        LogToConsole(hooked_msg, level, lctx->color);
    }

    if (level <= lctx->log_level)
    {
        LogToSystemLog(hooked_msg, level);
    }
    free(msg);
    free(hooked_msg);
}

/**
 * @brief Logs binary data in #buf, with each byte translated to '.' if not
 *        printable. Message is prefixed with #prefix.
 */
void LogRaw(LogLevel level, const char *prefix, const void *buf, size_t buflen)
{
    /* Translate non printable characters to printable ones. */
    const unsigned char *src = (const unsigned char *) buf;
    unsigned char dst[buflen+1];
    size_t i;

    for (i = 0; i < buflen; i++)
    {
        dst[i] = isprint(src[i]) ? src[i] : '.';
    }
    dst[i] = '\0';

    /* And Log the translated buffer, which is now a valid string. */
    Log(level, "%s%s", prefix, dst);
}

void Log(LogLevel level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VLog(level, fmt, ap);
    va_end(ap);
}

void LogSetGlobalLevel(LogLevel level)
{
    global_level = level;
    LoggingPrivSetLevels(level, level);
}

LogLevel LogGetGlobalLevel(void)
{
    return global_level;
}

void LoggingSetColor(bool enabled)
{
    LoggingContext *lctx = GetCurrentThreadContext();
    lctx->color = enabled;
}
