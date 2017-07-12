/*
   Copyright 2017 Northern.tech AS

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
#include <alloc.h>
#include <string_lib.h>
#include <misc_lib.h>


char VPREFIX[1024] = ""; /* GLOBAL_C */

static char AgentType[80] = "generic";
static bool TIMESTAMPS = false;

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

LoggingContext *GetCurrentThreadContext(void)
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

void LoggingSetAgentType(const char *type)
{
    strlcpy(AgentType, type, sizeof(AgentType));
}

void LoggingEnableTimestamps(bool enable)
{
    TIMESTAMPS = enable;
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
        return "CRITICAL";
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
    FILE *output_file = stdout; // Messages should ALL go to stdout else they are disordered
    struct tm now;
    time_t now_seconds = time(NULL);
    localtime_r(&now_seconds, &now);

    if (color)
    {
        fprintf(output_file, "%s", LogLevelToColor(level));
    }
    if (level >= LOG_LEVEL_INFO && VPREFIX[0])
    {
        fprintf(stdout, "%s ", VPREFIX);
    }
    if (TIMESTAMPS)
    {
        char formatted_timestamp[64];
        LoggingFormatTimestamp(formatted_timestamp, 64, &now);
        fprintf(stdout, "%s ", formatted_timestamp);
    }

    fprintf(stdout, "%8s: %s\n", LogLevelToString(level), msg);

    if (color)
    {
        // Turn off the color again.
        fprintf(output_file, "\x1b[0m");
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
    char logmsg[4096];
    snprintf(logmsg, sizeof(logmsg), "CFEngine(%s) %s %s\n", AgentType, VPREFIX, msg);
    syslog(LogLevelToSyslogPriority(level), "%s", logmsg);
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

    bool log_to_console = ( level <= lctx->report_level );
    bool log_to_syslog  = ( level <= lctx->log_level &&
                            level < LOG_LEVEL_VERBOSE );
    bool force_hook     = ( lctx->pctx &&
                            lctx->pctx->log_hook &&
                            lctx->pctx->force_hook_level >= level );

    if (!log_to_console && !log_to_syslog && !force_hook)
    {
        return;                            /* early return - save resources */
    }

    char *msg = StringVFormat(fmt, ap);
    char *hooked_msg = NULL;

    /* Remove ending EOLN. */
    for (char *sp = msg; *sp != '\0'; sp++)
    {
        if (*sp == '\n' && *(sp+1) == '\0')
        {
            *sp = '\0';
            break;
        }
    }

    if (lctx->pctx && lctx->pctx->log_hook)
    {
        hooked_msg = lctx->pctx->log_hook(lctx->pctx, level, msg);
    }
    else
    {
        hooked_msg = msg;
    }

    if (log_to_console)
    {
        LogToConsole(hooked_msg, level, lctx->color);
    }
    if (log_to_syslog)
    {
        LogToSystemLog(hooked_msg, level);
    }

    if (hooked_msg != msg)
    {
        free(hooked_msg);
    }
    free(msg);
}

/* TODO create libutils/defs.h. */
#define CF_MAX_BUFSIZE 16384

/**
 * @brief Logs binary data in #buf, with unprintable bytes translated to '.'.
 *        Message is prefixed with #prefix.
 * @param #buflen must be no more than CF_MAX_BUFSIZE
 */
void LogRaw(LogLevel level, const char *prefix, const void *buf, size_t buflen)
{
    const unsigned char *src = buf;
    unsigned char dst[buflen+1];
    assert(sizeof(dst) <= CF_MAX_BUFSIZE);

    LoggingContext *lctx = GetCurrentThreadContext();
    if (level <= lctx->report_level || level <= lctx->log_level)
    {
        size_t i;
        for (i = 0; i < buflen; i++)
        {
            dst[i] = isprint(src[i]) ? src[i] : '.';
        }
        dst[i] = '\0';

        /* And Log the translated buffer, which is now a valid string. */
        Log(level, "%s%s", prefix, dst);
    }
}

void Log(LogLevel level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VLog(level, fmt, ap);
    va_end(ap);
}



static bool module_is_enabled[LOG_MOD_MAX];
static const char *log_modules[LOG_MOD_MAX] =
{
    "",
    "evalctx",
    "expand",
    "iterations",
    "parser",
    "vartable",
    "vars",
    "locks",
    "ps",
};

static enum LogModule LogModuleFromString(const char *s)
{
    for (enum LogModule i = 0; i < LOG_MOD_MAX; i++)
    {
        if (strcmp(log_modules[i], s) == 0)
        {
            return i;
        }
    }

    return LOG_MOD_NONE;
}

void LogEnableModule(enum LogModule mod)
{
    assert(mod < LOG_MOD_MAX);

    module_is_enabled[mod] = true;
}

void LogModuleHelp(void)
{
    printf("\n--log-modules accepts a comma separated list of one or more of the following:\n\n");
    printf("    help\n");
    printf("    all\n");
    for (enum LogModule i = LOG_MOD_NONE + 1;  i < LOG_MOD_MAX;  i++)
    {
        printf("    %s\n", log_modules[i]);
    }
    printf("\n");
}

/**
 * Parse a string of modules, and enable the relevant DEBUG logging modules.
 * Example strings:
 *
 *   all         : enables all debug modules
 *   help        : enables nothing, but prints a help message
 *   iterctx     : enables the "iterctx" debug logging module
 *   iterctx,vars: enables the 2 debug modules, "iterctx" and "vars"
 *
 * @NOTE modifies string #s but restores it before returning.
 */
bool LogEnableModulesFromString(char *s)
{
    bool retval = true;

    const char *token = s;
    char saved_sep = ',';                     /* any non-NULL value will do */
    while (saved_sep != '\0' && retval != false)
    {
        char *next_token = strchrnul(token, ',');
        saved_sep        = *next_token;
        *next_token      = '\0';                      /* modify parameter s */
        size_t token_len = next_token - token;

        if (strcmp(token, "help") == 0)
        {
            LogModuleHelp();
            retval = false;                                   /* early exit */
        }
        else if (strcmp(token, "all") == 0)
        {
            for (enum LogModule j = LOG_MOD_NONE + 1; j < LOG_MOD_MAX; j++)
            {
                LogEnableModule(j);
            }
        }
        else
        {
            enum LogModule mod = LogModuleFromString(token);

            assert(mod < LOG_MOD_MAX);
            if (mod == LOG_MOD_NONE)
            {
                Log(LOG_LEVEL_WARNING,
                    "Unknown debug logging module '%*s'",
                    (int) token_len, token);
            }
            else
            {
                LogEnableModule(mod);
            }
        }


        *next_token = saved_sep;            /* restore modified parameter s */
        next_token++;                       /* bypass comma */
        token = next_token;
    }

    return retval;
}

bool LogModuleEnabled(enum LogModule mod)
{
    assert(mod > LOG_MOD_NONE);
    assert(mod < LOG_MOD_MAX);

    if (module_is_enabled[mod])
    {
        return true;
    }
    else
    {
        return false;
    }
}

void LogDebug(enum LogModule mod, const char *fmt, ...)
{
    assert(mod < LOG_MOD_MAX);

    /* Did we forget any entry in log_modules? Should be a static assert. */
    assert(sizeof(log_modules) / sizeof(log_modules[0]) == LOG_MOD_MAX);

    if (LogModuleEnabled(mod))
    {
        va_list ap;
        va_start(ap, fmt);
        VLog(LOG_LEVEL_DEBUG, fmt, ap);
        va_end(ap);
        /* VLog(LOG_LEVEL_DEBUG, "%s: ...", */
        /*      debug_modules_description[mod_order], ...); */
    }
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
