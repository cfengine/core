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

static void NotePerformance(char *eventname,time_t t,double value);
static void UpdateLastSawHost(char *rkey,char *ipaddress);
static void PurgeMultipleIPReferences(CF_DB *dbp,char *rkey,char *ipaddress);

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

static void NotePerformance(char *eventname,time_t t,double value)

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

void NoteClassUsage(struct AlphaList baselist)

{ CF_DB *dbp;
  CF_DBC *dbcp;
  void *stored;
  char *key,name[CF_BUFSIZE];
  int i,j,ksize,vsize;
  struct Event e,entry,newe;
  double lsea = CF_WEEK * 52; /* expire after a year */
  time_t now = time(NULL);
  struct Item *ip,*list = NULL;
  double lastseen,delta2;
  double vtrue = 1.0;      /* end with a rough probability */

/* Only do this for the default policy, too much "downgrading" otherwise */

if (MINUSF) 
   {
   return;
   }

Debug("RecordClassUsage\n");

for (i = 0; i < CF_ALPHABETSIZE; i++)
   {
   for (ip = baselist.list[i]; ip != NULL; ip=ip->next)
      {
      if (IGNORECLASS(ip->name))
         {
         Debug("Ignoring class %s (not packing)", ip->name);
         continue;
         }

      for (j = 0; j < 4; j++)
         {
         if (strcmp(ip->name,SHIFT_TEXT[j]) == 0)
            {
            continue;
            }
         }

      for (j = 0; j < 7; j++)
         {
         if (strcmp(ip->name,DAY_TEXT[j]) == 0)
            {
            continue;
            }
         }

      for (j = 0; j < 12; j++)
         {
         if (strcmp(ip->name,MONTH_TEXT[j]) == 0)
            {
            continue;
            }
         }
   
      IdempPrependItem(&list,ip->name,NULL);
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
      newe.Q.expect = GAverage(vtrue,e.Q.expect,0.7);
      delta2 = (vtrue - e.Q.expect)*(vtrue - e.Q.expect);
      newe.Q.var = GAverage(delta2,e.Q.var,0.7);
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
   CloseDB(dbp);
   DeleteItemList(list);
   return;
   }

memset(&entry, 0, sizeof(entry));

OpenDBTransaction(dbp);

while(NextDB(dbp,dbcp,&key,&ksize,&stored,&vsize))
   {
   double measure,av,var;
   time_t then;
   char eventname[CF_BUFSIZE];

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

CommitDBTransaction(dbp);

DeleteDBCursor(dbp,dbcp);
CloseDB(dbp);
DeleteItemList(list);
}

/***************************************************************/
/* Last saw handling                                           */
/***************************************************************/

void LastSaw(char *username,char *ipaddress,unsigned char digest[EVP_MAX_MD_SIZE+1],enum roles role)

{ char databuf[CF_BUFSIZE];
  time_t now = time(NULL);
  int known = false;
  char *mapip;

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

mapip = MapAddress(ipaddress);

UpdateLastSawHost(databuf,mapip);
}

/*****************************************************************************/

static void UpdateLastSawHost(char *rkey,char *ipaddress)

{ CF_DB *dbpent = NULL,*dbp = NULL;
  struct CfKeyHostSeen q,newq; 
  double lastseen,delta2;
  void *stored;
  char name[CF_BUFSIZE],*key;
  time_t now = time(NULL);
  int intermittency = false;
  char timebuf[26];

if (BooleanControl("control_agent",CFA_CONTROLBODY[cfa_intermittency].lval))
   {
   CfOut(cf_inform,""," -> Recording intermittency");
   intermittency = true;
   }
  
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   CfOut(cf_inform,""," !! Unable to open last seen db");
   return;
   }

if (intermittency)
   {
   /* Open special file for peer entropy record - INRIA-like intermittency */
   snprintf(name,CF_BUFSIZE-1,"%s/lastseen/%s.%s",CFWORKDIR,CF_LASTDB_FILE,rkey);
   MapName(name);
   
   if (!OpenDB(name,&dbpent))
      {
      intermittency = false;
      }
   }

if (ReadDB(dbp,rkey,&q,sizeof(q)))
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
   strncpy(newq.address,ipaddress,CF_ADDRSIZE-1);
   }
else
   {
   lastseen = 0.0;
   newq.Q.q = (double)now;
   newq.Q.expect = 0.0;
   newq.Q.var = 0.0;
   strncpy(newq.address,ipaddress,CF_ADDRSIZE-1);
   }

if (strcmp(rkey+1,PUBKEY_DIGEST) == 0)
   {
   struct Item *ip;
   int match = false;

   for (ip = IPADDRESSES; ip != NULL; ip=ip->next)
      {
      if (strcmp(ipaddress,ip->name) == 0)
         {
         match = true;
         }
      }

   if (!match)
      {
      CfOut(cf_verbose,""," ! Not updating last seen, as this appears to be a host with a duplicate key");
      CloseDB(dbp);

      if (intermittency && dbpent)
         {
         CloseDB(dbpent);
         }
      
      return;
      }
   }

CfOut(cf_verbose,""," -> Last saw %s (alias %s) at %s\n",rkey,ipaddress,cf_strtimestamp_local(now,timebuf));

PurgeMultipleIPReferences(dbp,rkey,ipaddress);

WriteDB(dbp,rkey,&newq,sizeof(newq));

if (intermittency)
   {
   WriteDB(dbpent,GenTimeKey(now),&newq,sizeof(newq));
   }

if (intermittency && dbpent)
   {
   CloseDB(dbpent);
   }

CloseDB(dbp);
}

/*****************************************************************************/

bool RemoveHostFromLastSeen(const char *hostname, char *hostkey)
{
char ip[CF_BUFSIZE];
char digest[CF_BUFSIZE]={0};

if(!hostkey)
   {
   strcpy(ip, Hostname2IPString(hostname));
   IPString2KeyDigest(ip, digest);
   }
else
   {
   snprintf(digest,sizeof(digest),"%s",hostkey);
   }

CF_DB *dbp;
char name[CF_BUFSIZE], key[CF_BUFSIZE];
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);
MapName(name);

if (!OpenDB(name, &dbp))
   {
   CfOut(cf_error, "", " !! Unable to open last seen DB");
   return false;
   }

snprintf(key, CF_BUFSIZE, "-%s", digest);
DeleteComplexKeyDB(dbp, key, strlen(key) + 1);
snprintf(key, CF_BUFSIZE, "+%s", digest);
DeleteComplexKeyDB(dbp, key, strlen(key) + 1);

CloseDB(dbp);
return true;
}

/*****************************************************************************/

static void PurgeMultipleIPReferences(CF_DB *dbp,char *rkey,char *ipaddress)

{ CF_DBC *dbcp;
  struct CfKeyHostSeen q,newq; 
  double lastseen,delta2,lsea = LASTSEENEXPIREAFTER;
  void *stored;
  char name[CF_BUFSIZE],*key;
  time_t now = time(NULL);
  int qsize,ksize,update_address,keys_match;

// This is an expensive call, but it is the price we pay for consistency
// Make sure we only call it if we have to
  
if (!NewDBCursor(dbp,&dbcp))
   {
   CfOut(cf_inform,""," !! Unable to scan the last seen db");
   return;
   }

while(NextDB(dbp,dbcp,&key,&ksize,&stored,&qsize))
   {
   keys_match = false;
   
   if (strcmp(key+1,rkey+1) == 0)
      {
      keys_match = true;
      }

   memcpy(&q,stored,sizeof(q));

   lastseen = (double)now - q.Q.q;

   if (lastseen > lsea)
      {
      CfOut(cf_verbose,""," -> Last-seen record for %s expired after %.1lf > %.1lf hours\n",key,lastseen/3600,lsea/3600);
      DeleteDB(dbp,key);
      continue;
      }

   // Avoid duplicate address/key pairs
   
   if (keys_match && strcmp(q.address,ipaddress) != 0)
      {
      CfOut(cf_verbose,""," ! Synchronizing %s's address as this host %s seems to have moved from location %s to %s",key,rkey,q.address,ipaddress);
      strcpy(q.address,ipaddress);
      update_address = true;
      }
   else if (!keys_match && strcmp(q.address,ipaddress) == 0)
      {
      CfOut(cf_verbose,""," ! Updating %s's address (%s) as this host %s seems to have gone off line",key,ipaddress,rkey);
      strcpy(q.address,CF_UNKNOWN_IP);
      update_address = true;
      }
   else 
      {
      update_address = false;
      }

   if (update_address)
      {
      WriteDB(dbp,key,&q,sizeof(q));
      }
   }

DeleteDBCursor(dbp,dbcp);
}

/*****************************************************************************/
/* Toolkit                                                                   */
/*****************************************************************************/

double GAverage(double anew,double aold,double p)

/* return convex mixture - p is the trust/confidence in the new value */
    
{
return (p*anew + (1.0-p)*aold);
}

