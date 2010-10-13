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
     if (IGNORECLASS(ip->name))
      {
      Debug("Ignoring class %s (not packing)", ip->name);
      continue;
      }
   
   IdempPrependItem(&list,ip->name,NULL);
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

void LastSaw(char *username,char *ipaddress,unsigned char digest[EVP_MAX_MD_SIZE+1],enum roles role)

{ char databuf[CF_BUFSIZE],varbuf[CF_BUFSIZE],rtype;
  time_t now = time(NULL);
  int known = false;
  struct Rlist *rp;
  struct CfKeyBinding *kp;

if (strlen(ipaddress) == 0)
   {
   CfOut(cf_inform,"","LastSeen registry for empty IP with role %d",role);
   return;
   }

ThreadLock(cft_output);

switch (role)
   {
   case cf_accept:
       snprintf(databuf,CF_BUFSIZE-1,"-%s",HashPrint(CF_DEFAULT_DIGEST,digest));
       break;
   case cf_connect:
       snprintf(databuf,CF_BUFSIZE-1,"+%s",HashPrint(CF_DEFAULT_DIGEST,digest));
       break;
   }

ThreadUnlock(cft_output);

for (rp = SERVER_KEYSEEN; rp !=  NULL; rp=rp->next)
   {
   kp = (struct CfKeyBinding *) rp->item;

   if (strcmp(kp->name,databuf) == 0)
      {
      known = true;
      kp->timestamp = now;
      CfOut(cf_verbose,""," -> Last saw %s (%s) now",ipaddress,databuf);

      // Refresh address
      
      ThreadLock(cft_system);
      kp->address = strdup(ipaddress);
      ThreadUnlock(cft_system);
      return;
      }
   }

CfOut(cf_verbose,""," -> Last saw %s (%s) now",ipaddress,databuf);

rp = PrependRlist(&SERVER_KEYSEEN,"nothing",CF_SCALAR);

ThreadLock(cft_system);

kp = (struct CfKeyBinding *)malloc((sizeof(struct CfKeyBinding)));

if (kp == NULL)
   {
   ThreadUnlock(cft_system);
   return;
   }

free(rp->item);
rp->item = kp;

kp->address = strdup(ipaddress);

if ((kp->name = strdup(databuf)) == NULL)
   {
   free(kp);
   ThreadUnlock(cft_system);
   return;
   }

ThreadUnlock(cft_system);

kp->key = HavePublicKey(username,ipaddress,databuf+1);
kp->timestamp = now;
}

/***************************************************************/

void UpdateLastSeen()

{ double lsea = LASTSEENEXPIREAFTER;
  int intermittency = false,qsize,ksize;
  struct CfKeyHostSeen q,newq; 
  double lastseen,delta2;
  void *stored;
  CF_DB *dbp = NULL,*dbpent = NULL;
  CF_DBC *dbcp;
  char name[CF_BUFSIZE],*key;
  struct Rlist *rp;
  struct CfKeyBinding *kp;
  time_t now = time(NULL);
  static time_t then;
  
if (now < then + 300 && then > 0 && then <= now + 300)
   {
   // Rate limiter
   return;
   }

then = now;

CfOut(cf_verbose,""," -> Writing last-seen observations");

if (SERVER_KEYSEEN == NULL)
   {
   CfOut(cf_verbose,""," -> Keyring is empty");
   return;
   }

if (BooleanControl("control_agent",CFA_CONTROLBODY[cfa_intermittency].lval))
   {
   CfOut(cf_inform,""," -> Recording intermittency");
   intermittency = true;
   }

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return;
   }

/* First scan for hosts that have moved address and purge their records so that
   the database always has a 1:1 relationship between keyhash and IP address    */

if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan class db");
   return;
   }

while(NextDB(dbp,dbcp,&key,&ksize,&stored,&qsize))
   {
   memcpy(&q,stored,sizeof(q));

   lastseen = (double)now - q.Q.q;

   if (lastseen > lsea)
      {
      CfOut(cf_verbose,""," -> Last-seen record for %s expired after %.1lf > %.1lf hours\n",key,lastseen/3600,lsea/3600);
      DeleteDB(dbp,key);
      }

   for (rp = SERVER_KEYSEEN; rp !=  NULL; rp=rp->next)
      {
      kp = (struct CfKeyBinding *) rp->item;
      
      if ((strcmp(q.address,kp->address) == 0) && (strcmp(key+1,kp->name+1) != 0))
         {
         CfOut(cf_verbose,""," ! Deleting %s's address (%s=%d) as this host %s seems to have moved elsewhere (%s=5d)",key,kp->address,strlen(kp->address),kp->name,q.address,strlen(q.address));
         DeleteDB(dbp,key);
         }
      }
   }

DeleteDBCursor(dbp,dbcp);

/* Now perform updates with the latest data */

for (rp = SERVER_KEYSEEN; rp !=  NULL; rp=rp->next)
   {
   kp = (struct CfKeyBinding *) rp->item;

   now = kp->timestamp;
   
   if (intermittency)
      {
      /* Open special file for peer entropy record - INRIA intermittency */
      snprintf(name,CF_BUFSIZE-1,"%s/lastseen/%s.%s",CFWORKDIR,CF_LASTDB_FILE,kp->name);
      MapName(name);
      
      if (!OpenDB(name,&dbpent))
         {
         continue;
         }
      }
   
   if (ReadDB(dbp,kp->name,&q,sizeof(q)))
      {
      lastseen = (double)now - q.Q.q;
      
      if (q.Q.q <= 0)
         {
         lastseen = 300;
         q.Q.expect = 0;
         q.Q.var = 0;
         }
      
      newq.Q.q = (double)now;
      newq.Q.expect = GAverage(lastseen,q.Q.expect,0.4);
      delta2 = (lastseen - q.Q.expect)*(lastseen - q.Q.expect);
      newq.Q.var = GAverage(delta2,q.Q.var,0.4);
      strncpy(newq.address,kp->address,CF_ADDRSIZE-1);
      }
   else
      {
      lastseen = 0.0;
      newq.Q.q = (double)now;
      newq.Q.expect = 0.0;
      newq.Q.var = 0.0;
      strncpy(newq.address,kp->address,CF_ADDRSIZE-1);
      }
   
   if (lastseen > lsea)
      {
      CfOut(cf_verbose,""," -> Last-seen record for %s expired after %.1lf > %.1lf hours\n",kp->name,lastseen/3600,lsea/3600);
      DeleteDB(dbp,kp->name);
      }
   else
      {
      CfOut(cf_verbose,""," -> Last saw %s (alias %s) at %s (noexpiry %.1lf <= %.1lf)\n",kp->name,kp->address,ctime(&now),lastseen/3600,lsea/3600);

      ThreadLock(cft_dbhandle);
      WriteDB(dbp,kp->name,&newq,sizeof(newq));
      ThreadUnlock(cft_dbhandle);
      
      if (intermittency)
         {
         WriteDB(dbpent,GenTimeKey(now),&newq,sizeof(newq));
         }
      }
   
   if (intermittency && dbpent)
      {
      CloseDB(dbpent);
      }
   }

CloseDB(dbp);

// Should we purge the list DeleteRlist(SERVER_KEYSEEN)?
// Careful to dealloc Keyring
}

/*****************************************************************************/
/* Toolkit                                                                   */
/*****************************************************************************/

double GAverage(double anew,double aold,double p)

/* return convex mixture - p is the trust in the new value */
    
{
return (p*anew + (1.0-p)*aold);
}

