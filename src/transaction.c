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
/* File: transaction.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void SummarizeTransaction(struct Attributes attr,struct Promise *pp)

{
if (attr.transaction.log_string)
   {
   cfPS(cf_log,CF_NOP,"",pp,attr,attr.transaction.log_string);
   }
 
/*
   char *log_string;
   char *log_level;

   enum cfoutputlevel report_level;
   cf_inform,
   cf_verbose,
   cf_error,
   cflogonly,
   {"log_level",cf_str,"inform,verbose,debug"},
*/
}

/*****************************************************************************/

struct CfLock AcquireLock(char *operator,char *operand,char *host,time_t now,struct Attributes attr,struct Promise *pp)

{ unsigned int pid;
  int i, err, sum=0;
  time_t lastcompleted = 0, elapsedtime;
  char c_operator[CF_BUFSIZE],c_operand[CF_BUFSIZE],cc_operator[CF_BUFSIZE],cc_operand[CF_BUFSIZE];
  char cflock[CF_BUFSIZE],cflast[CF_BUFSIZE],cflog[CF_BUFSIZE];
  struct CfLock this;

this.last =  (char *) CF_UNDEFINED;
this.lock =  (char *) CF_UNDEFINED;
this.log =  (char *) CF_UNDEFINED;

/* Mark this here as done if we tried ... as we have passed all class constraints now */

*(pp->donep) = true;

if (IGNORELOCK)
   {
   return this;
   }

if (now == 0)
   {
   return this;
   }

this.last = NULL;
this.lock = NULL;
this.log = NULL;

if (pp->done)
   {
   return this;
   }

Debug("AcquireLock(%s,%s,time=%d), ExpireAfter=%d, IfElapsed=%d\n",operator,operand,now,attr.transaction.expireafter,attr.transaction.ifelapsed);

/* Make local copy in case CanonifyName called - not re-entrant - best fix for now */

strncpy(c_operator,operator,CF_BUFSIZE-1);
strncpy(c_operand,operand,CF_BUFSIZE-1);
strncpy(cc_operator,CanonifyName(c_operator),CF_BUFSIZE-1);
strncpy(cc_operand,CanonifyName(c_operand),CF_BUFSIZE-1);

for (i = 0; operator[i] != '\0'; i++)
    {
    sum = (CF_MACROALPHABET * sum + operator[i]) % CF_HASHTABLESIZE;
    }

for (i = 0; operand[i] != '\0'; i++)
    {
    sum = (CF_MACROALPHABET * sum + operand[i]) % CF_HASHTABLESIZE;
    }

snprintf(cflog,CF_BUFSIZE,"%s/cf3.%.40s.runlog",CFWORKDIR,host);
snprintf(cflock,CF_BUFSIZE,"lock.%.40s.%.100s.%s.%.100s_%d",CanonifyName(host),pp->bundle,cc_operator,cc_operand,sum);
snprintf(cflast,CF_BUFSIZE,"last.%.40s.%.100s.%s.%.100s_%d",CanonifyName(host),pp->bundle,cc_operator,cc_operand,sum);

/* for signal handler - not threadsafe so applies only to main thread */

CFINITSTARTTIME = time(NULL);
strcpy(CFLOCK,cflock);

/* Look for non-existent (old) processes */

lastcompleted = FindLock(cflast);
elapsedtime = (time_t)(now-lastcompleted) / 60;

if (elapsedtime < 0)
   {
   CfOut(cf_verbose,"","Another cfengine seems to have done [%s.%s] since I started (elapsed=%d)\n",operator,operand,elapsedtime);
   return this;
   }

if (elapsedtime < attr.transaction.ifelapsed)
   {
   CfOut(cf_verbose,"","Nothing promised for [%s.%s] (%u/%u minutes elapsed)\n",operator,operand,elapsedtime,attr.transaction.ifelapsed);
   return this;
   }

/* Look for existing (current) processes */

lastcompleted = FindLock(cflock);
elapsedtime = (time_t)(now-lastcompleted) / 60;
    
if (lastcompleted != 0)
   {
   if (elapsedtime >= attr.transaction.expireafter)
      {
      CfOut(cf_inform,"","Lock %s expired (after %u/%u minutes)\n",cflock,elapsedtime,attr.transaction.expireafter);

      pid = FindLockPid(cflock);

      if (pid == -1)
         {
         CfOut(cf_error,"","Illegal pid in corrupt lock %s - ignoring lock\n",cflock);
         }
      else
         {
         Verbose("Trying to kill expired process, pid %d\n",pid);
         
         err = 0;
         
         if ((err = kill(pid,SIGINT)) == -1)
            {
            sleep(1);
            err=0;
            
            if ((err = kill(pid,SIGTERM)) == -1)
               {   
               sleep(5);
               err=0;
               
               if ((err = kill(pid,SIGKILL)) == -1)
                  {
                  sleep(1);
                  }
               }
            }
         
         if (err == 0 || errno == ESRCH)
            {
            LogLockCompletion(cflog,pid,"Lock expired, process killed",operator,operand);
            unlink(cflock);
            }
         else
            {
            CfOut(cf_error,"kill","Unable to kill expired cfagent process %d from lock %s, exiting this time..\n",pid,cflock);
            
            FatalError("");
            }
         }
      }
   else
      {
      Verbose("Couldn't obtain lock for %s (already running!)\n",cflock);
      return this;
      }
   }
 
WriteLock(cflock);

this.lock = strdup(cflock);
this.last = strdup(cflast);
this.log  = strdup(cflog);

return this;
}

/************************************************************************/

void YieldCurrentLock(struct CfLock this)

{
if (IGNORELOCK)
   {
   return;
   }

if (this.lock == (char *)CF_UNDEFINED)
   {
   return;
   }

Debug("ReleaseCurrentLock(%s)\n",this.lock);

if (RemoveLock(this.lock) == -1)
   {
   Debug("Unable to remove lock %s\n",this.lock);
   free(this.last);
   free(this.lock);
   free(this.log);
   return;
   }

if (WriteLock(this.last) == -1)
   {
   CfOut(cf_error,"creat","Unable to create %s\n",this.last);
   free(this.last);
   free(this.lock);
   free(this.log);
   return;
   }
 
LogLockCompletion(this.log,getpid(),"Lock removed normally ",this.lock,"");

free(this.last);
free(this.lock);
free(this.log);
}


/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

time_t FindLock(char *last)

{ time_t mtime;

if ((mtime = FindLockTime(last)) == -1)
   {   
   /* Do this to prevent deadlock loops from surviving if IfElapsed > T_sched */
   
   if (WriteLock(last) == -1)
      {
      CfOut(cf_error,"","Unable to lock %s\n",last);
      return 0;
      }

   return 0;
   }
else
   {
   return mtime;
   }
}

/************************************************************************/

int WriteLock(char *name)

{ DB *dbp;
  struct LockData entry;

Debug("WriteLock(%s)\n",name);

if ((dbp = OpenLock()) == NULL)
   {
   return 0;
   }

DeleteDB(dbp,name);

entry.pid = getpid();
entry.time = time((time_t *)NULL); 

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD 
if (pthread_mutex_lock(&MUTEX_LOCK) != 0)
   {
   CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
   }
#endif

WriteDB(dbp,name,&entry,sizeof(entry));

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD
if (pthread_mutex_unlock(&MUTEX_LOCK) != 0)
   {
   CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
   }
#endif
 
CloseLock(dbp);
return 0; 
}

/*****************************************************************************/

void LogLockCompletion(char *cflog,int pid,char *str,char *operator,char *operand)

{ FILE *fp;
  char buffer[CF_MAXVARSIZE];
  struct stat statbuf;
  time_t tim;

Debug("LockLogCompletion(%s)\n",str);

if ((fp = fopen(cflog,"a")) == NULL)
   {
   CfOut(cf_error,"fopen","Can't open lock-log file %s\n",cflog);
   FatalError("");
   }

if ((tim = time((time_t *)NULL)) == -1)
   {
   Debug("Cfengine: couldn't read system clock\n");
   }

sprintf(buffer,"%s",ctime(&tim));

Chop(buffer);

fprintf(fp,"%s:%s:pid=%d:%s:%s\n",buffer,str,pid,operator,operand);

fclose(fp);

if (stat(cflog,&statbuf) != -1)
   {
   if (statbuf.st_size > CFLOGSIZE)
      {
      Verbose("Rotating lock-runlog file\n");
      RotateFiles(CFLOG,2);
      }
   }
}

/*****************************************************************************/

int RemoveLock(char *name)

{ DBT key;
  DB *dbp;

if ((dbp = OpenLock()) == NULL)
   {
   return 0;
   }

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD
if (pthread_mutex_lock(&MUTEX_LOCK) != 0)
   {
   CfOut(cf_error,"pthread_mutex_lock","pthread_mutex_lock failed");
   }
#endif 
  
DeleteDB(dbp,name);

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD
if (pthread_mutex_unlock(&MUTEX_LOCK) != 0)
   {
   CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
   }
#endif
 
CloseLock(dbp);
return 0; 
}

/************************************************************************/

time_t FindLockTime(char *name)

{ DB *dbp;
  struct LockData entry;

Debug("FindLockTime(%s)\n",name);
  
if ((dbp = OpenLock()) == NULL)
   {
   return -1;
   }

if (ReadDB(dbp,name,&entry,sizeof(entry)))
   {
   CloseLock(dbp);
   return entry.time;
   }
else
   {
   CloseLock(dbp);
   return -1;
   }
}

/************************************************************************/

pid_t FindLockPid(char *name)

{ DB *dbp;
  struct LockData entry;

if ((dbp = OpenLock()) == NULL)
   {
   return -1;
   }

if (ReadDB(dbp,name,&entry,sizeof(entry)))
   {
   CloseLock(dbp);
   return entry.pid;
   }
else
   {
   CloseLock(dbp);
   return -1;
   }
}

/************************************************************************/

DB *OpenLock()

{ char name[CF_BUFSIZE];
  DB *dbp;

snprintf(name,CF_BUFSIZE,"%s/cfengine_lock_db",CFWORKDIR);

Debug("OpenLock(%s)\n",name);

if ((errno = db_create(&dbp,NULL,0)) != 0)
   {
   CfOut(cf_error,"db_open","Couldn't open lock database %s\n",name);
   return NULL;
   }

#ifdef CF_OLD_DB
if ((errno = (dbp->open)(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = (dbp->open)(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)    
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open lock database %s\n",name);
   return NULL;
   }

return dbp;
}

/************************************************************************/

void CloseLock(DB *dbp)

{
dbp->close(dbp,0);
}
