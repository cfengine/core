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

/*********************************************************/
/* patches.c                                             */
/*                                                       */
/* Contains any fixes which need to be made because of   */
/* lack of OS support on a given platform                */
/* These are conditionally compiled, pending extensions  */
/* or developments in the OS concerned.                  */
/*********************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************/

int IntMin (int a,int b)

{
if (a > b)
   {
   return b;
   }
else     
   {
   return a;
   }
}

/*********************************************************/

/* We assume that s is at least MAX_FILENAME large.
 * MapName() is thread-safe, but the argument is modified. */

char *MapName(char *s)

{ char buffer[CF_BUFSIZE];
  char *spf,*spto;

#ifdef NT
memset(buffer,0,CF_BUFSIZE);

if (UseUnixStandard(s))
   {
   spto = buffer;

   for (spf = s; *spf != '\0'; spf++)
      {
      if (IsFileSep(*spf) && IsFileSep(*(spf+1))) /* compress // or \\ */
         {
         continue;
         }

      if (IsFileSep(*spf) && *(spf+1) != '\0' && *(spf+2) == ':') /* compress \c:\abc */
         {
         continue;
         }

      if (*(spf+1) != '\0' && (strncmp(spf+1,":\\",2) == 0 || strncmp(spf+1,":/",2) == 0 ))
         {
         /* For cygwin translation */
         strcat(spto,"/cygdrive/");
         /* Drive letter */
         strncat(spto,spf,1); 
         strcat(spto,"/");
         spto += strlen("/cygdrive/c/");
         spf += strlen("c:/") - 1;
         continue;
         }

      switch (*spf)
         {
         case '\\':
             *spto = '/';
             break;

         default:
             *spto = *spf;
             break;          
         }
      
      spto++;
      }
   }
else
   {
   spto = buffer;

   for (spf = s; *spf != '\0'; spf++)
      {
      switch (*spf)
         {
         case '/':
             *spto++ = '\\';
             break;

         default:
             *spto++ = *spf;
             break;          
         }
      }
   }

memset(s,0,MAX_FILENAME);
strncpy(s,buffer,MAX_FILENAME-1);

#endif  /* NT */

return s;
}

/*********************************************************/

char *MapNameForward(char *s)
/* Like MapName(), but maps all slashes to forward */

{ char *sp;
 
for (sp = s; *sp != '\0'; sp++)
   {
   switch(*sp)
      {
      case '\\':
	  *sp = '/';
      }
   }

return s;
}

/*********************************************************/

int UseUnixStandard(char *s)

{
#ifdef MINGW
return false;
#else
return true;
#endif
}

/*********************************************************/

char *StrStr(char *a,char *b) /* Case insensitive match */

{ char buf1[CF_BUFSIZE],buf2[CF_BUFSIZE];

strncpy(buf1,ToLowerStr(a),CF_BUFSIZE-1);
strncpy(buf2,ToLowerStr(b),CF_BUFSIZE-1);
return strstr(buf1,buf2); 
}

/*********************************************************/

int StrnCmp(char *a,char *b,size_t n) /* Case insensitive match */

{ char buf1[CF_BUFSIZE],buf2[CF_BUFSIZE];

strncpy(buf1,ToLowerStr(a),CF_BUFSIZE-1);
strncpy(buf2,ToLowerStr(b),CF_BUFSIZE-1);
return strncmp(buf1,buf2,n); 
}

/*********************************************************************/

int cf_strcmp(char *s1,char *s2)

{
/* Windows native eventually? */
return strcmp(s1,s2);
}

/*********************************************************************/

int cf_strncmp(char *s1,char *s2,size_t n)

{
/* Windows native eventually? */
return strncmp(s1,s2,n);
}

/*********************************************************************/

char *cf_strcpy(char *s1,char *s2)

{
/* Windows native eventually? */
return strcpy(s1,s2);
}

/*********************************************************************/

char *cf_strncpy(char *s1,char *s2,size_t n)

{
/* Windows native eventually? */
return strncpy(s1,s2,n);
}

/*********************************************************************/

char *cf_strdup(char *s)

{
return strdup(s);
}

/*********************************************************************/

int cf_strlen(char *s)
    
{
return strlen(s);
}

/*********************************************************************/

char *cf_strchr(char *s, int c)
    
{
return strchr(s,c);
}

/*********************************************************/

#ifndef HAVE_GETNETGRENT

#if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

# ifndef const
#  define const
# endif
#endif

/*********************************************************/

int setnetgrent(netgroup)

const char *netgroup;

{
return 0;
}

/**********************************************************/

int getnetgrent(a,b,c)

char **a, **b, **c;

{
*a=NULL;
*b=NULL;
*c=NULL;
return 0;
}

/***********************************************************/

void endnetgrent()

{
}

#endif

#ifndef HAVE_UNAME

#if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

# ifndef const
#  define const
# endif
#endif


/***********************************************************/
/* UNAME is missing on some weird OSes                     */
/***********************************************************/

int uname (struct utsname *sys)

#ifdef MINGW

{
return NovaWin_uname(sys);
}

#else  /* NOT MINGW */

{ char buffer[CF_BUFSIZE], *sp;

if (gethostname(buffer,CF_BUFSIZE) == -1)
   {
   perror("gethostname");
   exit(1);
   }

strcpy(sys->nodename,buffer);

if (strcmp(buffer,AUTOCONF_HOSTNAME) != 0)
   {
   CfOut(cf_verbose,"","This binary was complied on a different host (%s).\n",AUTOCONF_HOSTNAME);
   CfOut(cf_verbose,"","This host does not have uname, so I can't tell if it is the exact same OS\n");
   }

strcpy(sys->sysname,AUTOCONF_SYSNAME);
strcpy(sys->release,"cfengine-had-to-guess");
strcpy(sys->machine,"missing-uname(2)");
strcpy(sys->version,"unknown");


  /* Extract a version number if possible */

for (sp = sys->sysname; *sp != '\0'; sp++)
   {
   if (isdigit(*sp))
      {
      strcpy(sys->release,sp);
      strcpy(sys->version,sp);
      *sp = '\0';
      break;
      }
   }

return (0);
}

#endif  /* NOT MINGW */

#endif  /* NOT HAVE_UNAME */

/***********************************************************/
/* strstr() missing on old BSD systems                     */
/***********************************************************/

#ifndef HAVE_STRSTR

#if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

# ifndef const
#  define const
# endif
#endif


char *strstr(char *s1,char *s2)

{ char *sp;

for (sp = s1; *sp != '\0'; sp++)
   {
   if (*sp != *s2)
      {
      continue;
      }

   if (strncmp(sp,s2,strlen(s2))== 0)
      {
      return sp;
      }
   }

return NULL;
}

#endif

/***********************************************************/
/* strdup() missing on old BSD systems                     */
/***********************************************************/

#ifndef HAVE_STRDUP

char *strdup(char *str)

{ char *sp;
 
if (str == NULL)
   {
   return NULL;
   }

if ((sp = malloc(strlen(str)+1)) == NULL)
   {
   perror("malloc");
   return NULL;
   }

strcpy(sp,str);
return sp; 
}

#endif

#ifndef HAVE_STRSEP

char *strsep(char **stringp, const char *delim)

{
return strtok(*stringp,delim);
}

#endif


/***********************************************************/
/* strrchr() missing on old BSD systems                     */
/***********************************************************/

#ifndef HAVE_STRRCHR

char *strrchr(char *str,char ch)

{ char *sp;
 
if (str == NULL)
   {
   return NULL;
   }

if (strlen(str) == 0)
   {
   return NULL;
   }

for (sp = str+strlen(str)-1; sp > str; sp--)
   {
   if (*sp == ch)
      {
      return *sp;
      }
   }

return NULL; 
}

#endif


/***********************************************************/
/* strerror() missing on systems                           */
/***********************************************************/

#ifndef HAVE_STRERROR

char *strerror(int err)

{ static char buffer[20];

sprintf(buffer,"Error number %d\n",err);
return buffer; 
}

#endif

/***********************************************************/
/* putenv() missing on old BSD systems                     */
/***********************************************************/

#ifndef HAVE_PUTENV

#if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

# ifndef const
#  define const
# endif
#endif


int putenv(char *s)

{
CfOut(cf_verbose,"","(This system does not have putenv: cannot update CFALLCLASSES\n");
return 0;
}


#endif


/***********************************************************/
/* seteuid/gid() missing on some on posix systems          */
/***********************************************************/

#ifndef HAVE_SETEUID

#if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

# ifndef const
#  define const
# endif
#endif


int seteuid (uid_t uid)

{
#ifdef HAVE_SETREUID
return setreuid(-1,uid);
#else
CfOut(cf_verbose,"","(This system does not have setreuid (patches.c)\n");
return -1; 
#endif 
}

#endif

/***********************************************************/

#ifndef HAVE_SETEGID

#if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

# ifndef const
#  define const
# endif
#endif


int setegid (gid_t gid)

{
#ifdef HAVE_SETREGID
return setregid(-1,gid);
#else
CfOut(cf_verbose,"","(This system does not have setregid (patches.c)\n");
return -1; 
#endif 
}

#endif

/*******************************************************************/

#ifndef HAVE_DRAND48

double drand48(void)

{
return (double)random();
}

#endif

#ifndef HAVE_DRAND48  

void srand48(long seed)

{
srandom((unsigned int)seed);
}

#endif


/*******************************************************************/

int IsPrivileged()

{
#ifdef NT
return true;
#else
return (getuid() == 0);
#endif 
}

/*******************************************************************/

#ifndef HAVE_LIBRT

int clock_gettime(clockid_t clock_id,struct timespec *tp)

{ static time_t now;

now = time(NULL);

tp->tv_sec = (time_t)now;
tp->tv_nsec = 0;
return 0;
}

#endif

/*******************************************************************/

char *cf_ctime(const time_t *timep)

/* NT uses format "Wed Jan 02 02:03:55 1980", but should use
 * "Wed Jan  2 02:03:55 1980" (no 0-padding for days)        */

{
char *times = ctime(timep);

if (times == NULL)
  {
  CfOut(cf_error, "ctime", "!! Could not convert time to string");
  return NULL;
  }


#ifdef MINGW

if (times[8] == '0')
  {
  times[8] = ' ';
  }

#endif  /* MINGW */

return times;
}

/*******************************************************************/

int cf_closesocket(int sd)

{
#ifdef MINGW
return closesocket(sd);
#else
return close(sd);
#endif
}

/*******************************************************************/

int cf_mkdir(const char *path, mode_t mode)

{
#ifdef MINGW
return NovaWin_mkdir(path, mode);
#else
return mkdir(path,mode);
#endif
}

/*******************************************************************/

int cf_chmod(const char *path, mode_t mode)

{
#ifdef MINGW
return NovaWin_chmod(path, mode);
#else
return chmod(path,mode);
#endif
}

/*******************************************************************/

int cf_rename(const char *oldpath, const char *newpath)

{
#ifdef MINGW
return NovaWin_rename(oldpath, newpath);
#else
return rename(oldpath,newpath);
#endif
}

/*******************************************************************/

void *cf_malloc(size_t size, char *errLocation)
/* Stops on memory allocation error */
{
 char buf[CF_SMALLBUF];
 void *ptr = NULL;
 
 ptr = malloc(size);

 if(ptr == NULL)
    {
    CfOut(cf_error, "malloc", "!! Could not allocate memory in \"%s\"", errLocation);
    FatalError("Memory allocation\n");
    }
 
 return ptr;
}

/*******************************************************************/

void OpenNetwork()

{
#ifdef MINGW
NovaWin_OpenNetwork();
#else
/* no network init on Unix */
#endif
}

/*******************************************************************/

void CloseNetwork()

{
#ifdef MINGW
NovaWin_CloseNetwork();
#else
/* no network close on Unix */
#endif
}

/*******************************************************************/

void CloseWmi()

{
#ifdef MINGW
NovaWin_WmiDeInitialize();
#else
/* no WMI on Unix */
#endif
}

/*******************************************************************/

#ifdef MINGW  // FIXME: Timeouts ignored on windows for now...
unsigned int alarm(unsigned int seconds)

{
return 0;
}
#endif  /* MINGW */
