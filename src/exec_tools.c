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
/* File: exec_tools.c                                                        */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*************************************************************/

int IsExecutable(char *file)

{ struct stat sb;
  gid_t grps[NGROUPS];
  int i,n;

if (stat(file,&sb) == -1)
   {
   CfOut(cf_error,"","Proposed executable %s doesn't exist",file);
   return false;
   }
  
if (getuid() == sb.st_uid)
   {
   if (sb.st_mode && 0100)
      {
      return true;
      }
   }
else if (getgid() == sb.st_gid)
   {
   if (sb.st_mode && 0010)
      {
      return true;
      }    
   }
else
   {
   if (sb.st_mode && 0001)
      {
      return true;
      }
   
   if ((n = getgroups(NGROUPS,grps)) > 0)
      {
      for (i = 0; i < n; i++)
         {
         if (grps[i] == sb.st_gid)
            {
            if (sb.st_mode && 0010)
               {
               return true;
               }                 
            }
         }
      }
   }

return false;
}

/*******************************************************************/

int ShellCommandReturnsZero(char *comm,int useshell)

{ int status, i, argc = 0;
  pid_t pid;
  char arg[CF_MAXSHELLARGS][CF_BUFSIZE];
  char **argv;
  char esc_command[CF_BUFSIZE];

if (!useshell)
   {
   /* Build argument array */

   for (i = 0; i < CF_MAXSHELLARGS; i++)
      {
      memset (arg[i],0,CF_BUFSIZE);
      }

   argc = ArgSplitCommand(comm,arg);

   if (argc == -1)
      {
      CfOut(cf_error,"","Too many arguments in %s\n",comm);
      return false;
      }
   }

if ((pid = fork()) < 0)
   {
   FatalError("Failed to fork new process");
   }
else if (pid == 0)                     /* child */
   {
   ALARM_PID = -1;

   if (useshell)
      {
      strncpy(esc_command,WinEscapeCommand(comm),CF_BUFSIZE-1);

      if (execl("/bin/sh","sh","-c",esc_command,NULL) == -1)
         {
         CfOut(cf_error,"execl","Command %s failed",esc_command);
         exit(1);
         }
      }
   else
      {
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
         CfOut(cf_error,"execvp","Command %s failed",argv);
         exit(1);
         }

      free((char *)argv);
      }
   }
else                                    /* parent */
   {
   pid_t wait_result;

   while ((wait_result = wait(&status)) != pid)
      {
      if (wait_result <= 0)
         {
         CfOut(cf_inform,"wait","Wait for child failed\n");
         return false;
         }
      }

   if (WIFSIGNALED(status))
      {
      Debug("Script %s returned: %d\n",comm,WTERMSIG(status));
      return false;
      }
   
   if (! WIFEXITED(status))
      {
      return false;
      }
   
   if (WEXITSTATUS(status) == 0)
      {
      Debug("Shell command returned 0\n");
      return true;
      }
   else
      {
      Debug("Shell command was non-zero: %d\n",WEXITSTATUS(status));
      return false;
      }
   }

return false;
}

/********************************************************************/

int GetExecOutput(char *command,char *buffer,int useshell)

/* Buffer initially contains whole exec string */

{ int offset = 0;
  char line[CF_EXPANDSIZE], *sp; 
  FILE *pp;

Debug("GetExecOutput(%s,%s) - use shell = %d\n",command,buffer,useshell);
  
if (useshell)
   {
   pp = cf_popen_sh(command,"r");
   }
else
   {
   pp = cf_popen(command,"r");
   }

if (pp == NULL)
   {
   CfOut(cf_error,"cf_popen","Couldn't open pipe to command %s\n",command);
   return false;
   }

memset(buffer,0,CF_EXPANDSIZE);
  
while (!feof(pp))
   {
   if (ferror(pp))  /* abortable */
      {
      fflush(pp);
      break;
      }

   CfReadLine(line,CF_EXPANDSIZE,pp);

   if (ferror(pp))  /* abortable */
      {
      fflush(pp);
      break;
      }  
   
   for (sp = line; *sp != '\0'; sp++)
      {
      if (*sp == '\n')
         {
         *sp = ' ';
         }
      }
   
   if (strlen(line)+offset > CF_EXPANDSIZE-10)
      {
      CfOut(cf_error,"","Buffer exceeded %d bytes in exec %s\n",CF_EXPANDSIZE,command);
      break;
      }

   snprintf(buffer+offset,CF_EXPANDSIZE,"%s ",line);
   offset += strlen(line)+1;
   }

if (offset > 0)
   {
   Chop(buffer); 
   }

Debug("GetExecOutput got: [%s]\n",buffer);
 
cf_pclose(pp);
return true;
}

/**********************************************************************/

void ActAsDaemon(int preserve)

{ int fd, maxfd;

#ifdef HAVE_SETSID
setsid();
#endif

closelog();

fflush(NULL);
fd = open("/dev/null", O_RDWR, 0);

if (fd != -1)
   {
   if (dup2(fd,STDIN_FILENO) == -1)
      {
      CfOut(cf_error,"dup2","Could not dup");
      }

   if (dup2(fd,STDOUT_FILENO) == -1)
      {
      CfOut(cf_error,"dup2","Could not dup");
      }

   dup2(fd,STDERR_FILENO);

   if (fd > STDERR_FILENO)
      {
      close(fd);
      }
   }

chdir("/");
   
#ifdef HAVE_SYSCONF
maxfd = sysconf(_SC_OPEN_MAX);
#else
# ifdef _POXIX_OPEN_MAX
maxfd = _POSIX_OPEN_MAX;
# else
maxfd = 1024;
# endif
#endif

for (fd = STDERR_FILENO + 1; fd < maxfd; ++fd)
   {
   if (fd != preserve)
      {
      close(fd);
      }
   }
}

/**********************************************************************/

char *WinEscapeCommand(char *s)

{ static char buffer[CF_BUFSIZE];
  char *spf,*spto;

memset(buffer,0,CF_BUFSIZE);

spto = buffer;

for (spf = s; *spf != '\0'; spf++)
   {
   switch (*spf)
      {
      case '\\':
          *spto++ = '\\';
          *spto++ = '\\';
          break;

      default:
          *spto++ = *spf;
          break;          
      }
   }

return buffer;
}
