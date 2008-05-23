/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: instrumentation.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#include <math.h>

/*
# if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
pthread_mutex_t MUTEX_GETADDR = PTHREAD_MUTEX_INITIALIZER;
# endif
*/

extern pthread_mutex_t MUTEX_GETADDR;

/* Alter this code at your peril. Berkeley DB is very sensitive to errors. */

/***************************************************************/

void LastSaw(char *hostname,enum roles role)

{ DB *dbp,*dbpent;
  DB_ENV *dbenv = NULL, *dbenv2 = NULL;
  char name[CF_BUFSIZE],databuf[CF_BUFSIZE],varbuf[CF_BUFSIZE],rtype;
  time_t now = time(NULL);
  struct QPoint q,newq;
  double lastseen,delta2;
  int lsea = -1;

if (strlen(hostname) == 0)
   {
   snprintf(OUTPUT,CF_BUFSIZE,"LastSeen registry for empty hostname with role %d",role);
   CfLog(cflogonly,OUTPUT,"");
   return;
   }

Debug("LastSeen(%s) reg\n",hostname);

/* Tidy old versions - temporary */
snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_OLDLASTDB_FILE);
unlink(name);

if ((errno = db_create(&dbp,dbenv,0)) != 0)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't init last-seen database %s\n",name);
   CfLog(cferror,OUTPUT,"db_open");
   return;
   }

snprintf(name,CF_BUFSIZE-1,"%s/%s",CFWORKDIR,CF_LASTDB_FILE);

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't open last-seen database %s\n",name);
   CfLog(cferror,OUTPUT,"db_open");
   dbp->close(dbp,0);
   return;
   }

/* Now open special file for peer entropy record - INRIA intermittency */
snprintf(name,CF_BUFSIZE-1,"%s/%s.%s",CFWORKDIR,CF_LASTDB_FILE,hostname);

if ((errno = db_create(&dbpent,dbenv2,0)) != 0)
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't init last-seen database %s\n",name);
   CfLog(cferror,OUTPUT,"db_open");
   return;
   }

#ifdef CF_OLD_DB
if ((errno = (dbpent->open)(dbpent,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbpent->open)(dbpent,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   snprintf(OUTPUT,CF_BUFSIZE*2,"Couldn't open last-seen database %s\n",name);
   CfLog(cferror,OUTPUT,"db_open");
   dbp->close(dbp,0);
   return;
   }


#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_lock(&MUTEX_GETADDR) != 0)
   {
   CfLog(cferror,"pthread_mutex_lock failed","unlock");
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
   CfLog(cferror,"pthread_mutex_unlock failed","unlock");
   exit(1);
   }
#endif

if (GetVariable("common","lastseenexpireafter",(void *)varbuf,&rtype) != cf_notype)
   {
   lsea = atoi(varbuf);
   lsea *= CF_TICKS_PER_DAY;
   }

if (lsea < 0)
   {
   lsea = CF_WEEK;
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

#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_lock(&MUTEX_GETADDR) != 0)
   {
   CfLog(cferror,"pthread_mutex_lock failed","unlock");
   exit(1);
   }
#endif

if (lastseen > (double)lsea)
   {
   Verbose("Last seen %s expired\n",databuf);
   DeleteDB(dbp,databuf);   
   }
else
   {
   WriteDB(dbp,databuf,&newq,sizeof(newq));
   WriteDB(dbpent,GenTimeKey(now),&newq,sizeof(newq));
   }

#ifdef HAVE_PTHREAD_H  
if (pthread_mutex_unlock(&MUTEX_GETADDR) != 0)
   {
   CfLog(cferror,"pthread_mutex_unlock failed","unlock");
   exit(1);
   }
#endif

dbp->close(dbp,0);
dbpent->close(dbpent,0);
}


