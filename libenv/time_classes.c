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

#include <time_classes.h>
#include <eval_context.h>

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

static void AddTimeClass(EvalContext *ctx, time_t time, const char* tags)
{
    // The first element is the local timezone
    const char* tz_prefix[2] = { "", "GMT_" };
    const char* tz_function[2] = { "localtime_r", "gmtime_r" };
    struct tm tz_parsed_time[2];
    const struct tm* tz_tm[2] = {
        localtime_r(&time, &(tz_parsed_time[0])),
        gmtime_r(&time, &(tz_parsed_time[1]))
    };

    for (int tz = 0; tz < 2; tz++)
    {
        char buf[CF_BUFSIZE];
        int day_text_index, quarter, interval_start, interval_end;

        if (tz_tm[tz] == NULL)
        {
            Log(LOG_LEVEL_ERR, "Unable to parse passed time. (%s: %s)", tz_function[tz], GetErrorStr());
            return;
        }

/* Lifecycle */

        snprintf(buf, CF_BUFSIZE, "%sLcycle_%d", tz_prefix[tz], ((tz_parsed_time[tz].tm_year + 1900) % 3));
        EvalContextClassPutHard(ctx, buf, tags);

/* Year */

        snprintf(buf, CF_BUFSIZE, "%sYr%04d", tz_prefix[tz], tz_parsed_time[tz].tm_year + 1900);
        EvalContextClassPutHard(ctx, buf, tags);

/* Month */

        snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], MONTH_TEXT[tz_parsed_time[tz].tm_mon]);
        EvalContextClassPutHard(ctx, buf, tags);

/* Day of week */

/* Monday  is 1 in tm_wday, 0 in DAY_TEXT
   Tuesday is 2 in tm_wday, 1 in DAY_TEXT
   ...
   Sunday  is 0 in tm_wday, 6 in DAY_TEXT */
        day_text_index = (tz_parsed_time[tz].tm_wday + 6) % 7;
        snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], DAY_TEXT[day_text_index]);
        EvalContextClassPutHard(ctx, buf, tags);

/* Day */

        snprintf(buf, CF_BUFSIZE, "%sDay%d", tz_prefix[tz], tz_parsed_time[tz].tm_mday);
        EvalContextClassPutHard(ctx, buf, tags);

/* Shift */

        snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], SHIFT_TEXT[tz_parsed_time[tz].tm_hour / 6]);
        EvalContextClassPutHard(ctx, buf, tags);

/* Hour */

        snprintf(buf, CF_BUFSIZE, "%sHr%02d", tz_prefix[tz], tz_parsed_time[tz].tm_hour);
        EvalContextClassPutHard(ctx, buf, tags);
        snprintf(buf, CF_BUFSIZE, "%sHr%d", tz_prefix[tz], tz_parsed_time[tz].tm_hour);
        EvalContextClassPutHard(ctx, buf, tags);

/* Quarter */

        quarter = tz_parsed_time[tz].tm_min / 15 + 1;

        snprintf(buf, CF_BUFSIZE, "%sQ%d", tz_prefix[tz], quarter);
        EvalContextClassPutHard(ctx, buf, tags);
        snprintf(buf, CF_BUFSIZE, "%sHr%02d_Q%d", tz_prefix[tz], tz_parsed_time[tz].tm_hour, quarter);
        EvalContextClassPutHard(ctx, buf, tags);

/* Minute */

        snprintf(buf, CF_BUFSIZE, "%sMin%02d", tz_prefix[tz], tz_parsed_time[tz].tm_min);
        EvalContextClassPutHard(ctx, buf, tags);

        interval_start = (tz_parsed_time[tz].tm_min / 5) * 5;
        interval_end = (interval_start + 5) % 60;

        snprintf(buf, CF_BUFSIZE, "%sMin%02d_%02d", tz_prefix[tz], interval_start, interval_end);
        EvalContextClassPutHard(ctx, buf, tags);
    }
}

void UpdateTimeClasses(EvalContext *ctx, time_t t)
{
    RemoveTimeClass(ctx, "cfengine_internal_time_based_autoremove");
    AddTimeClass(ctx, t, "time_based,cfengine_internal_time_based_autoremove,source=agent");
}
