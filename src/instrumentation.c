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

# if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
pthread_mutex_t MUTEX_GETADDR = PTHREAD_MUTEX_INITIALIZER;
# endif

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

mid = GetConstraint("measurement_class",pp->conlist,CF_SCALAR);

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

{ DB *dbp;
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

dbp->close(dbp,0);
}

/***************************************************************/

void NoteClassUsage(struct Item *baselist)

{ DB *dbp;
  DBC *dbcp;
  DBT key,stored;
  char name[CF_BUFSIZE];
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

if ((errno = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0)
   {
   dbp->err(dbp, errno, "DB->cursor");
   dbp->close(dbp,0);
   return;
   }

 /* Initialize the key/data return pair. */
 
memset(&key, 0, sizeof(key));
memset(&stored, 0, sizeof(stored));
memset(&entry, 0, sizeof(entry)); 

while (dbcp->c_get(dbcp, &key, &stored, DB_NEXT) == 0)
   {
   double measure,av,var;
   time_t then;
   char tbuf[CF_BUFSIZE],eventname[CF_BUFSIZE];

   strcpy(eventname,(char *)key.data);

   if (stored.data != NULL)
      {
      memcpy(&entry,stored.data,sizeof(entry));
      
      then    = entry.t;
      measure = entry.Q.q;
      av = entry.Q.expect;
      var = entry.Q.var;
      lastseen = now - then;
            
      snprintf(tbuf,CF_BUFSIZE-1,"%s",ctime(&then));
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

dbp->close(dbp,0);
DeleteItemList(list);
}

/***************************************************************/

void LastSaw(char *hostname,enum roles role)

{ DB *dbp,*dbpent;
  DB_ENV *dbenv = NULL, *dbenv2 = NULL;
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

/* Tidy old versions - temporary */
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);

if (!OpenDB(name,&dbp))
   {
   return;
   }

if (intermittency)
   {
   /* Open special file for peer entropy record - INRIA intermittency */
   snprintf(name,CF_BUFSIZE-1,"%s/lastseen/%s.%s",CFWORKDIR,CF_LASTDB_FILE,hostname);
   
   if (!OpenDB(name,&dbpent))
      {
      return;
      }
   }

#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_lock(&MUTEX_GETADDR) != 0)
   {
   CfOut(cf_error,"lock","pthread_mutex_lock failed");
   exit(1);
   }
#endif

switch (role)
   {
   case cf_accept:
       snprintf(databuf,CF_BUFSIZE-1,"-%s",Hostname2IPString(hostname));
       break;
   case cf_connect:
       snprintf(databuf,CF_BUFSIZE-1,"+%s",Hostname2IPString(hostname));
       break;
   }

#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_unlock(&MUTEX_GETADDR) != 0)
   {
   CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
   exit(1);
   }
#endif
   
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

#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_lock(&MUTEX_GETADDR) != 0)
   {
   CfOut(cf_error,"lock","pthread_mutex_lock failed");
   exit(1);
   }
#endif

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

#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_unlock(&MUTEX_GETADDR) != 0)
   {
   CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
   exit(1);
   }
#endif

if (intermittency)
   {
   dbpent->close(dbpent,0);
   }

dbp->close(dbp,0);
}

/*****************************************************************************/
/* level                                                                     */
/*****************************************************************************/

int OpenDB(char *filename,DB **dbp)
 
{ DB_ENV *dbenv = NULL;

if ((errno = db_create(dbp,dbenv,0)) != 0)
   {
   CfOut(cf_error,"db_open","Couldn't get database environment for %s\n",filename);
   return false;
   }

#ifdef CF_OLD_DB
if ((errno = ((*dbp)->open)(*dbp,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = ((*dbp)->open)(*dbp,NULL,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open database %s\n",filename);
   return false;
   }

return true;
}

/*****************************************************************************/

int ReadDB(DB *dbp,char *name,void *ptr,int size)

{ DBT *key,value;

if (dbp == NULL)
   {
   return false;
   }
 
key = NewDBKey(name);
memset(&value,0,sizeof(DBT));

if ((errno = dbp->get(dbp,NULL,key,&value,0)) == 0)
   {
   memset(ptr,0,size);
   
   if (value.data)
      {
      memcpy(ptr,value.data,size);
      }
   else
      {
      DeleteDBKey(key);
      return false;
      }
   
   Debug("READ %s\n",name);
   DeleteDBKey(key);
   return true;
   }
else
   {
   Debug("Database read failed: %s",db_strerror(errno));
   DeleteDBKey(key);
   return false;
   }
}

/*****************************************************************************/

int WriteDB(DB *dbp,char *name,void *ptr,int size)

{ DBT *key,*value;

if (dbp == NULL)
   {
   return false;
   }
 
key = NewDBKey(name); 
value = NewDBValue(ptr,size);

if ((errno = dbp->put(dbp,NULL,key,value,0)) != 0)
   {
   Debug("Database write failed: %s",db_strerror(errno));
   DeleteDBKey(key);
   DeleteDBValue(value);
   return false;
   }
else
   {
   Debug("WriteDB => %s\n",name);

   DeleteDBKey(key);
   DeleteDBValue(value);
   return true;
   }
}

/*****************************************************************************/

void DeleteDB(DB *dbp,char *name)

{ DBT *key;

if (dbp == NULL)
   {
   return;
   }
 
key = NewDBKey(name);

if ((errno = dbp->del(dbp,NULL,key,0)) != 0)
   {
   Debug("Database deletion failed: %s",db_strerror(errno));
   }

DeleteDBKey(key);
Debug("DELETED DB %s\n",name);
}

/*****************************************************************************/
/* Level 2                                                                   */
/*****************************************************************************/

DBT *NewDBKey(char *name)

{ char *dbkey;
  DBT *key;

if ((dbkey = malloc(strlen(name)+1)) == NULL)
   {
   FatalError("NewChecksumKey malloc error");
   }

if ((key = (DBT *)malloc(sizeof(DBT))) == NULL)
   {
   FatalError("DBT  malloc error");
   }

memset(key,0,sizeof(DBT));
memset(dbkey,0,strlen(name)+1);

strncpy(dbkey,name,strlen(name));

key->data = (void *)dbkey;
key->size = strlen(name)+1;

return key;
}

/*****************************************************************************/

void DeleteDBKey(DBT *key)

{
free((char *)key->data);
free((char *)key);
}

/*****************************************************************************/

DBT *NewDBValue(void *ptr,int size)

{ void *val;
  DBT *value;

if ((val = (void *)malloc(size)) == NULL)
   {
   FatalError("NewDBKey malloc error");
   }

if ((value = (DBT *) malloc(sizeof(DBT))) == NULL)
   {
   FatalError("DBT Value malloc error");
   }

memset(value,0,sizeof(DBT)); 
memset(val,0,size);
memcpy(val,ptr,size);

value->data = val;
value->size = size;

return value;
}

/*****************************************************************************/

void DeleteDBValue(DBT *value)

{
free((char *)value->data);
free((char *)value);
}

/*****************************************************************************/
/* Toolkit                                                                   */
/*****************************************************************************/

double GAverage(double anew,double aold,double p)

/* return convex mixture - p is the trust in the new value */
    
{
return (p*anew + (1.0-p)*aold);
}

