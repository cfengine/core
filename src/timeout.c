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

#include "cf3.defs.h"

#include "env_context.h"

/* Prototypes */

static void AddTimeClass(time_t time);

/*************************************************************************/

void SetTimeOut(int timeout)
{
    ALARM_PID = -1;
    signal(SIGALRM, (void *) TimeOut);
    alarm(timeout);
}

/*************************************************************************/

void TimeOut()
{
    alarm(0);

    if (ALARM_PID != -1)
    {
        CfOut(cf_verbose, "", "Time out of process %jd\n", (intmax_t)ALARM_PID);
        GracefulTerminate(ALARM_PID);
    }
    else
    {
        CfOut(cf_verbose, "", "%s> Time out\n", VPREFIX);
    }
}

/*************************************************************************/

void SetReferenceTime(int setclasses)
{
    time_t tloc;
    char vbuff[CF_BUFSIZE];

    if ((tloc = time((time_t *) NULL)) == -1)
    {
        CfOut(cf_error, "time", "Couldn't read system clock\n");
    }

    CFSTARTTIME = tloc;

    snprintf(vbuff, CF_BUFSIZE, "%s", cf_ctime(&tloc));

    CfOut(cf_verbose, "", "Reference time set to %s\n", cf_ctime(&tloc));

    if (setclasses)
    {
        AddTimeClass(tloc);
    }
}

/*******************************************************************/

void SetStartTime(void)
{
    time_t tloc;

    if ((tloc = time((time_t *) NULL)) == -1)
    {
        CfOut(cf_error, "time", "Couldn't read system clock\n");
    }

    CFINITSTARTTIME = tloc;

    CfDebug("Job start time set to %s\n", cf_ctime(&tloc));
}

/*********************************************************************/

static void AddTimeClass(time_t time)
{
    struct tm parsed_time;
    struct tm gmt_parsed_time;
    char buf[CF_BUFSIZE];
    int day_text_index, quarter, interval_start, interval_end;

    if (localtime_r(&time, &parsed_time) == NULL)
    {
        CfOut(cf_error, "localtime_r", "Unable to parse passed time");
        return;
    }

    if (gmtime_r(&time, &gmt_parsed_time) == NULL)
    {
        CfOut(cf_error, "gmtime_r", "Unable to parse passed date");
        return;
    }

/* Lifecycle */

    snprintf(buf, CF_BUFSIZE, "Lcycle_%d", ((parsed_time.tm_year + 1900) % 3));
    HardClass(buf);

/* Year */

    snprintf(VYEAR, CF_BUFSIZE, "%04d", parsed_time.tm_year + 1900);
    snprintf(buf, CF_BUFSIZE, "Yr%04d", parsed_time.tm_year + 1900);
    HardClass(buf);

/* Month */

    strlcpy(VMONTH, MONTH_TEXT[parsed_time.tm_mon], 4);
    HardClass(MONTH_TEXT[parsed_time.tm_mon]);

/* Day of week */

/* Monday  is 1 in tm_wday, 0 in DAY_TEXT
   Tuesday is 2 in tm_wday, 1 in DAY_TEXT
   ...
   Sunday  is 0 in tm_wday, 6 in DAY_TEXT */
    day_text_index = (parsed_time.tm_wday + 6) % 7;
    HardClass(DAY_TEXT[day_text_index]);

/* Day */

    snprintf(VDAY, CF_BUFSIZE, "%d", parsed_time.tm_mday);
    snprintf(buf, CF_BUFSIZE, "Day%d", parsed_time.tm_mday);
    HardClass(buf);

/* Shift */

    strcpy(VSHIFT, SHIFT_TEXT[parsed_time.tm_hour / 6]);
    HardClass(VSHIFT);

/* Hour */

    snprintf(buf, CF_BUFSIZE, "Hr%02d", parsed_time.tm_hour);
    HardClass(buf);

/* GMT hour */

    snprintf(buf, CF_BUFSIZE, "GMT_Hr%d\n", gmt_parsed_time.tm_hour);
    HardClass(buf);

/* Quarter */

    quarter = parsed_time.tm_min / 15 + 1;

    snprintf(buf, CF_BUFSIZE, "Q%d", quarter);
    HardClass(buf);
    snprintf(buf, CF_BUFSIZE, "Hr%02d_Q%d", parsed_time.tm_hour, quarter);
    HardClass(buf);

/* Minute */

    snprintf(buf, CF_BUFSIZE, "Min%02d", parsed_time.tm_min);
    HardClass(buf);

    interval_start = (parsed_time.tm_min / 5) * 5;
    interval_end = (interval_start + 5) % 60;

    snprintf(buf, CF_BUFSIZE, "Min%02d_%02d", interval_start, interval_end);
    HardClass(buf);
}
