/*
   Copyright 2018 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <processes_select.h>

#include <string.h>

#include <eval_context.h>
#include <files_names.h>
#include <conversion.h>
#include <matching.h>
#include <systype.h>
#include <string_lib.h>                                         /* Chop */
#include <regex.h> /* CompileRegex,StringMatchWithPrecompiledRegex,StringMatchFull */
#include <item_lib.h>
#include <pipes.h>
#include <files_interfaces.h>
#include <rlist.h>
#include <policy.h>
#include <zones.h>
#include <printsize.h>
#include <known_dirs.h>

# ifdef HAVE_GETZONEID
#include <sequence.h>
#define MAX_ZONENAME_SIZE 64
# endif

#ifdef _WIN32
#define TABLE_STORAGE
#else
#define TABLE_STORAGE static
#endif
TABLE_STORAGE Item *PROCESSTABLE = NULL;

typedef enum
{
    /*
     * In this mode, all columns must be present by the occurrence of at least
     * one non-whitespace character.
     */
    PCA_AllColumnsPresent,
    /*
     * In this mode, if a process is a zombie, and if there is nothing but
     * whitespace in the byte columns directly below a header, that column is
     * skipped.
     *
     * This means that very little text shifting will be tolerated, preferably
     * none, or small enough that all column entries still remain under their
     * own header. It also means that a zombie process must be detectable by
     * reading the 'Z' state directly below the 'S' or 'ST' header.
     */
    PCA_ZombieSkipEmptyColumns,
} PsColumnAlgorithm;

/*
  Ideally this should be autoconf-tested, but it's really hard to do so. See
  SplitProcLine() to see the exact effects this has.
*/
static const PsColumnAlgorithm PS_COLUMN_ALGORITHM[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = PCA_AllColumnsPresent,
    [PLATFORM_CONTEXT_OPENVZ] = PCA_AllColumnsPresent,      /* virt_host_vz_vzps */
    [PLATFORM_CONTEXT_HP] = PCA_AllColumnsPresent,          /* hpux */
    [PLATFORM_CONTEXT_AIX] = PCA_ZombieSkipEmptyColumns,    /* aix */
    [PLATFORM_CONTEXT_LINUX] = PCA_AllColumnsPresent,       /* linux */
    [PLATFORM_CONTEXT_BUSYBOX] = PCA_AllColumnsPresent,     /* linux */
    [PLATFORM_CONTEXT_SOLARIS] = PCA_AllColumnsPresent,     /* solaris >= 11 */
    [PLATFORM_CONTEXT_SUN_SOLARIS] = PCA_AllColumnsPresent, /* solaris  < 11 */
    [PLATFORM_CONTEXT_FREEBSD] = PCA_AllColumnsPresent,     /* freebsd */
    [PLATFORM_CONTEXT_NETBSD] = PCA_AllColumnsPresent,      /* netbsd */
    [PLATFORM_CONTEXT_CRAYOS] = PCA_AllColumnsPresent,      /* cray */
    [PLATFORM_CONTEXT_WINDOWS_NT] = PCA_AllColumnsPresent,  /* NT - cygnus */
    [PLATFORM_CONTEXT_SYSTEMV] = PCA_AllColumnsPresent,     /* unixware */
    [PLATFORM_CONTEXT_OPENBSD] = PCA_AllColumnsPresent,     /* openbsd */
    [PLATFORM_CONTEXT_CFSCO] = PCA_AllColumnsPresent,       /* sco */
    [PLATFORM_CONTEXT_DARWIN] = PCA_AllColumnsPresent,      /* darwin */
    [PLATFORM_CONTEXT_QNX] = PCA_AllColumnsPresent,         /* qnx  */
    [PLATFORM_CONTEXT_DRAGONFLY] = PCA_AllColumnsPresent,   /* dragonfly */
    [PLATFORM_CONTEXT_MINGW] = PCA_AllColumnsPresent,       /* mingw */
    [PLATFORM_CONTEXT_VMWARE] = PCA_AllColumnsPresent,      /* vmware */
    [PLATFORM_CONTEXT_ANDROID] = PCA_AllColumnsPresent,     /* android */
};

#if defined(__sun) || defined(TEST_UNIT_TEST)
static StringMap *UCB_PS_MAP = NULL;
// These will be tried in order.
static const char *UCB_STYLE_PS[] = {
    "/usr/ucb/ps",
    "/bin/ps",
    NULL
};
static const char *UCB_STYLE_PS_ARGS = "axwww";
static const PsColumnAlgorithm UCB_STYLE_PS_COLUMN_ALGORITHM = PCA_ZombieSkipEmptyColumns;
#endif

static int SelectProcRangeMatch(char *name1, char *name2, int min, int max, char **names, char **line);
static bool SelectProcRegexMatch(const char *name1, const char *name2, const char *regex, bool anchored, char **colNames, char **line);
static bool SplitProcLine(const char *proc,
                          time_t pstime,
                          char **names,
                          int *start,
                          int *end,
                          PsColumnAlgorithm pca,
                          char **line);
static int SelectProcTimeCounterRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line);
static int SelectProcTimeAbsRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line);
static int GetProcColumnIndex(const char *name1, const char *name2, char **names);
static void GetProcessColumnNames(const char *proc, char **names, int *start, int *end);
static int ExtractPid(char *psentry, char **names, int *end);
static void ApplyPlatformExtraTable(char **names, char **columns);

/***************************************************************************/

static bool SelectProcess(const char *procentry,
                          time_t pstime,
                          char **names,
                          int *start,
                          int *end,
                          const char *process_regex,
                          ProcessSelect a,
                          bool attrselect)
{
    bool result = true;
    char *column[CF_PROCCOLS];
    Rlist *rp;

    assert(process_regex);

    StringSet *process_select_attributes = StringSetNew();

    memset(column, 0, sizeof(column));

    if (!SplitProcLine(procentry, pstime, names, start, end,
                       PS_COLUMN_ALGORITHM[VPSHARDCLASS], column))
    {
        result = false;
        goto cleanup;
    }

    ApplyPlatformExtraTable(names, column);

    for (int i = 0; names[i] != NULL; i++)
    {
        LogDebug(LOG_MOD_PS, "In SelectProcess, COL[%s] = '%s'",
                 names[i], column[i]);
    }

    if (!SelectProcRegexMatch("CMD", "COMMAND", process_regex, false, names, column))
    {
        result = false;
        goto cleanup;
    }

    if (!attrselect)
    {
        // If we are not considering attributes, then the matching is done.
        goto cleanup;
    }

    for (rp = a.owner; rp != NULL; rp = rp->next)
    {
        if (rp->val.type == RVAL_TYPE_FNCALL)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Function call '%s' in process_select body was not resolved, skipping",
                RlistFnCallValue(rp)->name);
        }
        else if (SelectProcRegexMatch("USER", "UID", RlistScalarValue(rp), true, names, column))
        {
            StringSetAdd(process_select_attributes, xstrdup("process_owner"));
            break;
        }
    }

    if (SelectProcRangeMatch("PID", "PID", a.min_pid, a.max_pid, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("pid"));
    }

    if (SelectProcRangeMatch("PPID", "PPID", a.min_ppid, a.max_ppid, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("ppid"));
    }

    if (SelectProcRangeMatch("PGID", "PGID", a.min_pgid, a.max_pgid, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("pgid"));
    }

    if (SelectProcRangeMatch("VSZ", "SZ", a.min_vsize, a.max_vsize, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("vsize"));
    }

    if (SelectProcRangeMatch("RSS", "RSS", a.min_rsize, a.max_rsize, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("rsize"));
    }

    if (SelectProcTimeCounterRangeMatch("TIME", "TIME", a.min_ttime, a.max_ttime, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("ttime"));
    }

    if (SelectProcTimeAbsRangeMatch
        ("STIME", "START", a.min_stime, a.max_stime, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("stime"));
    }

    if (SelectProcRangeMatch("NI", "PRI", a.min_pri, a.max_pri, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("priority"));
    }

    if (SelectProcRangeMatch("NLWP", "NLWP", a.min_thread, a.max_thread, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("threads"));
    }

    if (SelectProcRegexMatch("S", "STAT", a.status, true, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("status"));
    }

    if (SelectProcRegexMatch("CMD", "COMMAND", a.command, true, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("command"));
    }

    if (SelectProcRegexMatch("TTY", "TTY", a.tty, true, names, column))
    {
        StringSetAdd(process_select_attributes, xstrdup("tty"));
    }

    if (!a.process_result)
    {
        if (StringSetSize(process_select_attributes) == 0)
        {
            result = EvalProcessResult("", process_select_attributes);
        }
        else
        {
            Writer *w = StringWriter();
            StringSetIterator iter = StringSetIteratorInit(process_select_attributes);
            char *attr = StringSetIteratorNext(&iter);
            WriterWrite(w, attr);

            while ((attr = StringSetIteratorNext(&iter)))
            {
                WriterWriteChar(w, '.');
                WriterWrite(w, attr);
            }

            result = EvalProcessResult(StringWriterData(w), process_select_attributes);
            WriterClose(w);
        }
    }
    else
    {
        result = EvalProcessResult(a.process_result, process_select_attributes);
    }

cleanup:
    StringSetDestroy(process_select_attributes);

    for (int i = 0; column[i] != NULL; i++)
    {
        free(column[i]);
    }

    return result;
}

Item *SelectProcesses(const char *process_name, ProcessSelect a, bool attrselect)
{
    const Item *processes = PROCESSTABLE;
    Item *result = NULL;

    if (processes == NULL)
    {
        return result;
    }

    char *names[CF_PROCCOLS];
    int start[CF_PROCCOLS];
    int end[CF_PROCCOLS];

    GetProcessColumnNames(processes->name, names, start, end);

    /* TODO: use actual time of ps-run, as time(NULL) may be later. */
    time_t pstime = time(NULL);

    for (Item *ip = processes->next; ip != NULL; ip = ip->next)
    {
        if (NULL_OR_EMPTY(ip->name))
        {
            continue;
        }

        if (!SelectProcess(ip->name, pstime, names, start, end, process_name, a, attrselect))
        {
            continue;
        }

        pid_t pid = ExtractPid(ip->name, names, end);

        if (pid == -1)
        {
            Log(LOG_LEVEL_VERBOSE, "Unable to extract pid while looking for %s", process_name);
            continue;
        }

        PrependItem(&result, ip->name, "");
        result->counter = (int)pid;
    }

    for (int i = 0; i < CF_PROCCOLS; i++)
    {
        free(names[i]);
    }

    return result;
}

static int SelectProcRangeMatch(char *name1, char *name2, int min, int max, char **names, char **line)
{
    int i;
    long value;

    if ((min == CF_NOINT) || (max == CF_NOINT))
    {
        return false;
    }

    if ((i = GetProcColumnIndex(name1, name2, names)) != -1)
    {
        value = IntFromString(line[i]);

        if (value == CF_NOINT)
        {
            Log(LOG_LEVEL_INFO, "Failed to extract a valid integer from '%s' => '%s' in process list", names[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    return false;
}

/***************************************************************************/

static long TimeCounter2Int(const char *s)
{
    long days, hours, minutes, seconds;

    if (s == NULL)
    {
        return CF_NOINT;
    }

    /* If we match dd-hh:mm[:ss], believe it: */
    int got = sscanf(s, "%ld-%ld:%ld:%ld", &days, &hours, &minutes, &seconds);
    if (got > 2)
    {
        /* All but perhaps seconds set */
    }
    /* Failing that, try matching hh:mm[:ss] */
    else if (1 < (got = sscanf(s, "%ld:%ld:%ld", &hours, &minutes, &seconds)))
    {
        /* All but days and perhaps seconds set */
        days = 0;
        got++;
    }
    else
    {
        Log(LOG_LEVEL_ERR,
            "Unable to parse 'ps' time field as [dd-]hh:mm[:ss], got '%s'",
            s);
        return CF_NOINT;
    }
    assert(got > 2); /* i.e. all but maybe seconds have been set */
    /* Clear seconds if unset: */
    if (got < 4)
    {
        seconds = 0;
    }

    LogDebug(LOG_MOD_PS, "TimeCounter2Int:"
             " Parsed '%s' as elapsed time '%ld-%02ld:%02ld:%02ld'",
             s, days, hours, minutes, seconds);

    /* Convert to seconds: */
    return ((days * 24 + hours) * 60 + minutes) * 60 + seconds;
}

static int SelectProcTimeCounterRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line)
{
    if ((min == CF_NOINT) || (max == CF_NOINT))
    {
        return false;
    }

    int i = GetProcColumnIndex(name1, name2, names);
    if (i != -1)
    {
        time_t value = (time_t) TimeCounter2Int(line[i]);

        if (value == CF_NOINT)
        {
            Log(LOG_LEVEL_INFO,
                "Failed to extract a valid integer from %c => '%s' in process list",
                name1[i], line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            Log(LOG_LEVEL_VERBOSE, "Selection filter matched counter range"
                " '%s/%s' = '%s' in [%jd,%jd] (= %jd secs)",
                  name1, name2, line[i], (intmax_t)min, (intmax_t)max, (intmax_t)value);
            return true;
        }
        else
        {
            LogDebug(LOG_MOD_PS, "Selection filter REJECTED counter range"
                     " '%s/%s' = '%s' in [%jd,%jd] (= %jd secs)",
                     name1, name2, line[i],
                     (intmax_t) min, (intmax_t) max, (intmax_t) value);
            return false;
        }
    }

    return false;
}

static time_t TimeAbs2Int(const char *s)
{
    if (s == NULL)
    {
        return CF_NOINT;
    }

    struct tm tm;
    localtime_r(&CFSTARTTIME, &tm);
    tm.tm_sec = 0;
    tm.tm_isdst = -1;

    /* Try various ways to parse s: */
    char word[4]; /* Abbreviated month name */
    long ns[3]; /* Miscellaneous numbers, diverse readings */
    int got = sscanf(s, "%2ld:%2ld:%2ld", ns, ns + 1, ns + 2);
    if (1 < got) /* Hr:Min[:Sec] */
    {
        tm.tm_hour = ns[0];
        tm.tm_min = ns[1];
        if (got == 3)
        {
            tm.tm_sec = ns[2];
        }
    }
    /* or MMM dd (the %ld shall ignore any leading space) */
    else if (sscanf(s, "%3[a-zA-Z]%ld", word, ns) == 2 &&
             /* Only match if word is a valid month text: */
             0 < (ns[1] = Month2Int(word)))
    {
        int month = ns[1] - 1;
        if (tm.tm_mon < month)
        {
            /* Wrapped around */
            tm.tm_year--;
        }
        tm.tm_mon = month;
        tm.tm_mday = ns[0];
        tm.tm_hour = 0;
        tm.tm_min = 0;
    }
    /* or just year, or seconds since 1970 */
    else if (sscanf(s, "%ld", ns) == 1)
    {
        if (ns[0] > 9999)
        {
            /* Seconds since 1970.
             *
             * This is the amended value SplitProcLine() replaces
             * start time with if it's imprecise and a better value
             * can be calculated from elapsed time.
             */
            return (time_t)ns[0];
        }
        /* else year, at most four digits; either 4-digit CE or
         * already relative to 1900. */

        memset(&tm, 0, sizeof(tm));
        tm.tm_year = ns[0] < 999 ? ns[0] : ns[0] - 1900;
        tm.tm_isdst = -1;
    }
    else
    {
        return CF_NOINT;
    }

    return mktime(&tm);
}

static int SelectProcTimeAbsRangeMatch(char *name1, char *name2, time_t min, time_t max, char **names, char **line)
{
    int i;
    time_t value;

    if ((min == CF_NOINT) || (max == CF_NOINT))
    {
        return false;
    }

    if ((i = GetProcColumnIndex(name1, name2, names)) != -1)
    {
        value = TimeAbs2Int(line[i]);

        if (value == CF_NOINT)
        {
            Log(LOG_LEVEL_INFO, "Failed to extract a valid integer from %c => '%s' in process list", name1[i],
                  line[i]);
            return false;
        }

        if ((min <= value) && (value <= max))
        {
            Log(LOG_LEVEL_VERBOSE, "Selection filter matched absolute '%s/%s' = '%s(%jd)' in [%jd,%jd]", name1, name2, line[i], (intmax_t)value,
                  (intmax_t)min, (intmax_t)max);
            return true;
        }
        else
        {
            return false;
        }
    }

    return false;
}

/***************************************************************************/

static bool SelectProcRegexMatch(const char *name1, const char *name2,
                                 const char *regex, bool anchored,
                                 char **colNames, char **line)
{
    int i;

    if (regex == NULL)
    {
        return false;
    }

    if ((i = GetProcColumnIndex(name1, name2, colNames)) != -1)
    {
        if (anchored)
        {
            return StringMatchFull(regex, line[i]);
        }
        else
        {
            int s, e;
            return StringMatch(regex, line[i], &s, &e);
        }
    }

    return false;
}

static void PrintStringIndexLine(int prefix_spaces, int len)
{
    char arrow_str[CF_BUFSIZE];
    arrow_str[0] = '^';
    arrow_str[1] = '\0';
    char index_str[CF_BUFSIZE];
    index_str[0] = '0';
    index_str[1] = '\0';
    for (int lineindex = 10; lineindex <= len; lineindex += 10)
    {
        char num[PRINTSIZE(lineindex)];
        xsnprintf(num, sizeof(num), "%10d", lineindex);
        strlcat(index_str, num, sizeof(index_str));
        strlcat(arrow_str, "         ^", sizeof(arrow_str));
    }

    // Prefix the beginning of the indexes with the given number.
    LogDebug(LOG_MOD_PS, "%*s%s", prefix_spaces, "", arrow_str);
    LogDebug(LOG_MOD_PS, "%*s%s", prefix_spaces, "Index: ", index_str);
}

static void MaybeFixStartTime(const char *line,
                              time_t pstime,
                              char **names,
                              char **fields)
{
    /* Since start times can be very imprecise (e.g. just a past day's
     * date, or a past year), calculate a better value from elapsed
     * time, if available: */
    int k = GetProcColumnIndex("ELAPSED", "ELAPSED", names);
    if (k != -1)
    {
        const long elapsed = TimeCounter2Int(fields[k]);
        if (elapsed != CF_NOINT) /* Only use if parsed successfully ! */
        {
            int j = GetProcColumnIndex("STIME", "START", names), ns[3];
            /* Trust the reported value if it matches hh:mm[:ss], though: */
            if (sscanf(fields[j], "%d:%d:%d", ns, ns + 1, ns + 2) < 2)
            {
                time_t value = pstime - (time_t) elapsed;

                LogDebug(LOG_MOD_PS,
                    "processes: Replacing parsed start time %s with %s",
                    fields[j], ctime(&value));

                free(fields[j]);
                xasprintf(fields + j, "%ld", value);
            }
        }
        else if (fields[k])
        {
            Log(LOG_LEVEL_VERBOSE,
                "Parsing problem was in ELAPSED field of '%s'",
                line);
        }
    }
}

/*******************************************************************/
/* fields must be char *fields[CF_PROCCOLS] in fact. */
/* pstime should be the time at which ps was run. */

static bool SplitProcLine(const char *line,
                          time_t pstime,
                          char **names,
                          int *start,
                          int *end,
                          PsColumnAlgorithm pca,
                          char **fields)
{
    if (line == NULL || line[0] == '\0')
    {
        return false;
    }

    size_t linelen = strlen(line);

    if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
    {
        LogDebug(LOG_MOD_PS, "Parsing ps line: '%s'", line);
        // Makes the entry line up with the line above.
        PrintStringIndexLine(18, linelen);
    }

    /*
      All platforms have been verified to not produce overlapping fields with
      currently used ps tools, and hence we can parse based on space separation
      (with some caveats, see below).

      Dates may have spaces in them, like "May  4", or not, like "May4". Prefer
      to match a date without spaces as long as it contains a number, but fall
      back to parsing letters followed by space(s) and a date.

      Commands will also have extra spaces, but it is always the last field, so
      we just include spaces at this point in the parsing.

      An additional complication is that some platforms (only AIX is known at
      the time of writing) can have empty columns when a process is a
      zombie. The plan is to match for this by checking the range between start
      and end directly below the header (same byte position). If the whole range
      is whitespace we consider the entry missing. The columns are (presumably)
      guaranteed to line up correctly for this, since zombie processes do not
      have memory usage which can produce large, potentially alignment-altering
      numbers. However, we cannot do this whitespace check in general, because
      non-zombie processes may shift columns in a way that leaves some columns
      apparently (but not actually) empty. Zombie processes have state Z and
      command <defunct> on AIX. Similarly processes marked with command
      <exiting> also have missing columns and need to be skipped. (AIX only).

      Take these two examples:

AIX:
    USER      PID     PPID     PGID  %CPU  %MEM   VSZ NI S    STIME        TIME COMMAND
    root        1        0        0   0.0   0.0   784 20 A   Nov 28    00:00:00 /etc/init
    root  1835344        1  1835344   0.0   0.0   944 20 A   Nov 28    00:00:00 /usr/lib/errdemon
    root  2097594        1  1638802   0.0   0.0   596 20 A   Nov 28    00:00:05 /usr/sbin/syncd 60
    root  3408328        1  3408328   0.0   0.0   888 20 A   Nov 28    00:00:00 /usr/sbin/srcmstr
    root  4325852  3408328  4325852   0.0   0.0   728 20 A   Nov 28    00:00:00 /usr/sbin/syslogd
    root  4784534  3408328  4784534   0.0   0.0  1212 20 A   Nov 28    00:00:00 sendmail: accepting connections
    root  5898690        1  5898690   0.0   0.0  1040 20 A   Nov 28    00:00:00 /usr/sbin/cron
          6095244  8913268  8913268                   20 Z             00:00:00 <defunct>
    root  6160866  3408328  6160866   0.0   0.0  1612 20 A   Nov 28    00:00:00 /opt/rsct/bin/IBM.ServiceRMd
          6750680 17826152 17826152                   20 Z             00:00:00 <defunct>
    root  7143692  3408328  7143692   0.0   0.0   476 20 A   Nov 28    00:00:00 /var/perf/pm/bin/pmperfrec
    root  7340384  8651136  8651136   0.0   0.0   500 20 A   Nov 28    00:00:00 [trspoolm]
    root  7602560  8978714  7602560   0.0   0.0   636 20 A   Nov 28    00:00:00 sshd: u0013628 [priv]
          7733720        -        -                    - A                    - <exiting>

Solaris 9:
    USER   PID %CPU %MEM   SZ  RSS TT      S    STIME        TIME COMMAND
 jenkins 29769  0.0  0.0  810 2976 pts/1   S 07:22:43        0:00 /usr/bin/perl ../../ps.pl
 jenkins 29835    -    -    0    0 ?       Z        -        0:00 <defunct>
 jenkins 10026  0.0  0.3 30927 143632 ?       S   Jan_21    01:18:58 /usr/jdk/jdk1.6.0_45/bin/java -jar slave.jar

      Due to how the process state 'S' is shifted under the 'S' header in the
      second example, it is not possible to separate between this and a missing
      column. Counting spaces is no good, because commands can contain an
      arbitrary number of spaces, and there is no way to know the byte position
      where a command begins. Hence the only way is to base this algorithm on
      platform and only do the "empty column detection" when:
        * The platform is known to produce empty columns for zombie processes
          (see PCA_ZombieSkipEmptyColumns)
        * The platform is known to not shift columns when the process is a
          zombie.
        * It is a zombie / exiting / idle process
          (These states provide almost no useful info in ps output)
    */

    bool skip = false;

    if (pca == PCA_ZombieSkipEmptyColumns)
    {
        // Find out if the process is a zombie.
        for (int field = 0; names[field] && !skip; field++)
        {
            if (strcmp(names[field], "S") == 0 ||
                strcmp(names[field], "ST") == 0)
            {
                // Check for zombie state.
                for (int pos = start[field]; pos <= end[field] && pos < linelen && !skip; pos++)
                {
                    // 'Z' letter with word boundary on each side.
                    if (isspace(line[pos - 1])
                        && line[pos] == 'Z'
                        && (isspace(line[pos + 1])
                            || line[pos + 1] == '\0'))
                    {
                        LogDebug(LOG_MOD_PS, "Detected zombie process, "
                                 "skipping parsing of empty ps fields.");
                        skip = true;
                    }
                }
            }
            else if (strcmp(names[field], "COMMAND") == 0)
            {
                // Check for exiting or idle state.
                for (int pos = start[field]; pos <= end[field] && pos < linelen && !skip; pos++)
                {
                    if (!isspace(line[pos])) // Skip spaces
                    {
                        if (strncmp(line + pos, "<exiting>", 9) == 0 ||
                            strncmp(line + pos, "<idle>", 6) == 0)
                        {
                            LogDebug(LOG_MOD_PS, "Detected exiting/idle process, "
                                     "skipping parsing of empty ps fields.");
                            skip = true;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }
    }

    int field = 0;
    int pos = 0;
    while (names[field])
    {
        // Some sanity checks.
        if (pos >= linelen)
        {
            if (pca == PCA_ZombieSkipEmptyColumns && skip)
            {
                LogDebug(LOG_MOD_PS, "Assuming '%s' field is empty, "
                    "since ps line '%s' is not long enough to reach under its "
                    "header.", names[field], line);
                fields[field] = xstrdup("");
                field++;
                continue;
            }
            else
            {
                Log(LOG_LEVEL_ERR, "ps output line '%s' is shorter than its "
                    "associated header.", line);
                return false;
            }
        }

        bool cmd = (strcmp(names[field], "CMD") == 0 ||
                    strcmp(names[field], "COMMAND") == 0);
        bool stime = !cmd && (strcmp(names[field], "STIME") == 0);

        // Equal boolean results, either both must be true, or both must be
        // false. IOW we must either both be at the last field, and it must be
        // CMD, or none of those.      |
        //                             v
        if ((names[field + 1] != NULL) == cmd)
        {
            Log(LOG_LEVEL_ERR, "Last field of ps output '%s' is not "
                "CMD/COMMAND.", line);
            return false;
        }

        // If zombie/exiting, check if field is empty.
        if (pca == PCA_ZombieSkipEmptyColumns && skip)
        {
            int empty_pos = start[field];
            bool empty = true;
            while (empty_pos <= end[field])
            {
                if (!isspace(line[empty_pos]))
                {
                    empty = false;
                    break;
                }
                empty_pos++;
            }
            if (empty)
            {
                LogDebug(LOG_MOD_PS, "Detected empty"
                         " '%s' field between positions %d and %d",
                         names[field], start[field], end[field]);
                fields[field] = xstrdup("");
                pos = end[field] + 1;
                field++;
                continue;
            }
            else
            {
                LogDebug(LOG_MOD_PS, "Detected non-empty "
                         "'%s' field between positions %d and %d",
                         names[field], start[field], end[field]);
            }
        }

        // Preceding space.
        while (isspace(line[pos]))
        {
            pos++;
        }

        // Field.
        int last = pos;
        if (cmd)
        {
            // Last field, slurp up the rest, but discard trailing whitespace.
            last = linelen;
            while (isspace(line[last - 1]))
            {
                last--;
            }
        }
        else if (stime)
        {
            while (isalpha(line[last]))
            {
                last++;
            }
            if (isspace(line[last]))
            {
                // In this case we expect spaces followed by a number.
                // It means what we first read was the month, now is the date.
                do
                {
                    last++;
                } while (isspace(line[last]));
                if (!isdigit(line[last]))
                {
                    char fmt[200];
                    xsnprintf(fmt, sizeof(fmt), "Unable to parse STIME entry in ps "
                              "output line '%%s': Expected day number after "
                              "'%%.%ds'", (last - 1) - pos);
                    Log(LOG_LEVEL_ERR, fmt, line, line + pos);
                    return false;
                }
            }
            while (line[last] && !isspace(line[last]))
            {
                last++;
            }
        }
        else
        {
            // Generic fields
            while (line[last] && !isspace(line[last]))
            {
                last++;
            }
        }

        // Make a copy and store in fields.
        fields[field] = xstrndup(line + pos, last - pos);
        LogDebug(LOG_MOD_PS, "'%s' field '%s'"
                 " extracted from between positions %d and %d",
                 names[field], fields[field], pos, last - 1);

        pos = last;
        field++;
    }

    MaybeFixStartTime(line, pstime, names, fields);

    return true;
}

/*******************************************************************/

static int GetProcColumnIndex(const char *name1, const char *name2, char **names)
{
    for (int i = 0; names[i] != NULL; i++)
    {
        if (strcmp(names[i], name1) == 0 ||
            strcmp(names[i], name2) == 0)
        {
            return i;
        }
    }

    LogDebug(LOG_MOD_PS, "Process column %s/%s"
             " was not supported on this system",
             name1, name2);

    return -1;
}

/**********************************************************************************/

bool IsProcessNameRunning(char *procNameRegex)
{
    char *colHeaders[CF_PROCCOLS];
    int start[CF_PROCCOLS] = { 0 };
    int end[CF_PROCCOLS] = { 0 };
    bool matched = false;
    int i;

    memset(colHeaders, 0, sizeof(colHeaders));

    if (PROCESSTABLE == NULL)
    {
        Log(LOG_LEVEL_ERR, "IsProcessNameRunning: PROCESSTABLE is empty");
        return false;
    }
    /* TODO: use actual time of ps-run, not time(NULL), which may be later. */
    time_t pstime = time(NULL);

    GetProcessColumnNames(PROCESSTABLE->name, colHeaders, start, end);

    for (const Item *ip = PROCESSTABLE->next; !matched && ip != NULL; ip = ip->next) // iterate over ps lines
    {
        char *lineSplit[CF_PROCCOLS];
        memset(lineSplit, 0, sizeof(lineSplit));

        if (NULL_OR_EMPTY(ip->name))
        {
            continue;
        }

        if (!SplitProcLine(ip->name, pstime, colHeaders, start, end,
                           PS_COLUMN_ALGORITHM[VPSHARDCLASS], lineSplit))
        {
            Log(LOG_LEVEL_ERR, "IsProcessNameRunning: Could not split process line '%s'", ip->name);
            goto loop_cleanup;
        }

        ApplyPlatformExtraTable(colHeaders, lineSplit);

        if (SelectProcRegexMatch("CMD", "COMMAND", procNameRegex, true, colHeaders, lineSplit))
        {
            matched = true;
        }

   loop_cleanup:
        for (i = 0; lineSplit[i] != NULL; i++)
        {
            free(lineSplit[i]);
        }
    }

    for (i = 0; colHeaders[i] != NULL; i++)
    {
        free(colHeaders[i]);
    }

    return matched;
}


static void GetProcessColumnNames(const char *proc, char **names, int *start, int *end)
{
    char title[16];
    int col, offset = 0;

    if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
    {
        LogDebug(LOG_MOD_PS, "Parsing ps line: '%s'", proc);
        // Makes the entry line up with the line above.
        PrintStringIndexLine(18, strlen(proc));
    }

    for (col = 0; col < CF_PROCCOLS; col++)
    {
        start[col] = end[col] = -1;
        names[col] = NULL;
    }

    col = 0;

    for (const char *sp = proc; *sp != '\0'; sp++)
    {
        offset = sp - proc;

        if (isspace((unsigned char) *sp))
        {
            if (start[col] != -1)
            {
                LogDebug(LOG_MOD_PS, "End of '%s' is %d", title, offset - 1);
                end[col++] = offset - 1;
                if (col >= CF_PROCCOLS) /* No space for more columns. */
                {
                    size_t blank = strspn(sp, " \t\r\n\f\v");
                    if (sp[blank]) /* i.e. that wasn't everything. */
                    {
                        /* If this happens, we have more columns in
                         * our ps output than space to store them.
                         * Update the #define CF_PROCCOLS (last seen
                         * in libpromises/cf3.defs.h) to a bigger
                         * number ! */
                        Log(LOG_LEVEL_ERR,
                            "Process table lacks space for last columns: %s",
                            sp + blank);
                    }
                    break;
                }
            }
        }
        else if (start[col] == -1)
        {
            if (col == 0)
            {
                // The first column always extends all the way to the left.
                start[col] = 0;
            }
            else
            {
                start[col] = offset;
            }

            if (sscanf(sp, "%15s", title) == 1)
            {
                LogDebug(LOG_MOD_PS, "Start of '%s' is at offset: %d",
                         title, offset);
                LogDebug(LOG_MOD_PS, "Col[%d] = '%s'", col, title);

                names[col] = xstrdup(title);
            }
        }
    }

    if (end[col] == -1)
    {
        LogDebug(LOG_MOD_PS, "End of '%s' is %d", title, offset);
        end[col] = offset;
    }
}

#ifndef _WIN32
static const char *GetProcessOptions(void)
{

# ifdef __linux__
    if (strncmp(VSYSNAME.release, "2.4", 3) == 0)
    {
        // No threads on 2.4 kernels, so omit nlwp
        return "-eo user,pid,ppid,pgid,pcpu,pmem,vsz,ni,rss:9,stime,etime,time,args";
    }
# endif

    return VPSOPTS[VPSHARDCLASS];
}
#endif

static int ExtractPid(char *psentry, char **names, int *end)
{
    int offset = 0;

    for (int col = 0; col < CF_PROCCOLS; col++)
    {
        if (strcmp(names[col], "PID") == 0)
        {
            if (col > 0)
            {
                offset = end[col - 1];
            }
            break;
        }
    }

    for (const char *sp = psentry + offset; *sp != '\0'; sp++) /* if first field contains alpha, skip */
    {
        /* If start with alphanum then skip it till the first space */

        if (isalnum((unsigned char) *sp))
        {
            while (*sp != ' ' && *sp != '\0')
            {
                sp++;
            }
        }

        while (*sp == ' ' || *sp == '\t')
        {
            sp++;
        }

        int pid;
        if (sscanf(sp, "%d", &pid) == 1 && pid != -1)
        {
            return pid;
        }
    }

    return -1;
}

# ifndef _WIN32
# ifdef HAVE_GETZONEID
/* ListLookup with the following return semantics
 * -1 if the first argument is smaller than the second
 *  0 if the arguments are equal
 *  1 if the first argument is bigger than the second
 */
int PidListCompare(const void *pid1, const void *pid2, ARG_UNUSED void *user_data)
{
    int p1 = (intptr_t)(void *)pid1;
    int p2 = (intptr_t)(void *)pid2;

    if (p1 < p2)
    {
        return -1;
    }
    else if (p1 > p2)
    {
        return 1;
    }
    return 0;
}
/* Load processes using zone-aware ps
 * to obtain solaris list of global
 * process ids for root and non-root
 * users to lookup later */
int ZLoadProcesstable(Seq *pidlist, Seq *rootpidlist)
{

    char *names[CF_PROCCOLS];
    int start[CF_PROCCOLS];
    int end[CF_PROCCOLS];

    const char *pscmd = "/usr/bin/ps -Aleo zone,user,pid";

    FILE *psf = cf_popen(pscmd, "r", false);
    if (psf == NULL)
    {
        Log(LOG_LEVEL_ERR, "ZLoadProcesstable: Couldn't open the process list with command %s.", pscmd);
        return false;
    }

    size_t pbuff_size = CF_BUFSIZE;
    char *pbuff = xmalloc(pbuff_size);
    bool header = true;

    while (true)
    {
        ssize_t res = CfReadLine(&pbuff, &pbuff_size, psf);
        if (res == -1)
        {
            if (!feof(psf))
            {
                Log(LOG_LEVEL_ERR, "IsGlobalProcess(char **, int): Unable to read process list with command '%s'. (fread: %s)", pscmd, GetErrorStr());
                cf_pclose(psf);
                free(pbuff);
                return false;
            }
            else
            {
                break;
            }
        }
        Chop(pbuff, pbuff_size);
        if (header) /* This line is the header. */
        {
            GetProcessColumnNames(pbuff, &names[0], start, end);
        }
        else
        {
            int pid = ExtractPid(pbuff, &names[0], end);

            size_t zone_offset = strspn(pbuff, " ");
            size_t zone_end_offset = strcspn(pbuff + zone_offset, " ") + zone_offset;
            size_t user_offset = strspn(pbuff + zone_end_offset, " ") + zone_end_offset;
            size_t user_end_offset = strcspn(pbuff + user_offset, " ") + user_offset;
            bool is_global = (zone_end_offset - zone_offset == 6
                                  && strncmp(pbuff + zone_offset, "global", 6) == 0);
            bool is_root = (user_end_offset - user_offset == 4
                                && strncmp(pbuff + user_offset, "root", 4) == 0);

            if (is_global && is_root)
            {
                SeqAppend(rootpidlist, (void*)(intptr_t)pid);
            }
            else if (is_global && !is_root)
            {
                SeqAppend(pidlist, (void*)(intptr_t)pid);
            }
        }

        header = false;
    }
    cf_pclose(psf);
    free(pbuff);
    return true;
}
bool PidInSeq(Seq *list, int pid)
{
    void *res = SeqLookup(list, (void *)(intptr_t)pid, PidListCompare);
    int result = (intptr_t)(void*)res;

    if (result == pid)
    {
        return true;
    }
    return false;
}
/* return true if the process with
 * pid is in the global zone */
int IsGlobalProcess(int pid, Seq *pidlist, Seq *rootpidlist)
{
    if (PidInSeq(pidlist, pid) || PidInSeq(rootpidlist, pid))
    {
       return true;
    }
    else
    {
       return false;
    }
}
void ZCopyProcessList(Item **dest, const Item *source, Seq *pidlist, char **names, int *end)
{
    int gpid = ExtractPid(source->name, names, end);

    if (PidInSeq(pidlist, gpid))
    {
        PrependItem(dest, source->name, "");
    }
}
# endif /* HAVE_GETZONEID */

static void CheckPsLineLimitations(void)
{
#ifdef __hpux
    FILE *ps_fd;
    int ret;
    char limit[21];
    char *buf = NULL;
    size_t bufsize = 0;

    ps_fd = fopen("/etc/default/ps", "r");
    if (!ps_fd)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not open '/etc/default/ps' "
            "to check ps line length limitations.");
        return;
    }

    while (true)
    {
        ret = CfReadLine(&buf, &bufsize, ps_fd);
        if (ret < 0)
        {
            break;
        }

        ret = sscanf(buf, "DEFAULT_CMD_LINE_WIDTH = %20[0-9]", limit);

        if (ret == 1)
        {
            if (atoi(limit) < 1024)
            {
                Log(LOG_LEVEL_VERBOSE, "ps line length limit is less than 1024. "
                    "Consider adjusting the DEFAULT_CMD_LINE_WIDTH setting in /etc/default/ps "
                    "in order to guarantee correct process matching.");
            }
            break;
        }
    }

    free(buf);
    fclose(ps_fd);
#endif // __hpux
}
#endif // _WIN32

const char *GetProcessTableLegend(void)
{
    if (PROCESSTABLE)
    {
        // First entry in the table is legend.
        return PROCESSTABLE->name;
    }
    else
    {
        return "<Process table not loaded>";
    }
}

#if defined(__sun) || defined(TEST_UNIT_TEST)
static FILE *OpenUcbPsPipe(void)
{
    for (int i = 0; UCB_STYLE_PS[i]; i++)
    {
        struct stat statbuf;
        if (stat(UCB_STYLE_PS[i], &statbuf) < 0)
        {
            Log(LOG_LEVEL_VERBOSE,
                "%s not found, cannot be used for extra process information",
                UCB_STYLE_PS[i]);
            continue;
        }
        if (!(statbuf.st_mode & 0111))
        {
            Log(LOG_LEVEL_VERBOSE,
                "%s not executable, cannot be used for extra process information",
                UCB_STYLE_PS[i]);
            continue;
        }

        char *ps_cmd;
        xasprintf(&ps_cmd, "%s %s", UCB_STYLE_PS[i],
                  UCB_STYLE_PS_ARGS);

        FILE *cmd = cf_popen(ps_cmd, "rt", false);
        if (!cmd)
        {
            Log(LOG_LEVEL_WARNING, "Could not execute \"%s\", extra process "
                "information not available. "
                "Process command line length may be limited to 80 characters.",
                ps_cmd);
        }

        free(ps_cmd);

        return cmd;
    }

    Log(LOG_LEVEL_VERBOSE, "No eligible tool for extra process information "
        "found. Skipping.");

    return NULL;
}

static void ReadFromUcbPsPipe(FILE *cmd)
{
    char *names[CF_PROCCOLS];
    memset(names, 0, sizeof(names));
    int start[CF_PROCCOLS];
    int end[CF_PROCCOLS];
    char *line = NULL;
    size_t linesize = 0;
    bool header = true;
    time_t pstime = time(NULL);
    int pidcol = -1;
    int cmdcol = -1;
    while (CfReadLine(&line, &linesize, cmd) > 0)
    {
        if (header)
        {
            GetProcessColumnNames(line, names, start, end);

            for (int i = 0; names[i]; i++)
            {
                if (strcmp(names[i], "PID") == 0)
                {
                    pidcol = i;
                }
                else if (strcmp(names[i], "COMMAND") == 0
                           || strcmp(names[i], "CMD") == 0)
                {
                    cmdcol = i;
                }
            }

            if (pidcol < 0 || cmdcol < 0)
            {
                Log(LOG_LEVEL_ERR,
                    "Could not find PID and/or CMD/COMMAND column in "
                    "ps output: \"%s\"", line);
                break;
            }

            header = false;
            continue;
        }


        char *columns[CF_PROCCOLS];
        memset(columns, 0, sizeof(columns));
        if (!SplitProcLine(line, pstime, names, start, end,
                           UCB_STYLE_PS_COLUMN_ALGORITHM, columns))
        {
            Log(LOG_LEVEL_WARNING,
                "Not able to parse ps output: \"%s\"", line);
        }

        StringMapInsert(UCB_PS_MAP, columns[pidcol], columns[cmdcol]);
        // We avoid strdup'ing these strings by claiming ownership here.
        columns[pidcol] = NULL;
        columns[cmdcol] = NULL;

        for (int i = 0; i < CF_PROCCOLS; i++)
        {
            // There may be some null entries here, but since we "steal"
            // strings in the section above, we may have set some of them to
            // NULL and there may be following non-NULL fields.
            free(columns[i]);
        }
    }

    if (!feof(cmd) && ferror(cmd))
    {
        Log(LOG_LEVEL_ERR, "Error while reading output from ps: %s",
            GetErrorStr());
    }

    for (int i = 0; names[i] && i < CF_PROCCOLS; i++)
    {
        free(names[i]);
    }

    free(line);
}

static void ClearPlatformExtraTable(void)
{
    if (UCB_PS_MAP)
    {
        StringMapDestroy(UCB_PS_MAP);
        UCB_PS_MAP = NULL;
    }
}

static void LoadPlatformExtraTable(void)
{
    if (UCB_PS_MAP)
    {
        return;
    }

    UCB_PS_MAP = StringMapNew();

    FILE *cmd = OpenUcbPsPipe();
    if (!cmd)
    {
        return;
    }
    ReadFromUcbPsPipe(cmd);
    if (cf_pclose(cmd) != 0)
    {
        Log(LOG_LEVEL_WARNING, "Command returned non-zero while gathering "
            "extra process information.");
        // Make an empty map, in this case. The information can't be trusted.
        StringMapClear(UCB_PS_MAP);
    }
}

static void ApplyPlatformExtraTable(char **names, char **columns)
{
    int pidcol = -1;

    for (int i = 0; names[i] && columns[i]; i++)
    {
        if (strcmp(names[i], "PID") == 0)
        {
            pidcol = i;
            break;
        }
    }

    if (pidcol == -1 || !StringMapHasKey(UCB_PS_MAP, columns[pidcol]))
    {
        return;
    }

    for (int i = 0; names[i] && columns[i]; i++)
    {
        if (strcmp(names[i], "COMMAND") == 0 || strcmp(names[i], "CMD") == 0)
        {
            free(columns[i]);
            columns[i] = xstrdup(StringMapGet(UCB_PS_MAP, columns[pidcol]));
            break;
        }
    }
}

#else
static inline void LoadPlatformExtraTable(void)
{
}

static inline void ClearPlatformExtraTable(void)
{
}

static inline void ApplyPlatformExtraTable(ARG_UNUSED char **names, ARG_UNUSED char **columns)
{
}
#endif

#ifndef _WIN32
int LoadProcessTable()
{
    FILE *prp;
    char pscomm[CF_MAXLINKSIZE];
    Item *rootprocs = NULL;
    Item *otherprocs = NULL;


    if (PROCESSTABLE)
    {
        Log(LOG_LEVEL_VERBOSE, "Reusing cached process table");
        return true;
    }

    LoadPlatformExtraTable();

    CheckPsLineLimitations();

    const char *psopts = GetProcessOptions();

    snprintf(pscomm, CF_MAXLINKSIZE, "%s %s", VPSCOMM[VPSHARDCLASS], psopts);

    Log(LOG_LEVEL_VERBOSE, "Observe process table with %s", pscomm);

    if ((prp = cf_popen(pscomm, "r", false)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open the process list with command '%s'. (popen: %s)", pscomm, GetErrorStr());
        return false;
    }

    size_t vbuff_size = CF_BUFSIZE;
    char *vbuff = xmalloc(vbuff_size);

# ifdef HAVE_GETZONEID

    char *names[CF_PROCCOLS];
    int start[CF_PROCCOLS];
    int end[CF_PROCCOLS];
    Seq *pidlist = SeqNew(1, NULL);
    Seq *rootpidlist = SeqNew(1, NULL);
    bool global_zone = IsGlobalZone();

    if (global_zone)
    {
        int res = ZLoadProcesstable(pidlist, rootpidlist);

        if (res == false)
        {
            Log(LOG_LEVEL_ERR, "Unable to load solaris zone process table.");
            return false;
        }
    }

# endif

    ARG_UNUSED bool header = true;           /* used only if HAVE_GETZONEID */

    for (;;)
    {
        ssize_t res = CfReadLine(&vbuff, &vbuff_size, prp);
        if (res == -1)
        {
            if (!feof(prp))
            {
                Log(LOG_LEVEL_ERR, "Unable to read process list with command '%s'. (fread: %s)", pscomm, GetErrorStr());
                cf_pclose(prp);
                free(vbuff);
                return false;
            }
            else
            {
                break;
            }
        }
        Chop(vbuff, vbuff_size);

# ifdef HAVE_GETZONEID

        if (global_zone)
        {
            if (header)
            {   /* this is the banner so get the column header names for later use*/
                GetProcessColumnNames(vbuff, &names[0], start, end);
            }
            else
            {
               int gpid = ExtractPid(vbuff, names, end);

               if (!IsGlobalProcess(gpid, pidlist, rootpidlist))
               {
                    continue;
               }
            }
        }

# endif
        AppendItem(&PROCESSTABLE, vbuff, "");

        header = false;
    }

    cf_pclose(prp);

/* Now save the data */
    const char* const statedir = GetStateDir();

    snprintf(vbuff, CF_MAXVARSIZE, "%s%ccf_procs", statedir, FILE_SEPARATOR);
    RawSaveItemList(PROCESSTABLE, vbuff, NewLineMode_Unix);

# ifdef HAVE_GETZONEID
    if (global_zone) /* pidlist and rootpidlist are empty if we're not in the global zone */
    {
        Item *ip = PROCESSTABLE;
        while (ip != NULL)
        {
            ZCopyProcessList(&rootprocs, ip, rootpidlist, names, end);
            ip = ip->next;
        }
        ReverseItemList(rootprocs);
        ip = PROCESSTABLE;
        while (ip != NULL)
        {
            ZCopyProcessList(&otherprocs, ip, pidlist, names, end);
            ip = ip->next;
        }
        ReverseItemList(otherprocs);
    }
    else
# endif
    {
        CopyList(&rootprocs, PROCESSTABLE);
        CopyList(&otherprocs, PROCESSTABLE);

        while (DeleteItemNotContaining(&rootprocs, "root"))
        {
        }

        while (DeleteItemContaining(&otherprocs, "root"))
        {
        }
    }
    if (otherprocs)
    {
        PrependItem(&rootprocs, otherprocs->name, NULL);
    }

    snprintf(vbuff, CF_MAXVARSIZE, "%s%ccf_rootprocs", statedir, FILE_SEPARATOR);
    RawSaveItemList(rootprocs, vbuff, NewLineMode_Unix);
    DeleteItemList(rootprocs);

    snprintf(vbuff, CF_MAXVARSIZE, "%s%ccf_otherprocs", statedir, FILE_SEPARATOR);
    RawSaveItemList(otherprocs, vbuff, NewLineMode_Unix);
    DeleteItemList(otherprocs);

    free(vbuff);
    return true;
}
# endif

void ClearProcessTable(void)
{
    ClearPlatformExtraTable();

    DeleteItemList(PROCESSTABLE);
    PROCESSTABLE = NULL;
}
