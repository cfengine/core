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

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdbool.h>

static void msgheader(void)
{
    fprintf(stderr, "[fakesyslog][%d] ", (int)getuid());
}

typedef struct
{
    int mask;
    const char *name;
} masks;

static const masks logopts[] = {
    { LOG_PID, "LOG_PID" },
    { LOG_CONS, "LOG_CONS" },
    { LOG_NDELAY, "LOG_NDELAY" },
    { LOG_ODELAY, "LOG_ODELAY" },
    { LOG_NOWAIT, "LOG_NOWAIT" }
};

static void print_mask(int value, const masks *masks, int masks_size)
{
    bool got_value = false;

    for (int i = 0; i < masks_size; ++i)
    {
        if (value & masks[i].mask)
        {
            fprintf(stderr, "%s%s", got_value ? " | " : "", masks[i].name);
            got_value = true;
            value &= ~masks[i].mask;
        }
    }

    if (value)
    {
        fprintf(stderr, "%s0x%02X", got_value ? " | " : "", value);
        got_value = true;
    }

    if (!got_value)
    {
        fprintf(stderr, "0");
    }
}

static const char *get_level_str(int level)
{
    switch (level)
    {
    case LOG_EMERG: return "LOG_EMERG";
    case LOG_ALERT: return "LOG_ALERT";
    case LOG_CRIT: return "LOG_CRIT";
    case LOG_ERR: return "LOG_ERR";
    case LOG_WARNING: return "LOG_WARNING";
    case LOG_NOTICE: return "LOG_NOTICE";
    case LOG_INFO: return "LOG_INFO";
    case LOG_DEBUG: return "LOG_DEBUG";
    default: return NULL;
    }
}

static const char *get_facility_str(int facility)
{
    switch (facility)
    {
    case LOG_KERN: return "LOG_KERN";
    case LOG_USER: return "LOG_USER";
    case LOG_MAIL: return "LOG_MAIL";
    case LOG_NEWS: return "LOG_NEWS";
    case LOG_UUCP: return "LOG_UUCP";
    case LOG_DAEMON: return "LOG_DAEMON";
    case LOG_AUTH: return "LOG_AUTH";
    case LOG_CRON: return "LOG_CRON";
    case LOG_LPR: return "LOG_LPR";
    case LOG_LOCAL0: return "LOG_LOCAL0";
    case LOG_LOCAL1: return "LOG_LOCAL1";
    case LOG_LOCAL2: return "LOG_LOCAL2";
    case LOG_LOCAL3: return "LOG_LOCAL3";
    case LOG_LOCAL4: return "LOG_LOCAL4";
    case LOG_LOCAL5: return "LOG_LOCAL5";
    case LOG_LOCAL6: return "LOG_LOCAL6";
    case LOG_LOCAL7: return "LOG_LOCAL7";
    default: return NULL;
    }
}

static void print_priority(int arg)
{
    int level = LOG_PRI(arg);
    int facility = arg & ~level;

    const char *level_str = get_level_str(level);
    const char *facility_str = get_facility_str(facility);

    if (facility_str)
    {
        fprintf(stderr, "%s | %s", level_str, facility_str);
    }
    else
    {
        fprintf(stderr, "%s | 0x%02X", level_str, facility);
    }
}

static int logmask = LOG_UPTO(LOG_DEBUG);

static const masks priority_masks[] = {
    { LOG_MASK(LOG_EMERG), "LOG_EMERG" },
    { LOG_MASK(LOG_ALERT), "LOG_ALERT" },
    { LOG_MASK(LOG_CRIT), "LOG_CRIT" },
    { LOG_MASK(LOG_ERR), "LOG_ERR" },
    { LOG_MASK(LOG_WARNING), "LOG_WARNING" },
    { LOG_MASK(LOG_NOTICE), "LOG_NOTICE" },
    { LOG_MASK(LOG_INFO), "LOG_INFO" },
    { LOG_MASK(LOG_DEBUG), "LOG_DEBUG" }
};




void openlog(const char *ident, int logopt, int facility)
{
    msgheader();

    fprintf(stderr, "openlog(%s, ", ident ? ident : "<none>");
    print_mask(logopt, logopts, sizeof(logopts)/sizeof(logopts[0]));
    const char *facility_str = get_facility_str(facility);
    if (facility_str)
    {
        fprintf(stderr, ", %s)\n", facility_str);
    }
    else
    {
        fprintf(stderr, ", 0x%02X)\n", facility);
    }
}

void vsyslog(int priority, const char *message, va_list ap)
{
    msgheader();
    fputs("syslog(", stderr);
    print_priority(priority);
    fputs(", ", stderr);
    vfprintf(stderr, message, ap);
    fputs(")\n", stderr);
}

void syslog(int priority, const char *message, ...)
{
    va_list ap;
    va_start(ap, message);
    vsyslog(priority, message, ap);
    va_end(ap);
}

int setlogmask(int maskpri)
{
    int oldlogmask = logmask;
    logmask = maskpri;
    msgheader();
    fputs("setlogmask(", stderr);
    print_mask(maskpri, priority_masks, sizeof(priority_masks)/sizeof(priority_masks[0]));
    fputs(")\n", stderr);
    return oldlogmask;
}

void closelog(void)
{
    msgheader();
    fputs("closelog()\n", stderr);
}
