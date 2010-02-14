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
      printf("L: %s\n",attr.transaction.log_string);
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

struct CfLock AcquireLock(char *operand,char *host,time_t now,struct Attributes attr,struct Promise *pp)

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

HashPromise(operand,pp,digest,cf_md5);
strcpy(str_digest,HashPrint(cf_md5,digest));

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
strncpy(cc_operator,promise,CF_MAXVARSIZE-1);
strncpy(cc_operand,CanonifyName(operand),CF_BUFSIZE-1);
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
   CfOut(cf_verbose,""," XX Nothing promised here [%.30s] (%u/%u minutes elapsed)\n",cflock,elapsedtime,attr.transaction.ifelapsed);
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

strncat(lockname,locktype,CF_BUFSIZE/10);
strcat(lockname,"_");
strncat(lockname,base,CF_BUFSIZE/10);
strcat(lockname,"_");

for (rp = params; rp != NULL; rp=rp->next)
   {
   strncat(lockname,(char *)rp->item,max_sample);
   }
}

/************************************************************************/

int ThreadLock(enum cf_thread_mutex name)

{ int val = 0;

switch(name)
   {
   case cft_system:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_lock(&MUTEX_SYSCALL)) != 0)
          {
          CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
          }
#endif

       break;

   case cft_count:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_lock(&MUTEX_COUNT)) != 0)
          {
          CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
          }
#endif

       break;

   case cft_lock:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_lock(&MUTEX_LOCK)) != 0)
          {
          CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
          }
#endif

       break;

   case cft_getaddr:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_lock(&MUTEX_GETADDR)) != 0)
          {
          CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
          }
#endif
       break;

   case cft_output:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_lock(&MUTEX_OUTPUT)) != 0)
          {
          CfOut(cf_error,"unlock","pthread_mutex_unlock failed");
          }
#endif
       break;


   default:
       break;
  }

return (val == 0);
}

/************************************************************************/

int ThreadUnlock(enum cf_thread_mutex name)

{ int val = 0;

switch(name)
   {
   case cft_system:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
       if ((val = pthread_mutex_unlock(&MUTEX_SYSCALL)) != 0)
          {
          CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
          }
#endif
       break;

   case cft_count:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_unlock(&MUTEX_COUNT)) != 0)
          {
          CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
          }
#endif
   break;

   case cft_lock:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_unlock(&MUTEX_LOCK)) != 0)
          {
          CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
          }
#endif
   break;

   case cft_getaddr:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_unlock(&MUTEX_GETADDR)) != 0)
          {
          CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
          }
#endif
   break;

   case cft_output:

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

       if ((val = pthread_mutex_unlock(&MUTEX_OUTPUT)) != 0)
          {
          CfOut(cf_error,"pthread_mutex_unlock","pthread_mutex_unlock failed");
          }
#endif
       break;


   }

return (val == 0);
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
