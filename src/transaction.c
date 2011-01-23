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
/* File: transaction.c                                                       */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void SummarizeTransaction(struct Attributes attr,struct Promise *pp,char *logname)

{ FILE *fout;

if (logname && attr.transaction.log_string)
   {
   if (strcmp(logname,"udp_syslog") == 0)
      {
      RemoteSyslog(attr,pp);
      }
   else if (strcmp(logname,"stdout") == 0)
      {
      CfOut(cf_reporting,"","L: %s\n",attr.transaction.log_string);
      }
   else
      {
      if ((fout = fopen(logname,"a")) == NULL)
         {
         CfOut(cf_error,"","Unable to open private log %s",logname);
         return;
         }

      CfOut(cf_verbose,""," -> Logging string \"%s\" to %s\n",attr.transaction.log_string,logname);
      fprintf(fout,"%s\n",attr.transaction.log_string);

      fclose(fout);
      }

   attr.transaction.log_string = NULL; /* To avoid repetition */
   }
else if (attr.transaction.log_failed)
   {
   if (strcmp(logname,attr.transaction.log_failed) == 0)
      {
      cfPS(cf_log,CF_NOP,"",pp,attr,"%s",attr.transaction.log_string);
      }
   }
}

/*****************************************************************************/

struct CfLock AcquireLock(char *operand,char *host,time_t now,struct Attributes attr,struct Promise *pp, int ignoreProcesses)

{ unsigned int pid;
  int i, err, sum=0;
  time_t lastcompleted = 0, elapsedtime;
  char *promise,cc_operator[CF_BUFSIZE],cc_operand[CF_BUFSIZE];
  char cflock[CF_BUFSIZE],cflast[CF_BUFSIZE],cflog[CF_BUFSIZE];
  char str_digest[CF_BUFSIZE];
  struct CfLock this;
  unsigned char digest[EVP_MAX_MD_SIZE+1];

this.last = (char *) CF_UNDEFINED;
this.lock = (char *) CF_UNDEFINED;
this.log  = (char *) CF_UNDEFINED;

if (now == 0)
   {
   return this;
   }

this.last = NULL;
this.lock = NULL;
this.log = NULL;

/* Indicate as done if we tried ... as we have passed all class
   constraints now but we should only do this for level 0
   promises. Sub routine bundles cannot be marked as done or it will
   disallow iteration over bundles */

if (pp->done)
   {
   return this;
   }

if (CF_STCKFRAME == 1)
   {
   *(pp->donep) = true;
   /* Must not set pp->done = true for editfiles etc */
   }

HashPromise(operand,pp,digest,CF_DEFAULT_DIGEST);
strcpy(str_digest,HashPrint(CF_DEFAULT_DIGEST,digest));

/* As a backup to "done" we need something immune to re-use */

if (THIS_AGENT_TYPE == cf_agent)
   {
   if (IsItemIn(DONELIST,str_digest))
      {
      CfOut(cf_verbose,""," -> This promise has already been verified");
      return this;
      }

   PrependItem(&DONELIST,str_digest,NULL);
   }

/* Finally if we're supposed to ignore locks ... do the remaining stuff */

if (IGNORELOCK)
   {
   this.lock = strdup("dummy");
   return this;
   }

promise = BodyName(pp);
snprintf(cc_operator,CF_MAXVARSIZE-1,"%s-%s",promise,host);
strncpy(cc_operand,CanonifyName(operand),CF_BUFSIZE-1);
RemoveDates(cc_operand);

free(promise);

Debug("AcquireLock(%s,%s), ExpireAfter=%d, IfElapsed=%d\n",cc_operator,cc_operand,attr.transaction.expireafter,attr.transaction.ifelapsed);

for (i = 0; cc_operator[i] != '\0'; i++)
   {
   sum = (CF_MACROALPHABET * sum + cc_operator[i]) % CF_HASHTABLESIZE;
   }

for (i = 0; cc_operand[i] != '\0'; i++)
   {
   sum = (CF_MACROALPHABET * sum + cc_operand[i]) % CF_HASHTABLESIZE;
   }

snprintf(cflog,CF_BUFSIZE,"%s/cf3.%.40s.runlog",CFWORKDIR,host);
snprintf(cflock,CF_BUFSIZE,"lock.%.100s.%s.%.100s_%d_%s",pp->bundle,cc_operator,cc_operand,sum,str_digest);
snprintf(cflast,CF_BUFSIZE,"last.%.100s.%s.%.100s_%d_%s",pp->bundle,cc_operator,cc_operand,sum,str_digest);

Debug("LOCK(%s)[%s]\n",pp->bundle,cflock);

/* for signal handler - not threadsafe so applies only to main thread */

CFINITSTARTTIME = time(NULL);

/* Look for non-existent (old) processes */

lastcompleted = FindLock(cflast);
elapsedtime = (time_t)(now-lastcompleted) / 60;

if (elapsedtime < 0)
   {
   CfOut(cf_verbose,""," XX Another cfengine seems to have done this since I started (elapsed=%d)\n",elapsedtime);
   return this;
   }

if (elapsedtime < attr.transaction.ifelapsed)
   {
   CfOut(cf_verbose,""," XX Nothing promised here [%.40s] (%u/%u minutes elapsed)\n",cflock,elapsedtime,attr.transaction.ifelapsed);
   return this;
   }

/* Look for existing (current) processes */

if (!ignoreProcesses)
   {
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
#ifdef MINGW  // killing processes with e.g. task manager does not allow for termination handling
         else if(!NovaWin_IsProcessRunning(pid))
            {
            CfOut(cf_verbose,"","Process with pid %d is not running - ignoring lock (Windows does not support graceful processes termination)\n",pid);
            LogLockCompletion(cflog,pid,"Lock expired, process not running",cc_operator,cc_operand);
            unlink(cflock);
            }
#endif  /* MINGW */
         else
	       {
               CfOut(cf_verbose,"","Trying to kill expired process, pid %d\n",pid);
               
               err = GracefulTerminate(pid);
               
               if (err || errno == ESRCH)
                  {
                  LogLockCompletion(cflog,pid,"Lock expired, process killed",cc_operator,cc_operand);
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
            CfOut(cf_verbose,"","Couldn't obtain lock for %s (already running!)\n",cflock);
            return this;
            }
      }
   
   WriteLock(cflock);   
   }

this.lock = strdup(cflock);
this.last = strdup(cflast);
this.log  = strdup(cflog);

/* Keep this as a global for signal handling */
strcpy(CFLOCK,cflock);
strcpy(CFLAST,cflast);
strcpy(CFLOG,cflog);

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

Debug("Yielding lock %s\n",this.lock);

if (RemoveLock(this.lock) == -1)
   {
   CfOut(cf_verbose,"","Unable to remove lock %s\n",this.lock);
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

/************************************************************************/

void GetLockName(char *lockname,char *locktype,char *base,struct Rlist *params)

{ struct Rlist *rp;
 int max_sample, count = 0;

for (rp = params; rp != NULL; rp=rp->next)
   {
   count++;
   }

if (count)
   {
   max_sample = CF_BUFSIZE / (2*count);
   }
else
   {
   max_sample = 0;
   }

strncpy(lockname,locktype,CF_BUFSIZE/10);
strcat(lockname,"_");
strncat(lockname,base,CF_BUFSIZE/10);
strcat(lockname,"_");

for (rp = params; rp != NULL; rp=rp->next)
   {
   strncat(lockname,(char *)rp->item,max_sample);
   }
}

/************************************************************************/

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

pthread_mutex_t *NameToThreadMutex(enum cf_thread_mutex name)

{
switch(name)
   {
   case cft_system:
       return &MUTEX_SYSCALL;
       break;
       
   case cft_count:
       return &MUTEX_COUNT;
       break;
       
   case cft_getaddr:
       return &MUTEX_GETADDR;
       break;
       
   case cft_lock:
       return &MUTEX_LOCK;
       break;
       
   case cft_output:
       return &MUTEX_OUTPUT;
       break;
       
   case cft_dbhandle:
       return &MUTEX_DBHANDLE;
       break;
       
   case cft_policy:
       return &MUTEX_POLICY;
       break;
       
   case cft_db_lastseen:
       return &MUTEX_DB_LASTSEEN;
       break;

   case cft_report:
       return &MUTEX_DB_REPORT;
       break;

   case cft_vscope:
       return &MUTEX_VSCOPE;
       break;

   case cft_server_keyseen:
       return &MUTEX_SERVER_KEYSEEN;
       break;

       
   default:
       CfOut(cf_error, "", "!! NameToThreadMutex supplied with unknown mutex name: %d", name);
       FatalError("Internal software error\n");
       break;
   }

return NULL;
}

#endif

/************************************************************************/

int ThreadLock(enum cf_thread_mutex name)

{
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
pthread_mutex_t *mutex;

mutex = NameToThreadMutex(name);

if (pthread_mutex_lock(mutex) != 0)
   {
   // Don't use CfOut here as it also requires locking
   printf("!! Could not lock: %d", name);
   return false;
   }

return true;

#else  // NOT_HAVE_PTHREAD

return true;

#endif

}

/************************************************************************/

int ThreadUnlock(enum cf_thread_mutex name)

{
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
pthread_mutex_t *mutex;

mutex = NameToThreadMutex(name);

if (pthread_mutex_unlock(mutex) != 0)
   {
   // Don't use CfOut here as it also requires locking
   printf("pthread_mutex_unlock","pthread_mutex_unlock failed");
   return false;
   }

return true;

#else  // NOT_HAVE_PTHREAD 

return true;

#endif
}

/*****************************************************************************/

void AssertThreadLocked(enum cf_thread_mutex name, char *fname)

/* Verifies that a given lock is taken (not neccessary by the current thread) */

{
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
pthread_mutex_t *mutex;
int status;

mutex = NameToThreadMutex(name);

status = pthread_mutex_trylock(mutex);

if (status != EBUSY && status != EDEADLK)
   {
   CfOut(cf_error, "", "!! The mutex %d was not locked in %s() -- status=%d", name, fname, status);
   FatalError("Software assertion failure\n");
   }

#endif
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

{ CF_DB *dbp;
  struct LockData entry;

Debug("WriteLock(%s)\n",name);

if ((dbp = OpenLock()) == NULL)
   {
   return -1;
   }

entry.pid = getpid();
entry.time = time((time_t *)NULL);

ThreadLock(cft_lock);
WriteDB(dbp,name,&entry,sizeof(entry));
ThreadUnlock(cft_lock);

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

if (cflog == NULL)
   {
   return;
   }

if ((fp = fopen(cflog,"a")) == NULL)
   {
   CfOut(cf_error,"fopen","Can't open lock-log file %s\n",cflog);
   exit(1);
   }

if ((tim = time((time_t *)NULL)) == -1)
   {
   Debug("Cfengine: couldn't read system clock\n");
   }

sprintf(buffer,"%s",cf_ctime(&tim));

Chop(buffer);

fprintf(fp,"%s:%s:pid=%d:%s:%s\n",buffer,str,pid,operator,operand);

fclose(fp);

if (cfstat(cflog,&statbuf) != -1)
   {
   if (statbuf.st_size > CFLOGSIZE)
      {
      CfOut(cf_verbose,"","Rotating lock-runlog file\n");
      RotateFiles(cflog,2);
      }
   }
}

/*****************************************************************************/

int RemoveLock(char *name)

{ CF_DB *dbp;

if ((dbp = OpenLock()) == NULL)
   {
   return -1;
   }

ThreadLock(cft_lock);
DeleteDB(dbp,name);
ThreadUnlock(cft_lock);

CloseLock(dbp);
return 0;
}

/************************************************************************/

time_t FindLockTime(char *name)

{ CF_DB *dbp;
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

{ CF_DB *dbp;
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

CF_DB *OpenLock()

{ char name[CF_BUFSIZE];
  CF_DB *dbp;

snprintf(name,CF_BUFSIZE,"%s/state/%s",CFWORKDIR, CF_LOCKDB_FILE);
MapName(name);

if (!OpenDB(name,&dbp))
   {
   return NULL;
   }

Debug("OpenLock(%s)\n",name);

return dbp;
}

/************************************************************************/

void CloseLock(CF_DB *dbp)

{
if (dbp)
   {
   CloseDB(dbp);
   }
}

/*****************************************************************************/

void RemoveDates(char *s)

{ int i,a = 0,b = 0,c = 0,d = 0;
  char *dayp = NULL, *monthp = NULL, *sp;
  char *days[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
  char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// Canonifies or blanks our times/dates for locks where there would be an explosion of state
  
if (strlen(s) < strlen("Fri Oct 1 15:15:23 EST 2010"))
   {
   // Probably not a full date
   return;
   }

for (i = 0; i < 7; i++)
   {
   if (dayp = strstr(s,days[i]))
      {
      *dayp = 'D';
      *(dayp+1) = 'A';
      *(dayp+2) = 'Y';
      break;
      }
   }

for (i = 0; i < 12; i++)
   {
   if (monthp = strstr(s,months[i]))
      {
      *monthp = 'M';
      *(monthp+1) = 'O';
      *(monthp+2) = 'N';
      break;
      }
   }

if (dayp && monthp) // looks like a full date
   {
   sscanf(monthp+4,"%d %d:%d:%d",&a,&b,&c,&d);

   if (a*b*c*d == 0)
      {
      // Probably not a date
      return;
      }

   for (sp = monthp+4; *sp != '\0'; sp++)
      {
      if (sp > monthp+15)
         {
         break;
         }
      
      if (isdigit(*sp))
         {
         *sp = 't';
         }
      }
   }
}

/************************************************************************/

void PurgeLocks()

{ CF_DB *dbp = OpenLock();
  CF_DBC *dbcp;
  char *key,name[CF_BUFSIZE];
  int i,ksize,vsize;
  struct LockData entry;
  time_t now = time(NULL);

memset(&entry, 0, sizeof(entry)); 

if (ReadDB(dbp,"lock_horizon",&entry,sizeof(entry)))
   {
   if (now - entry.time < CF_MONTH)
      {
      CfOut(cf_verbose,""," -> No lock purging scheduled");
      CloseLock(dbp);
      return;
      }
   }

CfOut(cf_verbose,""," -> Looking for stale locks to purge");

if (!NewDBCursor(dbp,&dbcp))
   {
   CloseLock(dbp);
   return;
   }

while(NextDB(dbp,dbcp,&key,&ksize,(void *)&entry,&vsize))
   {
   if (strncmp(key,"last.internal_bundle.track_license.handle",
               strlen("last.internal_bundle.track_license.handle")) == 0)
      {
      continue;
      }

   if (now - entry.time > (time_t)CF_LOCKHORIZON)
      {
      CfOut(cf_verbose,""," --> Purging lock (%d) %s",now-entry.time,key);
      DeleteDB(dbp,key);
      }
   }

entry.time = now;
WriteDB(dbp,"lock_horizon",&entry,sizeof(entry));

DeleteDBCursor(dbp,dbcp);
CloseLock(dbp);
}
