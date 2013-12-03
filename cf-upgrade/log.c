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

#include <log.h>
#include <alloc-mini.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

#define MAX_LOG_ENTRY_SIZE  4096

static FILE *log_stream = NULL;
static LogLevel current_level = LogNormal;

static char *prepare_message(char *format, va_list args)
{
    char *message = NULL;
    int message_size = 0;
    char timestamp[] = "YYYY/MM/DD HH:MM:SS";
    char buffer[MAX_LOG_ENTRY_SIZE];
    time_t now = time(NULL);
    struct tm *now_tm = gmtime(&now);
    int timestamp_size = sizeof(timestamp);
    message_size = vsnprintf(buffer, MAX_LOG_ENTRY_SIZE - 1, format, args);
    strftime(timestamp, timestamp_size, "%Y/%m/%d %H:%M:%S", now_tm);
    /* '[' + ']' + ' ' + '\0' */
    message = xmalloc(message_size + timestamp_size + 4);
    sprintf(message, "[%s] %s", timestamp, buffer);
    return message;
}

static void write_console_log_entry(const char *message)
{
    puts(message);
}

static void write_file_log_entry(const char *message)
{
    if (log_stream)
    {
        fputs(message, log_stream);
        fputs("\n", log_stream);
    }
}

#ifndef __MINGW32__
/* Unix implementation */
static void private_log_init()
{
    char path[] = "cf-upgrade-YYYYMMDD-HHMMSS.log";
    int path_size = sizeof(path);
    time_t now_seconds = time(NULL);
    struct tm *now_tm = gmtime(&now_seconds);
    int log_fd = -1;

    strftime(path, path_size, "cf-upgrade-%Y%m%d-%H%M%S.log", now_tm);
    log_fd = open(path, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (log_fd < 0)
    {
        puts("Could not initialize log file, only console messages will be printed");
        return;
    }
    /*
     * cf-upgrade spawns itself, therefore to avoid log confusion we need to
     * make sure that the log is closed when we call exeve.
     */
    int result = 0;
    result = fcntl(log_fd, F_GETFD);
    if (result < 0)
    {
        puts("Could not initialize log file, only console messages will be printed");
        return;
    }
    result = fcntl(log_fd, F_SETFD, result | FD_CLOEXEC);
    if (result < 0)
    {
        puts("Could not initialize log file, only console messages will be printed");
        return;
    }
    log_stream = fdopen(log_fd, "a");
}
#else
/* Windows implementation */
static void private_log_init()
{
    char path[] = "cf-upgrade-YYYYMMDD-HHMMSS.log";
    int path_size = sizeof(path);
    time_t now_seconds = time(NULL);
    struct tm *now_tm = gmtime(&now_seconds);
    int log_fd = -1;

    strftime(path, path_size, "cf-upgrade-%Y%m%d-%H%M%S.log", now_tm);
    log_fd = open(path, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
    if (log_fd < 0)
    {
        puts("Could not initialize log file, only console messages will be printed");
        return;
    }
    log_stream = fdopen(log_fd, "a");
}
#endif

void logInit()
{
    private_log_init();
}

void logFinish()
{
    if (log_stream)
    {
        fclose(log_stream);
    }
}

void log_entry(LogLevel level, char *format, ...)
{
    if (level > current_level)
    {
        return;
    }
    va_list ap;
    va_start(ap, format);
    char *message = prepare_message(format, ap);
    va_end(ap);
    switch (level)
    {
    case LogCritical:
        write_console_log_entry(message);
        write_file_log_entry(message);
        break;
    case LogNormal:
        write_file_log_entry(message);
        break;
    case LogDebug:
        write_file_log_entry(message);
        break;
    }
    free(message);
}
