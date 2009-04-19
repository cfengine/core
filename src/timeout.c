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

/*****************************************************************************/
/*                                                                           */
/* File: timeout.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*************************************************************************/
  
void SetTimeOut(int timeout)
 
{
ALARM_PID = -1;
signal(SIGALRM,(void *)TimeOut);
alarm(timeout);
}

/*************************************************************************/
  
void TimeOut()
 
{
alarm(0);

if (ALARM_PID != -1)
   {
   CfOut(cf_verbose,"","Time out of process %d\n",ALARM_PID);
   kill(ALARM_PID,cfterm);
   kill(ALARM_PID,cfkill);
   }
else
   {
   CfOut(cf_verbose,"","%s: Time out\n",VPREFIX);
   }
}

/*************************************************************************/

void DeleteTimeOut()

{
}

/*******************************************************************/

void SetReferenceTime(int setclasses)

{ time_t tloc;
  char vbuff[CF_BUFSIZE];
 
if ((tloc = time((time_t *)NULL)) == -1)
   {
   CfOut(cf_error,"time","Couldn't read system clock\n");
   }

CFSTARTTIME = tloc;

snprintf(vbuff,CF_BUFSIZE,"%s",ctime(&tloc));

CfOut(cf_verbose,"","Reference time set to %s\n",ctime(&tloc));

if (setclasses)
   {
   time_t now = time(NULL);
   struct tm *tmv = gmtime(&now);

   AddTimeClass(vbuff);
   snprintf(vbuff,CF_MAXVARSIZE,"GMT_Hr%d\n",tmv->tm_hour);
   NewClass(vbuff);
   }
}

/*******************************************************************/

void SetStartTime(int setclasses)

{ time_t tloc;
 
if ((tloc = time((time_t *)NULL)) == -1)
   {
   CfOut(cf_error,"time","Couldn't read system clock\n");
   }

CFINITSTARTTIME = tloc;

Debug("Job start time set to %s\n",ctime(&tloc));
}

/*********************************************************************/

void AddTimeClass(char *str)

{ int i,value;
  char buf2[10], buf3[10], buf4[10], buf5[10], buf[10], out[10];

for (i = 0; i < 7; i++)
   {
   if (strncmp(DAY_TEXT[i],str,3)==0)
      {
      NewClass(DAY_TEXT[i]);
      break;
      }
   }

sscanf(str,"%*s %s %s %s %s",buf2,buf3,buf4,buf5);

/* Hours */

buf[0] = '\0';

sscanf(buf4,"%[^:]",buf);
sprintf(out,"Hr%s",buf);
NewClass(out);
memset(VHR,0,3);
strncpy(VHR,buf,2); 

/* Shift */

sscanf(buf,"%d",&value);

if (0 <= value && value < 6)
   {
   snprintf(VSHIFT,11,"Night");
   }
else if (6 <= value && value < 12)
   {
   snprintf(VSHIFT,11,"Morning");
   }
else if (12 <= value && value < 18)
   {
   snprintf(VSHIFT,11,"Afternoon");
   }
else if (18 <= value && value < 24)
   {
   snprintf(VSHIFT,11,"Evening");
   }    

NewClass(VSHIFT);

/* Minutes */

sscanf(buf4,"%*[^:]:%[^:]",buf);
sprintf(out,"Min%s",buf);
NewClass(out);
memset(VMINUTE,0,3);
strncpy(VMINUTE,buf,2); 
 
sscanf(buf,"%d",&i);

switch ((i / 5))
   {
   case 0: NewClass("Min00_05");
           break;
   case 1: NewClass("Min05_10");
           break;
   case 2: NewClass("Min10_15");
           break;
   case 3: NewClass("Min15_20");
           break;
   case 4: NewClass("Min20_25");
           break;
   case 5: NewClass("Min25_30");
           break;
   case 6: NewClass("Min30_35");
           break;
   case 7: NewClass("Min35_40");
           break;
   case 8: NewClass("Min40_45");
           break;
   case 9: NewClass("Min45_50");
           break;
   case 10: NewClass("Min50_55");
            break;
   case 11: NewClass("Min55_00");
            break;
   }

/* Add quarters */ 

switch ((i / 15))
   {
   case 0: NewClass("Q1");
           sprintf(out,"Hr%s_Q1",VHR);
    NewClass(out);
           break;
   case 1: NewClass("Q2");
           sprintf(out,"Hr%s_Q2",VHR);
    NewClass(out);
           break;
   case 2: NewClass("Q3");
           sprintf(out,"Hr%s_Q3",VHR);
    NewClass(out);
           break;
   case 3: NewClass("Q4");
           sprintf(out,"Hr%s_Q4",VHR);
    NewClass(out);
           break;
   }
 

/* Day */

sprintf(out,"Day%s",buf3);
NewClass(out);
memset(VDAY,0,3);
strncpy(VDAY,buf3,2);
 
/* Month */

for (i = 0; i < 12; i++)
   {
   if (strncmp(MONTH_TEXT[i],buf2,3) == 0)
      {
      NewClass(MONTH_TEXT[i]);
      memset(VMONTH,0,4);
      strncpy(VMONTH,MONTH_TEXT[i],3);
      break;
      }
   }

/* Year */

strcpy(VYEAR,buf5); 
sprintf(out,"Yr%s",buf5);
NewClass(out);

/* Lifecycle - 3 year cycle */

value = -1;
sscanf(buf5,"%d",&value);
snprintf(VLIFECYCLE,10,"Lcycle_%d",(value%3));
NewClass(VLIFECYCLE);
}

