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

{ 
#ifdef MINGW
return NovaWin_IsExecutable(file);
#else
return Unix_IsExecutable(file);
#endif
}

/*******************************************************************/

int ShellCommandReturnsZero(char *comm,int useshell)

{
#ifdef MINGW
return NovaWin_ShellCommandReturnsZero(comm,useshell);
#else
return Unix_ShellCommandReturnsZero(comm,useshell);
#endif
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

CloseNetwork();
Cf3CloseLog();

fflush(NULL);
fd = open(NULLFILE, O_RDWR, 0);

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

/**********************************************************************/

int ArgSplitCommand(char *comm,char arg[CF_MAXSHELLARGS][CF_BUFSIZE])

{ char *sp;
  int i = 0;

for (sp = comm; sp < comm+strlen(comm); sp++)
   {
   if (i >= CF_MAXSHELLARGS-1)
      {
      CfOut(cf_error,"","Too many arguments in embedded script");
      FatalError("Use a wrapper");
      }
   
   while (*sp == ' ' || *sp == '\t')
      {
      sp++;
      }
   
   switch (*sp)
      {
      case '\0': return(i-1);
   
      case '\"': sscanf (++sp,"%[^\"]",arg[i]);
          break;
      case '\'': sscanf (++sp,"%[^\']",arg[i]);
          break;
      case '`':  sscanf (++sp,"%[^`]",arg[i]);
          break;
      default:   sscanf (sp,"%s",arg[i]);
          break;
      }
   
   sp += strlen(arg[i]);
   i++;
   }
 
 return (i);
}
