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
/* File: pipes.c                                                             */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifndef MINGW
static FILE *Unix_cf_popen(char *command,char *type);
static FILE *Unix_cf_popensetuid(char *command,char *type,uid_t uid,gid_t gid,char *chdirv,char *chrootv);
static FILE *Unix_cf_popen_sh(char *command,char *type);
static FILE *Unix_cf_popen_shsetuid(char *command,char *type,uid_t uid,gid_t gid,char *chdirv,char *chrootv);
static int Unix_cf_pclose(FILE *pp);
static int Unix_cf_pclose_def(FILE *pfp,struct Attributes a,struct Promise *pp);
static int CfSetuid(uid_t uid,gid_t gid);
#endif

/*******************************************************************/
/* Pipe API - OS function mapping                                  */
/*******************************************************************/

FILE *cf_popen(char *command,char *type)

{ 
#ifdef MINGW
return NovaWin_cf_popen(command, type);
#else
return Unix_cf_popen(command, type);
#endif
}

/*****************************************************************************/

FILE *cf_popensetuid(char *command,char *type,uid_t uid,gid_t gid,char *chdirv,char *chrootv,int background)

{ 
#ifdef MINGW
return NovaWin_cf_popensetuid(command, type, uid, gid, chdirv, chrootv,background);
#else
return Unix_cf_popensetuid(command, type, uid, gid, chdirv, chrootv);
#endif
}

/*****************************************************************************/

FILE *cf_popen_sh(char *command,char *type)

{ 
#ifdef MINGW
return NovaWin_cf_popen_sh(command, type);
#else
return Unix_cf_popen_sh(command, type);
#endif
}

/*****************************************************************************/

FILE *cf_popen_shsetuid(char *command,char *type,uid_t uid,gid_t gid,char *chdirv,char *chrootv,int background)

{ 
#ifdef MINGW
return NovaWin_cf_popen_shsetuid(command, type, uid, gid, chdirv, chrootv, background);
#else
return Unix_cf_popen_shsetuid(command, type, uid, gid, chdirv, chrootv);
#endif
}

/*****************************************************************************/

int cf_pclose(FILE *pp)

{ 
#ifdef MINGW
return NovaWin_cf_pclose(pp);
#else
return Unix_cf_pclose(pp);
#endif
}

/*****************************************************************************/

int cf_pclose_def(FILE *pfp,struct Attributes a,struct Promise *pp)

{ 
#ifdef MINGW
return NovaWin_cf_pclose_def(pfp, a, pp);
#else
return Unix_cf_pclose_def(pfp, a, pp);
#endif
}

/*******************************************************************/
/* End pipe API                                                    */
/*******************************************************************/

int VerifyCommandRetcode(int retcode, int fallback, struct Attributes a, struct Promise *pp)
{
  char retcodeStr[128] = {0};
  int result = true;
  int matched = false;

if(a.classes.retcode_kept || a.classes.retcode_repaired || a.classes.retcode_failed)
  {
  
  snprintf(retcodeStr,sizeof(retcodeStr),"%d",retcode);
    
  if(KeyInRlist(a.classes.retcode_kept, retcodeStr))
    {
    cfPS(cf_inform,CF_NOP,"",pp,a,"-> Command related to promiser \"%s\" returned code defined as promise kept (%d)", pp->promiser, retcode);
    result = true;
    matched = true;
    }

  if(KeyInRlist(a.classes.retcode_repaired, retcodeStr))
    {
    cfPS(cf_inform,CF_CHG,"",pp,a,"-> Command related to promiser \"%s\" returned code defined as promise repaired (%d)", pp->promiser, retcode);
    result = true;
    matched = true;
    }

  if(KeyInRlist(a.classes.retcode_failed, retcodeStr))
    {
    cfPS(cf_inform,CF_FAIL,"",pp,a,"!! Command related to promiser \"%s\" returned code defined as promise failed (%d)", pp->promiser, retcode);
    result = false;
    matched = true;
    }

  if(!matched)
    {
    CfOut(cf_verbose, "", "Command related to promiser \"%s\" returned code %d -- did not match any failed, repaired or kept lists", pp->promiser, retcode);
    }

  }
 else if(fallback)  // default: 0 is success, != 0 is failure
   {
   if (retcode == 0)
     {
     cfPS(cf_verbose,CF_CHG,"",pp,a," -> Finished command related to promiser \"%s\" -- succeeded",pp->promiser);
     result = true;
     }
   else
     {
     cfPS(cf_inform,CF_FAIL,"",pp,a," !! Finished command related to promiser \"%s\" -- an error occurred (returned %d)",pp->promiser, retcode);
     result = false;
     }
   }

 return result;
}



#ifndef MINGW

/*******************************************************************/
/* Unix implementations                                            */
/*******************************************************************/

/*****************************************************************************/

pid_t *CHILDREN;
int    MAX_FD = 128; /* Max number of simultaneous pipes */

/*****************************************************************************/

static FILE *Unix_cf_popen(char *command,char *type)

 { static char arg[CF_MAXSHELLARGS][CF_BUFSIZE];
   int i, argc, pd[2];
   char **argv;
   pid_t pid;
   FILE *pp = NULL;

Debug("Unix_cf_popen(%s)\n",command);

if ((*type != 'r' && *type != 'w') || (type[1] != '\0'))
   {
   errno = EINVAL;
   return NULL;
   }

if (!ThreadLock(cft_count))
   {
   return NULL;
   }

if (CHILDREN == NULL)   /* first time */
   {
   if ((CHILDREN = calloc(MAX_FD,sizeof(pid_t))) == NULL)
      {
      ThreadUnlock(cft_count);
      return NULL;
      }
   }

ThreadUnlock(cft_count);

if (pipe(pd) < 0)        /* Create a pair of descriptors to this process */
   {
   return NULL;
   }

if ((pid = fork()) == -1)
   {
   close(pd[0]);
   close(pd[1]);
   return NULL;
   }

signal(SIGCHLD,SIG_DFL);

ALARM_PID = (pid != 0 ? pid : -1);

if (pid == 0)
   {
   switch (*type)
      {
      case 'r':
          
          close(pd[0]);        /* Don't need output from parent */
          
          if (pd[1] != 1)
             {
             dup2(pd[1],1);    /* Attach pp=pd[1] to our stdout */
             dup2(pd[1],2);    /* Merge stdout/stderr */
             close(pd[1]);
             }
          
          break;
          
      case 'w':
          
          close(pd[1]);
          
          if (pd[0] != 0)
             {
             dup2(pd[0],0);
             close(pd[0]);
             }
       }
   
   for (i = 0; i < MAX_FD; i++)
      {
      if (CHILDREN[i] > 0)
         {
         close(i);
         }
      }
   
   argc = ArgSplitCommand(command,arg);
   argv = (char **) malloc((argc+1)*sizeof(char *));
   
   if (argv == NULL)
      {
      FatalError("Out of memory");
      }
   
   for (i = 0; i < argc; i++)
      {
      argv[i] = arg[i];
      }
   
   argv[i] = (char *) NULL;
   
   if (execv(arg[0],argv) == -1)
      {
      CfOut(cf_error,"execv","Couldn't run %s",arg[0]);
      }
   
   free((char *)argv);
   _exit(1);
   }
else
   {
   switch (*type)
      {
      case 'r':
          
          close(pd[1]);
          
          if ((pp = fdopen(pd[0],type)) == NULL)
             {
             cf_pwait(pid);
             return NULL;
             }
          break;
          
      case 'w':
          
          close(pd[0]);
          
          if ((pp = fdopen(pd[1],type)) == NULL)
             {
             cf_pwait(pid);
             return NULL;
             }
      }
   
   if (fileno(pp) >= MAX_FD)
      {
      CfOut(cf_error,"","File descriptor %d of child %d higher than MAX_FD in Unix_cf_popen, check for defunct children", fileno(pp), pid);
      }
   else
      {
      ThreadLock(cft_count);
      CHILDREN[fileno(pp)] = pid;
      ThreadUnlock(cft_count);
      }

   return pp;
   }

return NULL; /* Cannot reach here */
}

/*****************************************************************************/

static FILE *Unix_cf_popensetuid(char *command,char *type,uid_t uid,gid_t gid,char *chdirv,char *chrootv)
    
 { static char arg[CF_MAXSHELLARGS][CF_BUFSIZE];
   int i, argc, pd[2];
   char **argv;
   pid_t pid;
   FILE *pp = NULL;

Debug("Unix_cf_popensetuid(%s,%s,%d,%d)\n",command,type,uid,gid);

if ((*type != 'r' && *type != 'w') || (type[1] != '\0'))
   {
   errno = EINVAL;
   return NULL;
   }

if (!ThreadLock(cft_count))
   {
   return NULL;
   }

if (CHILDREN == NULL)   /* first time */
   {
   if ((CHILDREN = calloc(MAX_FD,sizeof(pid_t))) == NULL)
      {
      ThreadUnlock(cft_count);
      return NULL;
      }
   }

ThreadUnlock(cft_count);

if (pipe(pd) < 0)        /* Create a pair of descriptors to this process */
   {
   return NULL;
   }

if ((pid = fork()) == -1)
   {
   close(pd[0]);
   close(pd[1]);
   return NULL;
   }

signal(SIGCHLD,SIG_DFL);
ALARM_PID = (pid != 0 ? pid : -1);

if (pid == 0)
   {
   switch (*type)
      {
      case 'r':
          
          close(pd[0]);        /* Don't need output from parent */
          
          if (pd[1] != 1)
             {
             dup2(pd[1],1);    /* Attach pp=pd[1] to our stdout */
             dup2(pd[1],2);    /* Merge stdout/stderr */
             close(pd[1]);
             }
          
          break;
          
      case 'w':
          
          close(pd[1]);
          
          if (pd[0] != 0)
             {
             dup2(pd[0],0);
             close(pd[0]);
             }
      }
   
   for (i = 0; i < MAX_FD; i++)
      {
      if (CHILDREN[i] > 0)
         {
         close(i);
         }
      }
   
   argc = ArgSplitCommand(command,arg);
   argv = (char **) malloc((argc+1)*sizeof(char *));
   
   if (argv == NULL)
      {
      FatalError("Out of memory");
      }
   
   for (i = 0; i < argc; i++)
      {
      argv[i] = arg[i];
      }
   
   argv[i] = (char *) NULL;
   
   if (chrootv && strlen(chrootv) != 0)
      {
      if (chroot(chrootv) == -1)
         {
         CfOut(cf_error,"chroot","Couldn't chroot to %s\n",chrootv);
         free((char *)argv);
         return NULL;
         }
      }
   
   if (chdirv && strlen(chdirv) != 0)
      {
      if (chdir(chdirv) == -1)
         {
         CfOut(cf_error,"chdir","Couldn't chdir to %s\n",chdirv);
         free((char *)argv);
         return NULL;
         }
      }
   
   if (!CfSetuid(uid,gid))
      {
      free((char *)argv);
      _exit(1);
      }
   
   if (execv(arg[0],argv) == -1)
      {
      CfOut(cf_error,"execv","Couldn't run %s",arg[0]);
      }
   
   free((char *)argv);
   _exit(1);
   }
else
   {
   switch (*type)
      {
      case 'r':
          
          close(pd[1]);
          
          if ((pp = fdopen(pd[0],type)) == NULL)
             {
             cf_pwait(pid);
             return NULL;
             }
          break;
          
      case 'w':
          
          close(pd[0]);
          
          if ((pp = fdopen(pd[1],type)) == NULL)
             {
             cf_pwait(pid);
             return NULL;
             }
      }
   
   if (fileno(pp) >= MAX_FD)
      {
      CfOut(cf_error,"","File descriptor %d of child %d higher than MAX_FD in Unix_cf_popensetuid, check for defunct children", fileno(pp), pid);
      }
   else
      {
      ThreadLock(cft_count);
      CHILDREN[fileno(pp)] = pid;
      ThreadUnlock(cft_count);
      }
   
   return pp;
   }

return NULL; /* cannot reach here */
}

/*****************************************************************************/
/* Shell versions of commands - not recommended for security reasons         */
/*****************************************************************************/

static FILE *Unix_cf_popen_sh(char *command,char *type)
    
 { int i,pd[2];
   pid_t pid;
   FILE *pp = NULL;
   char esc_command[CF_BUFSIZE];

Debug("Unix_cf_popen_sh(%s)\n",command);

if ((*type != 'r' && *type != 'w') || (type[1] != '\0'))
   {
   errno = EINVAL;
   return NULL;
   }

if (!ThreadLock(cft_count))
   {
   return NULL;
   }

if (CHILDREN == NULL)   /* first time */
   {
   if ((CHILDREN = calloc(MAX_FD,sizeof(pid_t))) == NULL)
      {
      ThreadUnlock(cft_count);
      return NULL;
      }
   }

ThreadUnlock(cft_count);

if (pipe(pd) < 0)        /* Create a pair of descriptors to this process */
   {
   return NULL;
   }

if ((pid = fork()) == -1)
   {
   close(pd[0]);
   close(pd[1]);
   return NULL;
   }

signal(SIGCHLD,SIG_DFL);
ALARM_PID = (pid != 0 ? pid : -1);

if (pid == 0)
   {
   switch (*type)
      {
      case 'r':
          
          close(pd[0]);        /* Don't need output from parent */
          
          if (pd[1] != 1)
             {
             dup2(pd[1],1);    /* Attach pp=pd[1] to our stdout */
             dup2(pd[1],2);    /* Merge stdout/stderr */
             close(pd[1]);
             }
          
          break;
          
      case 'w':
          
          close(pd[1]);
          
          if (pd[0] != 0)
             {
             dup2(pd[0],0);
             close(pd[0]);
             }
      }
   
   for (i = 0; i < MAX_FD; i++)
      {
      if (CHILDREN[i] > 0)
         {
         close(i);
         }
      }

   strncpy(esc_command,ShEscapeCommand(command),CF_BUFSIZE-1);
   execl("/bin/sh","sh","-c",esc_command,NULL);
   _exit(1);
   }
else
   {
   switch (*type)
      {
      case 'r':
          
          close(pd[1]);
          
          if ((pp = fdopen(pd[0],type)) == NULL)
             {
             cf_pwait(pid);
             return NULL;
             }
          break;
          
      case 'w':
          
          close(pd[0]);
          
          if ((pp = fdopen(pd[1],type)) == NULL)
             {
             cf_pwait(pid);
             return NULL;
             }
      }
   
   if (fileno(pp) >= MAX_FD)
      {
      CfOut(cf_error,"","File descriptor %d of child %d higher than MAX_FD in Unix_cf_popen_sh, check for defunct children", fileno(pp), pid);
      }
   else
      {
      ThreadLock(cft_count);
      CHILDREN[fileno(pp)] = pid;
      ThreadUnlock(cft_count);
      }
   
   return pp;
   }

return NULL;
}

/******************************************************************************/

static FILE *Unix_cf_popen_shsetuid(char *command,char *type,uid_t uid,gid_t gid,char *chdirv,char *chrootv)
    
 { int i,pd[2];
   pid_t pid;
   FILE *pp = NULL;
   char esc_command[CF_BUFSIZE];

Debug("Unix_cf_popen_shsetuid(%s,%s,%d,%d)\n",command,type,uid,gid);

if ((*type != 'r' && *type != 'w') || (type[1] != '\0'))
   {
   errno = EINVAL;
   return NULL;
   }

if (!ThreadLock(cft_count))
   {
   return NULL;
   }

if (CHILDREN == NULL)   /* first time */
   {
   if ((CHILDREN = calloc(MAX_FD,sizeof(pid_t))) == NULL)
      {
      ThreadUnlock(cft_count);
      return NULL;
      }
   }

ThreadUnlock(cft_count);

if (pipe(pd) < 0)        /* Create a pair of descriptors to this process */
   {
   return NULL;
   }

if ((pid = fork()) == -1)
   {
   close(pd[0]);
   close(pd[1]);
   return NULL;
   }

signal(SIGCHLD,SIG_DFL);
ALARM_PID = (pid != 0 ? pid : -1);

if (pid == 0)
   {
   switch (*type)
      {
      case 'r':
          
          close(pd[0]);        /* Don't need output from parent */
          
          if (pd[1] != 1)
             {
             dup2(pd[1],1);    /* Attach pp=pd[1] to our stdout */
             dup2(pd[1],2);    /* Merge stdout/stderr */
             close(pd[1]);
             }
          
          break;
          
      case 'w':
          
          close(pd[1]);
          
          if (pd[0] != 0)
             {
             dup2(pd[0],0);
             close(pd[0]);
             }
      }
   
   for (i = 0; i < MAX_FD; i++)
      {
      if (CHILDREN[i] > 0)
         {
         close(i);
         }
      }
   
   if (chrootv && strlen(chrootv) != 0)
      {
      if (chroot(chrootv) == -1)
         {
         CfOut(cf_error,"chroot","Couldn't chroot to %s\n",chrootv);
         return NULL;
         }
      }
   
   if (chdirv && strlen(chdirv) != 0)
      {
      if (chdir(chdirv) == -1)
         {
         CfOut(cf_error,"chdir","Couldn't chdir to %s\n",chdirv);
         return NULL;
         }
      }
   
   if (!CfSetuid(uid,gid))
      {
      _exit(1);
      }

   strncpy(esc_command,ShEscapeCommand(command),CF_BUFSIZE-1);
   execl("/bin/sh","sh","-c",esc_command,NULL);
   _exit(1);
   }
else
   {
   switch (*type)
      {
      case 'r':
          
          close(pd[1]);
          
          if ((pp = fdopen(pd[0],type)) == NULL)
             {
             cf_pwait(pid);
             return NULL;
             }
          break;
          
      case 'w':
          
          close(pd[0]);
          
          if ((pp = fdopen(pd[1],type)) == NULL)
             {
             cf_pwait(pid);
             return NULL;
             }
      }
   
   if (fileno(pp) >= MAX_FD)
      {
      CfOut(cf_error,"","File descriptor %d of child %d higher than MAX_FD in Unix_cf_popen_shsetuid, check for defunct children", fileno(pp), pid);
      cf_pwait(pid);
      return NULL;
      }
   else
      {
      ThreadLock(cft_count);
      CHILDREN[fileno(pp)] = pid;
      ThreadUnlock(cft_count);
      }
   return pp;
   }

return NULL;
}


/******************************************************************************/
/* Close commands                                                             */
/******************************************************************************/

int cf_pwait(pid_t pid)

{ int status;

Debug("cf_pwait - Waiting for process %d\n",pid); 

#ifdef HAVE_WAITPID

while(waitpid(pid,&status,0) < 0)
   {
   if (errno != EINTR)
      {
      return -1;
      }
   }

if (!WIFEXITED(status))
   {
   return -1;
   }

return WEXITSTATUS(status);
 
#else

while ((wait_result = wait(&status)) != pid)
   {
   if (wait_result <= 0)
      {
      CfOut(cf_inform,"wait"," !! Wait for child failed\n");
      return -1;
      }
   }
 
if (WIFSIGNALED(status))
   {
   return -1;
   }
 
if (!WIFEXITED(status))
   {
   return -1;
   }
 
return (WEXITSTATUS(status));
#endif
}

/*******************************************************************/

static int Unix_cf_pclose(FILE *pp)

{ int fd;
  pid_t pid;

Debug("Unix_cf_pclose(pp)\n");

if (!ThreadLock(cft_count))
   {
   return -1;
   }

if (CHILDREN == NULL)  /* popen hasn't been called */
   {
   ThreadUnlock(cft_count);
   return -1;
   }

ThreadUnlock(cft_count);

ALARM_PID = -1;
fd = fileno(pp);

if (fd >= MAX_FD)
   {
   CfOut(cf_error,"","File descriptor %d of child higher than MAX_FD in Unix_cf_pclose, check for defunct children", fd);
   pid = -1;
   }
else
   {
   if ((pid = CHILDREN[fd]) == 0)
      {
      return -1;
      }

   ThreadLock(cft_count);
   CHILDREN[fd] = 0;
   ThreadUnlock(cft_count);
   }

if (fclose(pp) == EOF)
   {
   return -1;
   }

return cf_pwait(pid);
}

/*******************************************************************/

static int Unix_cf_pclose_def(FILE *pfp,struct Attributes a,struct Promise *pp)
/**
 * Defines command failure/success with cfPS based on exit code.
 */
{ int fd, status;
  pid_t pid;

Debug("Unix_cf_pclose_def(pfp)\n");

if (!ThreadLock(cft_count))
   {
   return -1;
   }

if (CHILDREN == NULL)  /* popen hasn't been called */
   {
   ThreadUnlock(cft_count);
   return -1;
   }

ThreadUnlock(cft_count);

ALARM_PID = -1;
fd = fileno(pfp);

if (fd >= MAX_FD)
   {
   CfOut(cf_error,"","File descriptor %d of child higher than MAX_FD in Unix_cf_pclose_def, check for defunct children", fd);
   fclose(pfp);
   return -1;
   }

if ((pid = CHILDREN[fd]) == 0)
   {
   return -1;
   }

ThreadLock(cft_count);
CHILDREN[fd] = 0;
ThreadUnlock(cft_count);

if (fclose(pfp) == EOF)
   {
   return -1;
   }

Debug("Unix_cf_pclose_def - Waiting for process %d\n",pid); 

#ifdef HAVE_WAITPID

while(waitpid(pid,&status,0) < 0)
   {
   if (errno != EINTR)
      {
      return -1;
      }
   }

if (!WIFEXITED(status))
   {
   cfPS(cf_inform,CF_FAIL,"",pp,a," !! Finished script \"%s\" - failed (abnormal termination)",pp->promiser);
   return -1;
   }

VerifyCommandRetcode(WEXITSTATUS(status),true,a,pp);

return status; 
 
#else

while ((wait_result = wait(&status)) != pid)
   {
   if (wait_result <= 0)
      {
      CfOut(cf_inform,"wait","Wait for child failed\n");
      return -1;
      }
   }
 
if (WIFSIGNALED(status))
   {
   cfPS(cf_inform,CF_INTERPT,"",pp,a," -> Finished script - interrupted %s\n",pp->promiser);
   return -1;
   }

if (!WIFEXITED(status))
   {
   cfPS(cf_inform,CF_FAIL,"",pp,a," !! Finished script \"%s\" - failed (abnormal termination)",pp->promiser);
   return -1;
   }

VerifyCommandRetcode(WEXITSTATUS(status),true,a,pp);

return (WEXITSTATUS(status));
#endif
}

/*******************************************************************/

static int CfSetuid(uid_t uid,gid_t gid)

{ struct passwd *pw;
 
if (gid != (gid_t) -1)
   {
   CfOut(cf_verbose,"","Changing gid to %d\n",gid);      
   
   if (setgid(gid) == -1)
      {
      CfOut(cf_error,"setgid","Couldn't set gid to %d\n",gid);
      return false;
      }

   /* Now eliminate any residual privileged groups */
   
   if ((pw = getpwuid(uid)) == NULL)
      {
      CfOut(cf_error,"getpwuid","Unable to get login groups when dropping privilege to %d",uid);
      return false;
      }
   
   if (initgroups(pw->pw_name, pw->pw_gid) == -1)
      {
      CfOut(cf_error,"initgroups","Unable to set login groups when dropping privilege to %s=%d",pw->pw_name,uid);
      return false;
      }
   }

if (uid != (uid_t) -1)
   {
   CfOut(cf_verbose,"","Changing uid to %d\n",uid);
   
   if (setuid(uid) == -1)
      {
      CfOut(cf_error,"setuid","Couldn't set uid to %d\n",uid);
      return false;
      }
   }

return true;
}

#endif  /* NOT MINGW */
