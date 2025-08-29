/*
  Copyright 2024 Northern.tech AS

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

#include <time_classes.h>
#include <eval_context.h>
#include <alloc.h> // xvasprintf

static void RemoveTimeClass(EvalContext *ctx, const char* tags)
{
    Rlist *tags_rlist = RlistFromSplitString(tags, ',');
    ClassTableIterator *iter = EvalContextClassTableIteratorNewGlobal(ctx, NULL, true, true);
    StringSet *global_matches = ClassesMatching(ctx, iter, ".*", tags_rlist, false);
    ClassTableIteratorDestroy(iter);

    StringSetIterator it = StringSetIteratorInit(global_matches);
    const char *element = NULL;
    while ((element = StringSetIteratorNext(&it)))
    {
        EvalContextClassRemove(ctx, NULL, element);
    }

    StringSetDestroy(global_matches);

    RlistDestroy(tags_rlist);
}

static void InsertTimeClassF(StringMap *classes, const char *prefix, const char *type, const char *fmt, ...)
{
    char *key;
    xasprintf(&key, "time_based_%s%s", prefix, type);

    va_list args;
    va_start(args, fmt);

    char *value;
    NDEBUG_UNUSED int ret = xvasprintf(&value, fmt, args);
    assert(ret >= 0);

    va_end(args);
    StringMapInsert(classes, key, value);
}

StringMap *GetTimeClasses(time_t time) {
    // The first element is the local timezone
    const char* tz_prefix[2] = { "", "GMT_" };
    const char* tz_var_prefix[2] = { "", "gmt_" };
    const char* tz_function[2] = { "localtime_r", "gmtime_r" };
    struct tm tz_parsed_time[2];
    const struct tm* tz_tm[2] = {
        localtime_r(&time, &(tz_parsed_time[0])),
        gmtime_r(&time, &(tz_parsed_time[1]))
    };

    StringMap *classes = StringMapNew();

    for (int tz = 0; tz < 2; tz++)
    {
        int day_text_index, quarter, interval_start, interval_end;

        if (tz_tm[tz] == NULL)
        {
            Log(LOG_LEVEL_ERR, "Unable to parse passed time. (%s: %s)", tz_function[tz], GetErrorStr());
            return NULL;
        }

/* Lifecycle */
        InsertTimeClassF(classes, tz_var_prefix[tz], "lcycle", "%sLcycle_%d", tz_prefix[tz], ((tz_parsed_time[tz].tm_year + 1900) % 3));

/* Year */
        InsertTimeClassF(classes, tz_var_prefix[tz], "yr", "%sYr%04d", tz_prefix[tz], tz_parsed_time[tz].tm_year + 1900);

/* Month */

        InsertTimeClassF(classes, tz_var_prefix[tz], "month", "%s%s", tz_prefix[tz], MONTH_TEXT[tz_parsed_time[tz].tm_mon]);

/* Day of week */

/* Monday  is 1 in tm_wday, 0 in DAY_TEXT
   Tuesday is 2 in tm_wday, 1 in DAY_TEXT
   ...
   Sunday  is 0 in tm_wday, 6 in DAY_TEXT */
        day_text_index = (tz_parsed_time[tz].tm_wday + 6) % 7;
        InsertTimeClassF(classes, tz_var_prefix[tz], "dow", "%s%s", tz_prefix[tz], DAY_TEXT[day_text_index]);

/* Day */

        InsertTimeClassF(classes, tz_var_prefix[tz], "dom", "%sDay%d", tz_prefix[tz], tz_parsed_time[tz].tm_mday);

/* Shift */

        InsertTimeClassF(classes, tz_var_prefix[tz], "pod", "%s%s", tz_prefix[tz], SHIFT_TEXT[tz_parsed_time[tz].tm_hour / 6]);

/* Hour */

        InsertTimeClassF(classes, tz_var_prefix[tz], "hr", "%sHr%02d", tz_prefix[tz], tz_parsed_time[tz].tm_hour);
        InsertTimeClassF(classes, tz_var_prefix[tz], "hr_2", "%sHr%d", tz_prefix[tz], tz_parsed_time[tz].tm_hour);

/* Quarter */

        quarter = tz_parsed_time[tz].tm_min / 15 + 1;

        InsertTimeClassF(classes, tz_var_prefix[tz], "qoh", "%sQ%d", tz_prefix[tz], quarter);
        InsertTimeClassF(classes, tz_var_prefix[tz], "hr_qoh", "%sHr%02d_Q%d", tz_prefix[tz], tz_parsed_time[tz].tm_hour, quarter);

/* Minute */

        InsertTimeClassF(classes, tz_var_prefix[tz], "min", "%sMin%02d", tz_prefix[tz], tz_parsed_time[tz].tm_min);

        interval_start = (tz_parsed_time[tz].tm_min / 5) * 5;
        interval_end = (interval_start + 5) % 60;

        InsertTimeClassF(classes, tz_var_prefix[tz], "min_span_5", "%sMin%02d_%02d", tz_prefix[tz], interval_start, interval_end);
    }

    return classes;
}

static void AddTimeClass(EvalContext *ctx, time_t time, const char* tags)
{
    StringMap *time_classes = GetTimeClasses(time);
    if (time_classes == NULL) {
        return;
    }

    StringMapIterator iter = StringMapIteratorInit(time_classes);
    MapKeyValue *item;
    while ((item = StringMapIteratorNext(&iter)) != NULL) {
        EvalContextClassPutHard(ctx, item->value, tags);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, item->key, item->value, CF_DATA_TYPE_STRING, "noreport");
    }

    StringMapDestroy(time_classes);
}

void UpdateTimeClasses(EvalContext *ctx, time_t t)
{
    RemoveTimeClass(ctx, "cfengine_internal_time_based_autoremove");
    AddTimeClass(ctx, t, "time_based,cfengine_internal_time_based_autoremove,source=agent");
}
