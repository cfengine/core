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


#include <history.h>

#include <monitoring.h>                                      /* MakeTimekey */
#include <actuator.h>
#include <promises.h>
#include <ornaments.h>
#include <locks.h>
#include <policy.h>
#include <vars.h>
#include <known_dirs.h>
#include <sysinfo.h>
#include <unix.h>
#include <scope.h>
#include <eval_context.h>
#include <item_lib.h>
#include <exec_tools.h>
#include <conversion.h>
#include <instrumentation.h>
#include <files_interfaces.h>
#include <pipes.h>
#include <matching.h>
#include <string_lib.h>
#include <regex.h>                                       /* StringMatchFull */
#include <timeout.h>
#include <constants.h>
#include <time_classes.h>
#include <file_lib.h>
#include <assert.h>


#define CF_DUNBAR_WORK 30


typedef struct
{
    char *path;
    Item *output;
} CustomMeasurement;

static int MONITOR_RESTARTED = true;
static CustomMeasurement ENTERPRISE_DATA[CF_DUNBAR_WORK];

static void PutRecordForTime(CF_DB *db, time_t time, const Averages *values)
{
    char timekey[CF_MAXVARSIZE];

    MakeTimekey(time, timekey);

    WriteDB(db, timekey, values, sizeof(Averages));
}

static void Nova_SaveFilePosition(const char *handle, char *name, long fileptr)
{
    CF_DB *dbp;
    char *key = StringConcatenate(2, handle, name);

    if (!OpenDB(&dbp, dbid_static))
    {
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Saving state for %s at %ld", key, fileptr);
    WriteDB(dbp, key, &fileptr, sizeof(long));
    CloseDB(dbp);
    free(key);
}

static long Nova_RestoreFilePosition(const char *handle, char *name)
{
    CF_DB *dbp;
    long fileptr;
    char *key = StringConcatenate(2, handle, name);

    if (!OpenDB(&dbp, dbid_static))
    {
        return 0L;
    }

    ReadDB(dbp, key, &fileptr, sizeof(long));
    Log(LOG_LEVEL_VERBOSE, "Resuming state for %s at %ld", key, fileptr);
    CloseDB(dbp);
    free(key);
    return fileptr;
}

static void Nova_DumpSlowlyVaryingObservations(void)
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    FILE *fout;
    char *key;
    void *stored;
    int ksize, vsize;
    char name[CF_BUFSIZE];

    if (!OpenDB(&dbp, dbid_static))
    {
        return;
    }

    snprintf(name, CF_BUFSIZE - 1, "%s%cstatic_data", GetStateDir(), FILE_SEPARATOR);

    if ((fout = fopen(name, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to save discovery data in '%s'. (fopen: %s)", name, GetErrorStr());
        CloseDB(dbp);
        return;
    }

/* Acquire a cursor for the database. */

    if (!NewDBCursor(dbp, &dbcp))
    {
        Log(LOG_LEVEL_INFO, "Unable to scan class db");
        CloseDB(dbp);
        return;
    }

    while (NextDB(dbcp, &key, &ksize, &stored, &vsize))
    {
        char buf[CF_MAXVARSIZE], lval[CF_MAXVARSIZE], rval[CF_BUFSIZE];

        strncpy(buf, key, CF_MAXVARSIZE - 1);

        sscanf(buf, "%s:", lval);

        if (stored != NULL)
        {
            strncpy(rval, stored, CF_BUFSIZE - 1);
            fprintf(fout, "%s:%s\n", lval, rval);
        }
    }

    DeleteDBCursor(dbcp);
    CloseDB(dbp);
    fclose(fout);
}

static void Nova_HistoryUpdate(time_t time, const Averages *newvals)
{
    CF_DB *dbp;

    if (!OpenDB(&dbp, dbid_history))
    {
        return;
    }

    PutRecordForTime(dbp, time, newvals);

    CloseDB(dbp);
}

static Item *NovaReSample(EvalContext *ctx, int slot, Attributes a, const Promise *pp, PromiseResult *result)
{
    CfLock thislock;
    char eventname[CF_BUFSIZE];
    struct timespec start;
    FILE *fin = NULL;
    mode_t maskval = 0;
    const char *handle = PromiseGetHandle(pp);

    if (a.measure.stream_type && strcmp(a.measure.stream_type, "pipe") == 0)
    {
        if (!IsExecutable(CommandArg0(pp->promiser)))
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "%s promises to be executable but isn't\n", pp->promiser);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            return NULL;
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Promiser string contains a valid executable (%s) - ok", CommandArg0(pp->promiser));
        }
    }

    TransactionContext tc = {
        .expireafter = a.transaction.expireafter,
        .ifelapsed = MONITOR_RESTARTED ? 0 : a.transaction.ifelapsed, // Force a measurement if restarted
    };

    CFSTARTTIME = time(NULL);

    thislock = AcquireLock(ctx, pp->promiser, VUQNAME, CFSTARTTIME, tc, pp, false);

    if (thislock.lock == NULL)
    {
        if (strcmp(a.measure.history_type, "log") == 0)
        {
            DeleteItemList(ENTERPRISE_DATA[slot].output);
            ENTERPRISE_DATA[slot].output = NULL;
        }
        else
        {
            /* If static or time-series, and too soon or busy then use a cached value
               to avoid artificial gaps in the history */
        }

        MONITOR_RESTARTED = false;
        return ENTERPRISE_DATA[slot].output;
    }
    else
    {
        DeleteItemList(ENTERPRISE_DATA[slot].output);
        ENTERPRISE_DATA[slot].output = NULL;

        Log(LOG_LEVEL_INFO, "Sampling \'%s\' ...(timeout=%d,owner=%ju,group=%ju)", pp->promiser, a.contain.timeout,
              (uintmax_t)a.contain.owner, (uintmax_t)a.contain.group);

        start = BeginMeasure();

        if (a.contain.timeout != 0)
        {
            SetTimeOut(a.contain.timeout);
        }

        /* Stream types */

        if (a.measure.stream_type && strcmp(a.measure.stream_type, "file") == 0)
        {
            long filepos = 0;
            struct stat sb;

            Log(LOG_LEVEL_VERBOSE, "Stream \"%s\" is a plain file", pp->promiser);

            if (stat(pp->promiser, &sb) == -1)
            {
                Log(LOG_LEVEL_INFO, "Unable to find stream '%s'. (stat: %s)",
                    pp->promiser, GetErrorStr());
                YieldCurrentLock(thislock);
                MONITOR_RESTARTED = false;

                return NULL;
            }

            fin = safe_fopen(pp->promiser, "r");

            if (a.measure.growing)
            {
                filepos = Nova_RestoreFilePosition(handle, pp->promiser);

                if (sb.st_size >= filepos)
                {
                    fseek(fin, filepos, SEEK_SET);
                }
            }
        }
        else if (a.measure.stream_type && strcmp(a.measure.stream_type, "pipe") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "(Setting pipe umask to %jo)", (uintmax_t)a.contain.umask);
            maskval = umask(a.contain.umask);

            if (a.contain.umask == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Programming %s running with umask 0! Use umask= to set", pp->promiser);
            }


            // Mark: This is strange that we used these wrappers. Currently no way of setting these
            a.contain.owner = -1;
            a.contain.group = -1;
            a.contain.chdir = NULL;
            a.contain.chroot = NULL;
            // Mark: they were unset, and would fail for non-root(!)

            if (a.contain.shelltype == SHELL_TYPE_POWERSHELL)
            {
#ifdef __MINGW32__
                fin =
                    cf_popen_powershell_setuid(pp->promiser, "r", a.contain.owner, a.contain.group, a.contain.chdir,
                                      a.contain.chroot, false);
#else // !__MINGW32__
                Log(LOG_LEVEL_ERR, "Powershell is only supported on Windows");
                YieldCurrentLock(thislock);
                MONITOR_RESTARTED = false;
                return NULL;
#endif // !__MINGW32__
            }
            else if (a.contain.shelltype == SHELL_TYPE_USE)
            {
                fin =
                    cf_popen_shsetuid(pp->promiser, "r", a.contain.owner, a.contain.group, a.contain.chdir,
                                      a.contain.chroot, false);
            }
            else
            {
                fin =
                    cf_popensetuid(pp->promiser, "r", a.contain.owner, a.contain.group, a.contain.chdir,
                                   a.contain.chroot, false);
            }
        }

        /* generic file stream */

        if (fin == NULL)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "Couldn't open pipe to command '%s'. (cf_popen: %s)", pp->promiser, GetErrorStr());
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            YieldCurrentLock(thislock);
            MONITOR_RESTARTED = false;
            return ENTERPRISE_DATA[slot].output;
        }

        size_t line_size = CF_BUFSIZE;
        char *line = xmalloc(line_size);

        for (;;)
        {
            ssize_t res = CfReadLine(&line, &line_size, fin);
            if (res == -1)
            {
                if (!feof(fin))
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_TIMEOUT, pp, a, "Sample stream '%s'. (fread: %s)",
                         pp->promiser, GetErrorStr());
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_TIMEOUT);
                    YieldCurrentLock(thislock);
                    free(line);
                    return ENTERPRISE_DATA[slot].output;
                }
                else
                {
                    break;
                }
            }

            AppendItem(&(ENTERPRISE_DATA[slot].output), line, NULL);
            Log(LOG_LEVEL_INFO, "Sampling => %s", line);
        }

        free(line);

        if (a.measure.stream_type && strcmp(a.measure.stream_type, "file") == 0)
        {
            long fileptr = ftell(fin);

            fclose(fin);
            Nova_SaveFilePosition(handle, pp->promiser, fileptr);
        }
        else if (a.measure.stream_type && strcmp(a.measure.stream_type, "pipe") == 0)
        {
            cf_pclose(fin);
        }
    }

    if (a.contain.timeout != 0)
    {
        alarm(0);
        signal(SIGALRM, SIG_DFL);
    }

    Log(LOG_LEVEL_INFO, "Collected sample of %s", pp->promiser);
    umask(maskval);
    YieldCurrentLock(thislock);
    MONITOR_RESTARTED = false;

    snprintf(eventname, CF_BUFSIZE - 1, "Sample(%s)", pp->promiser);
    EndMeasure(eventname, start);
    return ENTERPRISE_DATA[slot].output;
}

void HistoryUpdate(EvalContext *ctx, const Averages *const newvals)
{
    CfLock thislock;
    time_t now = time(NULL);

/* We do this only once per hour - this should not be changed */

    Banner("Update long-term history");

    Policy *history_db_policy = PolicyNew();
    Promise *pp = NULL;
    {
        Bundle *bp = PolicyAppendBundle(history_db_policy, NamespaceDefault(), "history_db_bundle", "agent", NULL, NULL);
        PromiseType *tp = BundleAppendPromiseType(bp, "history_db");

        pp = PromiseTypeAppendPromise(tp, "the long term memory", (Rval) { NULL, RVAL_TYPE_NOPROMISEE }, NULL, NULL);
    }
    assert(pp);

    TransactionContext tc = {
        .expireafter = 0,
        .ifelapsed = 59
    };

    thislock = AcquireLock(ctx, pp->promiser, VUQNAME, now, tc, pp, false);

    if (thislock.lock == NULL)
    {
        PolicyDestroy(history_db_policy);
        return;
    }

/* Refresh the class context of the agent */

    EvalContextClear(ctx);

    DetectEnvironment(ctx);
    time_t t = SetReferenceTime();
    UpdateTimeClasses(ctx, t);

    EvalContextHeapPersistentLoadAll(ctx);
    LoadSystemConstants(ctx);

    YieldCurrentLock(thislock);
    PolicyDestroy(history_db_policy);

    Nova_HistoryUpdate(CFSTARTTIME, newvals);

    Nova_DumpSlowlyVaryingObservations();
}

static Item *NovaGetMeasurementStream(EvalContext *ctx, Attributes a, const Promise *pp, PromiseResult *result)
{
    int i;

    for (i = 0; i < CF_DUNBAR_WORK; i++)
    {
        if (ENTERPRISE_DATA[i].path == NULL)
        {
            break;
        }

        if (strcmp(ENTERPRISE_DATA[i].path, pp->promiser) == 0)
        {
            ENTERPRISE_DATA[i].output = NovaReSample(ctx, i, a, pp, result);
            return ENTERPRISE_DATA[i].output;
        }
    }

    ENTERPRISE_DATA[i].path = xstrdup(pp->promiser);
    ENTERPRISE_DATA[i].output = NovaReSample(ctx, i, a, pp, result);
    return ENTERPRISE_DATA[i].output;
}

static PromiseResult NovaExtractValueFromStream(EvalContext *ctx, const char *handle,
                                                Item *stream, Attributes a,
                                                const Promise *pp, double *value_out)
{
    char value[CF_MAXVARSIZE];
    int count = 1, found = false, match_count = 0, done = false;
    double real_val = 0;
    Item *ip, *match = NULL;
    bool ok_conversion = true;

    for (ip = stream; ip != NULL; ip = ip->next)
    {
        if (count == a.measure.select_line_number)
        {
            found = true;
            match = ip;
            match_count++;
        }

        if (a.measure.select_line_matching && StringMatchFull(a.measure.select_line_matching, ip->name))
        {
            Log(LOG_LEVEL_VERBOSE, " ?? Look for %s regex %s", handle, a.measure.select_line_matching);
            found = true;
            match = ip;

            if (a.measure.extraction_regex)
            {
                switch (a.measure.data_type)
                {
                case CF_DATA_TYPE_INT:
                case CF_DATA_TYPE_REAL:
                case CF_DATA_TYPE_COUNTER:

                    strncpy(value, ExtractFirstReference(a.measure.extraction_regex, match->name), CF_MAXVARSIZE - 1);

                    if (strcmp(value, "CF_NOMATCH") == 0)
                    {
                        ok_conversion = false;
                        Log(LOG_LEVEL_VERBOSE, "Was not able to match a value with '%s' on '%s'",
                            a.measure.extraction_regex, match->name);
                    }
                    else
                    {
                        if (ok_conversion)
                        {
                            Log(LOG_LEVEL_VERBOSE, "Found candidate match value of '%s'", value);

                            if (a.measure.policy == MEASURE_POLICY_SUM || a.measure.policy == MEASURE_POLICY_AVERAGE)
                            {
                                double delta = 0;
                                if (DoubleFromString(value, &delta))
                                {
                                    real_val += delta;
                                }
                                else
                                {
                                    Log(LOG_LEVEL_ERR, "Error in double conversion from string value: %s", value);
                                    return false;
                                }
                            }
                            else
                            {
                                if (!DoubleFromString(value, &real_val))
                                {
                                    Log(LOG_LEVEL_ERR, "Error in double conversion from string value: %s", value);
                                    return false;
                                }
                            }

                            match_count++;

                            if (a.measure.policy == MEASURE_POLICY_FIRST)
                            {
                                done = true;
                            }
                        }
                    }
                    break;

                default:
                    Log(LOG_LEVEL_ERR, "Unexpected data type in data_type attribute: %d", a.measure.data_type);
                }
            }

        }

        count++;

        if (done)
        {
            break;
        }
    }

    if (!found)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Could not locate the line for promise '%s'", handle);
        *value_out = 0.0;
        return PROMISE_RESULT_FAIL;
    }

    switch (a.measure.data_type)
    {
    case CF_DATA_TYPE_COUNTER:

        real_val = (double) match_count;
        break;

    case CF_DATA_TYPE_INT:

        if (match_count > 1)
        {
            Log(LOG_LEVEL_INFO, "Warning: %d lines matched the line_selection \"%s\"- making best average",
                  match_count, a.measure.select_line_matching);
        }

        if (match_count > 0 && a.measure.policy == MEASURE_POLICY_AVERAGE) // If not "average" then "sum"
        {
            real_val /= match_count;
        }
        break;

    case CF_DATA_TYPE_REAL:

        if (match_count > 1)
        {
            Log(LOG_LEVEL_INFO, "Warning: %d lines matched the line_selection \"%s\"- making best average",
                  match_count, a.measure.select_line_matching);
        }

        if (match_count > 0)
        {
            real_val /= match_count;
        }

        break;

    default:
        Log(LOG_LEVEL_ERR, "Unexpected data type in data_type attribute: %d", a.measure.data_type);
    }

    if (!ok_conversion)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to extract a value from the matched line '%s'", match->name);
        PromiseRef(LOG_LEVEL_INFO, pp);
        *value_out = 0.0;
        return PROMISE_RESULT_FAIL;
    }

    Log(LOG_LEVEL_INFO, "Extracted value \"%f\" for promise \"%s\"", real_val, handle);
    *value_out = real_val;
    return PROMISE_RESULT_NOOP;
}

static void NovaLogSymbolicValue(EvalContext *ctx, const char *handle, Item *stream,
                                 Attributes a, const Promise *pp, PromiseResult *result)
{
    char value[CF_BUFSIZE], sdate[CF_MAXVARSIZE], filename[CF_BUFSIZE];
    int count = 1, found = false, match_count = 0;
    Item *ip, *match = NULL, *matches = NULL;
    time_t now = time(NULL);
    FILE *fout;

    if (stream == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "No stream to measure");
        return;
    }

    Log(LOG_LEVEL_VERBOSE, "Locate and log sample ...");

    for (ip = stream; ip != NULL; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        if (count == a.measure.select_line_number)
        {
            Log(LOG_LEVEL_VERBOSE, "Found line %d by number...", count);
            found = true;
            match_count = 1;
            match = ip;

            if (a.measure.extraction_regex)
            {
                Log(LOG_LEVEL_VERBOSE, "Now looking for a matching extractor \"%s\"", a.measure.extraction_regex);
                strncpy(value, ExtractFirstReference(a.measure.extraction_regex, match->name), CF_MAXVARSIZE - 1);
                Log(LOG_LEVEL_INFO, "Extracted value \"%s\" for promise \"%s\"", value, handle);
                AppendItem(&matches, value, NULL);
            }
            else
            {
                Log(LOG_LEVEL_INFO, "Using entire line \"%s\" for promise \"%s\"", match->name, handle);
                AppendItem(&matches, match->name, NULL);
            }
            break;
        }

        if (a.measure.select_line_matching && StringMatchFull(a.measure.select_line_matching, ip->name))
        {
            Log(LOG_LEVEL_VERBOSE, "Found line %d by pattern...", count);
            found = true;
            match = ip;
            match_count++;

            if (a.measure.extraction_regex)
            {
                Log(LOG_LEVEL_VERBOSE, "Now looking for a matching extractor \"%s\"", a.measure.extraction_regex);
                strncpy(value, ExtractFirstReference(a.measure.extraction_regex, match->name), CF_MAXVARSIZE - 1);
                Log(LOG_LEVEL_INFO, "Extracted value \"%s\" for promise \"%s\"", value, handle);
                AppendItem(&matches, value, NULL);
            }
            else
            {
                Log(LOG_LEVEL_INFO, "Using entire line \"%s\" for promise \"%s\"", match->name, handle);
                AppendItem(&matches, match->name, NULL);
            }
        }

        count++;
    }

    if (!found)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Promiser '%s' found no matching line.", pp->promiser);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return;
    }

    if (match_count > 1)
    {
        Log(LOG_LEVEL_INFO, "Warning: %d lines matched the line_selection \"%s\"- matching to last", match_count,
              a.measure.select_line_matching);
    }

    switch (a.measure.data_type)
    {
    case CF_DATA_TYPE_COUNTER:
        Log(LOG_LEVEL_VERBOSE, "Counted %d for %s", match_count, handle);
        snprintf(value, CF_MAXVARSIZE, "%d", match_count);
        break;

    case CF_DATA_TYPE_STRING_LIST:
        ItemList2CSV_bound(matches, value, sizeof(value), ',');
        break;

    default:
        snprintf(value, CF_BUFSIZE, "%s", matches->name);
    }

    DeleteItemList(matches);

    if (a.measure.history_type && strcmp(a.measure.history_type, "log") == 0)
    {
        snprintf(filename, CF_BUFSIZE, "%s%c%s_measure.log", GetStateDir(), FILE_SEPARATOR, handle);

        if ((fout = fopen(filename, "a")) == NULL)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to open the output log \"%s\"", filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return;
        }

        strncpy(sdate, ctime(&now), CF_MAXVARSIZE - 1);
        if (Chop(sdate, CF_EXPANDSIZE) == -1)
        {
            Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
        }

        fprintf(fout, "%s,%ld,%s\n", sdate, (long) now, value);
        Log(LOG_LEVEL_VERBOSE, "Logging: %s,%s to %s", sdate, value, filename);

        fclose(fout);
    }
    else                        // scalar or static
    {
        CF_DB *dbp;
        char id[CF_MAXVARSIZE];

        if (!OpenDB(&dbp, dbid_static))
        {
            return;
        }

        snprintf(id, CF_MAXVARSIZE - 1, "%s:%d", handle, a.measure.data_type);
        WriteDB(dbp, id, value, strlen(value) + 1);
        CloseDB(dbp);
    }
}

PromiseResult VerifyMeasurement(EvalContext *ctx, double *this,
                                Attributes a, const Promise *pp)
{
    const char *handle = PromiseGetHandle(pp);
    Item *stream = NULL;
    int slot = 0;
    double new_value;

    if (!handle)
    {
        Log(LOG_LEVEL_ERR, "The promised measurement has no handle to register it by.");
        return PROMISE_RESULT_NOOP;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Considering promise \"%s\"", handle);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    switch (a.measure.data_type)
    {
    case CF_DATA_TYPE_COUNTER:
    case CF_DATA_TYPE_INT:
    case CF_DATA_TYPE_REAL:

        /* First see if we can accommodate this measurement */
        Log(LOG_LEVEL_VERBOSE, "Promise '%s' is numerical in nature", handle);

        stream = NovaGetMeasurementStream(ctx, a, pp, &result);

        if (strcmp(a.measure.history_type, "weekly") == 0)
        {
            if ((slot = NovaRegisterSlot(handle, pp->comment ? pp->comment : "User defined measure",
                                         a.measure.units ? a.measure.units : "unknown", 0.0f, 100.0f, true)) < 0)
            {
                return result;
            }

            result = PromiseResultUpdate(result, NovaExtractValueFromStream(ctx, handle, stream, a, pp, &this[slot]));
            Log(LOG_LEVEL_VERBOSE, "Setting Nova slot %d=%s to %lf", slot, handle, this[slot]);
        }
        else if (strcmp(a.measure.history_type, "log") == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Promise to log a numerical value");
            NovaLogSymbolicValue(ctx, handle, stream, a, pp, &result);
        }
        else                    /* static */
        {
            Log(LOG_LEVEL_VERBOSE, "Promise to store a static numerical value");
            result = PromiseResultUpdate(result, NovaExtractValueFromStream(ctx, handle, stream, a, pp, &new_value));
            NovaNamedEvent(handle, new_value);
        }
        break;

    default:

        Log(LOG_LEVEL_VERBOSE, "Promise '%s' is symbolic in nature", handle);
        stream = NovaGetMeasurementStream(ctx, a, pp, &result);
        NovaLogSymbolicValue(ctx, handle, stream, a, pp, &result);
        break;
    }

    return result;

// stream gets de-allocated in ReSample
}
