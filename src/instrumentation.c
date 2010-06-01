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
/* File: instrumentation.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include <math.h>

/* Alter this code at your peril. Berkeley DB is very sensitive to errors. */

/***************************************************************/

struct timespec BeginMeasure()
    
{ struct timespec start;

if (clock_gettime(CLOCK_REALTIME, &start) == -1)
   {
   CfOut(cf_verbose,"clock_gettime","Clock gettime failure");
   }

return start;
}

/***************************************************************/

void EndMeasurePromise(struct timespec start,struct Promise *pp)

{ char id[CF_BUFSIZE], *mid = NULL;

mid = GetConstraint("measurement_class",pp,CF_SCALAR);

if (mid)
   {
   snprintf(id,CF_BUFSIZE,"%s:%s:%.100s",(char *)mid,pp->agentsubtype,pp->promiser);
   Chop(id);
   EndMeasure(id,start);
   }
}

/***************************************************************/

void EndMeasure(char *eventname,struct timespec start)

{ struct timespec stop;
  int measured_ok = true;
  double dt;

if (clock_gettime(CLOCK_REALTIME, &stop) == -1)
   {
   CfOut(cf_verbose,"clock_gettime","Clock gettime failure");
   measured_ok = false;
   }

dt = (double)(stop.tv_sec - start.tv_sec)+(double)(stop.tv_nsec-start.tv_nsec)/(double)CF_BILLION;

if (measured_ok)
   {
   NotePerformance(eventname,start.tv_sec,dt);
   }
}

/***************************************************************/

void NotePerformance(char *eventname,time_t t,double value)

{ CF_DB *dbp;
  char name[CF_BUFSIZE];
  struct Event e,newe;
  double lastseen,delta2;
  int lsea = CF_WEEK;
  time_t now = time(NULL);

Debug("PerformanceEvent(%s,%.1f s)\n",eventname,value);

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_PERFORMANCE);

if (!OpenDB(name,&dbp))
   {
   return;
   }

if (ReadDB(dbp,eventname,&e,sizeof(e)))
   {
   lastseen = now - e.t;
   newe.t = t;
   newe.Q.q = value;
   newe.Q.expect = GAverage(value,e.Q.expect,0.3);
   delta2 = (value - e.Q.expect)*(value - e.Q.expect);
   newe.Q.var = GAverage(delta2,e.Q.var,0.3);

   /* Have to kickstart variance computation, assume 1% to start  */
   
   if (newe.Q.var <= 0.0009)
      {
      newe.Q.var =  newe.Q.expect / 100.0;
      }
   }
else
   {
   lastseen = 0.0;
   newe.t = t;
   newe.Q.q = value;
   newe.Q.expect = value;
   newe.Q.var = 0.001;
   }

if (lastseen > (double)lsea)
   {
   Debug("Performance record %s expired\n",eventname);
   DeleteDB(dbp,eventname);   
   }
else
   {
   CfOut(cf_verbose,"","Performance(%s): time=%.4lf secs, av=%.4lf +/- %.4lf\n",eventname,value,newe.Q.expect,sqrt(newe.Q.var));
   WriteDB(dbp,eventname,&newe,sizeof(newe));
   }

CloseDB(dbp);
}

/***************************************************************/

void NoteClassUsage(struct Item *baselist)

{ CF_DB *dbp;
  CF_DBC *dbcp;
  void *stored;
  char *key,name[CF_BUFSIZE];
  int ksize,vsize;
  struct Event e,entry,newe;
  double lsea = CF_WEEK * 52; /* expire after a year */
  time_t now = time(NULL);
  struct Item *ip,*list = NULL;
  double lastseen,delta2;
  double vtrue = 1.0;      /* end with a rough probability */

Debug("RecordClassUsage\n");

for (ip = baselist; ip != NULL; ip=ip->next)
   {
   if (IsHardClass(ip->name))
      {
      continue;
      }
   
   if (!IsItemIn(list,ip->name))
      {
      PrependItem(&list,ip->name,NULL);
      }
   }

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_CLASSUSAGE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return;
   }

/* First record the classes that are in use */

for (ip = list; ip != NULL; ip=ip->next)
   {
   if (ReadDB(dbp,ip->name,&e,sizeof(e)))
      {
      lastseen = now - e.t;
      newe.t = now;
      newe.Q.q = vtrue;
      newe.Q.expect = GAverage(vtrue,e.Q.expect,0.3);
      delta2 = (vtrue - e.Q.expect)*(vtrue - e.Q.expect);
      newe.Q.var = GAverage(delta2,e.Q.var,0.3);
      }
   else
      {
      lastseen = 0.0;
      newe.t = now;
      newe.Q.q = 0.5*vtrue;
      newe.Q.expect = 0.5*vtrue;  /* With no data it's 50/50 what we can say */
      newe.Q.var = 0.000;
      }
   
   if (lastseen > lsea)
      {
      Debug("Class usage record %s expired\n",ip->name);
      DeleteDB(dbp,ip->name);   
      }
   else
      {
      Debug("Upgrading %s %f\n",ip->name,newe.Q.expect);
      WriteDB(dbp,ip->name,&newe,sizeof(newe));
      }
   }

/* Then update with zero the ones we know about that are not active */

/* Acquire a cursor for the database. */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan class db");
   return;
   }

memset(&entry, 0, sizeof(entry)); 

while(NextDB(dbp,dbcp,&key,&ksize,&stored,&vsize))
   {
   double measure,av,var;
   time_t then;
   char tbuf[CF_BUFSIZE],eventname[CF_BUFSIZE];

   memset(eventname,0,CF_BUFSIZE);
   strncpy(eventname,(char *)key,ksize);

   if (stored != NULL)
      {
      memcpy(&entry,stored,sizeof(entry));
      
      then    = entry.t;
      measure = entry.Q.q;
      av = entry.Q.expect;
      var = entry.Q.var;
      lastseen = now - then;
            
      snprintf(tbuf,CF_BUFSIZE-1,"%s",cf_ctime(&then));
      tbuf[strlen(tbuf)-9] = '\0';                     /* Chop off second and year */

      if (lastseen > lsea)
         {
         Debug("Class usage record %s expired\n",eventname);
         DeleteDB(dbp,eventname);   
         }
      else if (!IsItemIn(list,eventname))
         {
         newe.t = then;
         newe.Q.q = 0;
         newe.Q.expect = GAverage(0.0,av,0.5);
         delta2 = av*av;
         newe.Q.var = GAverage(delta2,var,0.5);
         Debug("Downgrading class %s from %lf to %lf\n",eventname,entry.Q.expect,newe.Q.expect);
         WriteDB(dbp,eventname,&newe,sizeof(newe));         
         }
      }
   }

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
DeleteItemList(list);
}

/***************************************************************/

void LastSaw(char *hostname,enum roles role)

{ CF_DB *dbp,*dbpent;
  char name[CF_BUFSIZE],databuf[CF_BUFSIZE],varbuf[CF_BUFSIZE],rtype;
  time_t now = time(NULL);
  struct QPoint q,newq;
  double lastseen,delta2;
  int lsea = LASTSEENEXPIREAFTER, intermittency = false;

if (strlen(hostname) == 0)
   {
   CfOut(cf_inform,"","LastSeen registry for empty hostname with role %d",role);
   return;
   }

if (BooleanControl("control_agent",CFA_CONTROLBODY[cfa_intermittency].lval))
   {
   CfOut(cf_inform,""," -> Recording intermittency");
   intermittency = true;
   }

CfOut(cf_verbose,"","LastSaw host %s now\n",hostname);

ThreadLock(cft_getaddr);

switch (role)
   {
   case cf_accept:
       snprintf(databuf,CF_BUFSIZE-1,"-%s",Hostname2IPString(hostname));
       break;
   case cf_connect:
       snprintf(databuf,CF_BUFSIZE-1,"+%s",Hostname2IPString(hostname));
       break;
   }

ThreadUnlock(cft_getaddr);

/* Tidy old versions - temporary */
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);


if (!ThreadLock(cft_db_lastseen))
   {
   return;
   }

if (!OpenDB(name,&dbp))
   {
   ThreadUnlock(cft_db_lastseen);
   return;
   }

if (intermittency)
   {
   /* Open special file for peer entropy record - INRIA intermittency */
   snprintf(name,CF_BUFSIZE-1,"%s/lastseen/%s.%s",CFWORKDIR,CF_LASTDB_FILE,hostname);
   MapName(name);
   
   if (!OpenDB(name,&dbpent))
      {
      ThreadUnlock(cft_db_lastseen);
      return;
      }
   }

   
if (ReadDB(dbp,databuf,&q,sizeof(q)))
   {
   lastseen = (double)now - q.q;
   newq.q = (double)now;                   /* Last seen is now-then */
   newq.expect = GAverage(lastseen,q.expect,0.3);
   delta2 = (lastseen - q.expect)*(lastseen - q.expect);
   newq.var = GAverage(delta2,q.var,0.3);
   }
else
   {
   lastseen = 0.0;
   newq.q = (double)now;
   newq.expect = 0.0;
   newq.var = 0.0;
   }

ThreadLock(cft_getaddr);

if (lastseen > (double)lsea)
   {
   CfOut(cf_verbose,"","Last seen %s expired\n",databuf);
   DeleteDB(dbp,databuf);   
   }
else
   {
   WriteDB(dbp,databuf,&newq,sizeof(newq));

   if (intermittency)
      {
      WriteDB(dbpent,GenTimeKey(now),&newq,sizeof(newq));
      }
   }

ThreadUnlock(cft_getaddr);

if (intermittency)
   {
   CloseDB(dbpent);
   }

CloseDB(dbp);
ThreadUnlock(cft_db_lastseen);
}

/*****************************************************************************/
/* Toolkit                                                                   */
/*****************************************************************************/

double GAverage(double anew,double aold,double p)

/* return convex mixture - p is the trust in the new value */
    
{
return (p*anew + (1.0-p)*aold);
}

