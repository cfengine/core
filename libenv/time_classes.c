/*
   Copyright 2017 Northern.tech AS

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

static void RemoveTimeClass(EvalContext *ctx, time_t time)
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
        int i, j;
        char buf[CF_BUFSIZE];

        if (tz_tm[tz] == NULL)
        {
            Log(LOG_LEVEL_ERR, "Unable to parse passed time. (%s: %s)", tz_function[tz], GetErrorStr());
            return;
        }

/* Lifecycle */

        for( i = 0; i < 3; i++ )
        {
            snprintf(buf, CF_BUFSIZE, "%sLcycle_%d", tz_prefix[tz], i);
            EvalContextClassRemove(ctx, NULL, buf);
        }

/* Year */

        snprintf(buf, CF_BUFSIZE, "%sYr%04d", tz_prefix[tz], tz_parsed_time[tz].tm_year - 1 + 1900);
        EvalContextClassRemove(ctx, NULL, buf);
        snprintf(buf, CF_BUFSIZE, "%sYr%04d", tz_prefix[tz], tz_parsed_time[tz].tm_year + 1900);
        EvalContextClassRemove(ctx, NULL, buf);

/* Month */

        for( i = 0; i < 12; i++ )
        {
            snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], MONTH_TEXT[i]);
            EvalContextClassRemove(ctx, NULL, buf);
        }

/* Day of week */

        for( i = 0; i < 7; i++ )
        {
            snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], DAY_TEXT[i]);
            EvalContextClassRemove(ctx, NULL, buf);
        }

/* Day */

        for( i = 1; i < 32; i++ )
        {
            snprintf(buf, CF_BUFSIZE, "%sDay%d", tz_prefix[tz], i);
            EvalContextClassRemove(ctx, NULL, buf);
        }

/* Shift */

        for( i = 0; i < 4; i++ )
        {
            snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], SHIFT_TEXT[i]);
            EvalContextClassRemove(ctx, NULL, buf);
        }

/* Hour */

        for( i = 0; i < 24; i++ )
        {
            snprintf(buf, CF_BUFSIZE, "%sHr%02d", tz_prefix[tz], i);
            EvalContextClassRemove(ctx, NULL, buf);
            snprintf(buf, CF_BUFSIZE, "%sHr%d", tz_prefix[tz], i);
            EvalContextClassRemove(ctx, NULL, buf);
        }

/* Quarter */

        for( i = 1; i <= 4; i++ )
        {
            snprintf(buf, CF_BUFSIZE, "%sQ%d", tz_prefix[tz], i);
            EvalContextClassRemove(ctx, NULL, buf);
            for( j = 0; j < 24; j++ )
            {
                snprintf(buf, CF_BUFSIZE, "%sHr%02d_Q%d", tz_prefix[tz], j, i);
                EvalContextClassRemove(ctx, NULL, buf);
            }
        }

/* Minute */

        for( i = 0; i < 60; i++ )
        {
            snprintf(buf, CF_BUFSIZE, "%sMin%02d", tz_prefix[tz], i);
            EvalContextClassRemove(ctx, NULL, buf);
        }

        for( i = 0; i < 60; i += 5 )
        {
            snprintf(buf, CF_BUFSIZE, "%sMin%02d_%02d", tz_prefix[tz], i, (i + 5) % 60);
            EvalContextClassRemove(ctx, NULL, buf);
        }
    }
}

static void AddTimeClass(EvalContext *ctx, time_t time)
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
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

/* Year */

        snprintf(buf, CF_BUFSIZE, "%sYr%04d", tz_prefix[tz], tz_parsed_time[tz].tm_year + 1900);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

/* Month */

        snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], MONTH_TEXT[tz_parsed_time[tz].tm_mon]);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

/* Day of week */

/* Monday  is 1 in tm_wday, 0 in DAY_TEXT
   Tuesday is 2 in tm_wday, 1 in DAY_TEXT
   ...
   Sunday  is 0 in tm_wday, 6 in DAY_TEXT */
        day_text_index = (tz_parsed_time[tz].tm_wday + 6) % 7;
        snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], DAY_TEXT[day_text_index]);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

/* Day */

        snprintf(buf, CF_BUFSIZE, "%sDay%d", tz_prefix[tz], tz_parsed_time[tz].tm_mday);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

/* Shift */

        snprintf(buf, CF_BUFSIZE, "%s%s", tz_prefix[tz], SHIFT_TEXT[tz_parsed_time[tz].tm_hour / 6]);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

/* Hour */

        snprintf(buf, CF_BUFSIZE, "%sHr%02d", tz_prefix[tz], tz_parsed_time[tz].tm_hour);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");
        snprintf(buf, CF_BUFSIZE, "%sHr%d", tz_prefix[tz], tz_parsed_time[tz].tm_hour);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

/* Quarter */

        quarter = tz_parsed_time[tz].tm_min / 15 + 1;

        snprintf(buf, CF_BUFSIZE, "%sQ%d", tz_prefix[tz], quarter);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");
        snprintf(buf, CF_BUFSIZE, "%sHr%02d_Q%d", tz_prefix[tz], tz_parsed_time[tz].tm_hour, quarter);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

/* Minute */

        snprintf(buf, CF_BUFSIZE, "%sMin%02d", tz_prefix[tz], tz_parsed_time[tz].tm_min);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");

        interval_start = (tz_parsed_time[tz].tm_min / 5) * 5;
        interval_end = (interval_start + 5) % 60;

        snprintf(buf, CF_BUFSIZE, "%sMin%02d_%02d", tz_prefix[tz], interval_start, interval_end);
        EvalContextClassPutHard(ctx, buf, "time_based,source=agent");
    }
}

void UpdateTimeClasses(EvalContext *ctx, time_t t)
{
    RemoveTimeClass(ctx, t);
    AddTimeClass(ctx, t);
}

