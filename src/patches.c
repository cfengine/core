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

static int IntMin (int a,int b);
static int UseUnixStandard(char *s);
static char *cf_format_strtimestamp(struct tm *tm, char *buf);


/*********************************************************/

static int IntMin (int a,int b)

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

#ifdef NT
char *MapName(char *s)
{
char buffer[CF_BUFSIZE];
char *spto;
char *spf;

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

return s;
}
#else
char *MapName(char *s)
{
return s;
}

#endif  /* NT */

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

static int UseUnixStandard(char *s)

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

int cf_strcmp(const char *s1, const char *s2)

{
/* Windows native eventually? */
return strcmp(s1,s2);
}

/*********************************************************************/

int cf_strncmp(const char *s1, const char *s2, size_t n)

{
/* Windows native eventually? */
return strncmp(s1,s2,n);
}

/*********************************************************************/

char *cf_strcpy(char *s1, const char *s2)

{
/* Windows native eventually? */
return strcpy(s1,s2);
}

/*********************************************************************/

char *cf_strncpy(char *s1, const char *s2, size_t n)

{
/* Windows native eventually? */
return strncpy(s1,s2,n);
}

/*********************************************************************/

char *cf_strdup(const char *s)

{
return strdup(s);
}

/*********************************************************************/

int cf_strlen(const char *s)
    
{
return strlen(s);
}

/*********************************************************************/

char *cf_strchr(const char *s, int c)
    
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

char *cf_ctime(const time_t *timep)
{
static char buf[26];
return cf_strtimestamp_local(*timep, buf);
}

/*
 * This function converts passed time_t value to string timestamp used
 * throughout the system. By sheer coincidence this timestamp has the same
 * format as ctime(3) output on most systems (but NT differs in definition of
 * ctime format, so those are not identical there).
 *
 * Buffer passed should be at least 26 bytes long (including the trailing zero).
 *
 * Please use this function instead of (non-portable and deprecated) ctime_r or
 * (non-threadsafe) cf_ctime or ctime.
 */

/*******************************************************************/

char *cf_strtimestamp_local(const time_t time, char *buf)
{
struct tm tm;

if (localtime_r(&time, &tm) == NULL)
   {
   CfOut(cf_verbose, "localtime_r", "Unable to parse passed timestamp");
   return NULL;
   }

return cf_format_strtimestamp(&tm, buf);
}

/*******************************************************************/

char *cf_strtimestamp_utc(const time_t time, char *buf)
{
struct tm tm;

if (gmtime_r(&time, &tm) == NULL)
   {
   CfOut(cf_verbose, "gmtime_r", "Unable to parse passed timestamp");
   return NULL;
   }

return cf_format_strtimestamp(&tm, buf);
}

/*******************************************************************/

static char *cf_format_strtimestamp(struct tm *tm, char *buf)
{
 /* Security checks */
if (tm->tm_year < -2899 || tm->tm_year > 8099)
   {
   CfOut(cf_error, "", "Unable to format timestamp: passed year is out of range: %d",
         tm->tm_year + 1900);
   return NULL;
   }

/* There is no easy way to replicate ctime output by using strftime */

if (snprintf(buf, 26, "%3.3s %3.3s %2d %02d:%02d:%02d %04d",
             DAY_TEXT[tm->tm_wday ? (tm->tm_wday - 1) : 6], MONTH_TEXT[tm->tm_mon],
             tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900) >= 26)
   {
   CfOut(cf_error, "", "Unable to format timestamp: passed values are out of range");
   return NULL;
   }

return buf;
}

/*******************************************************************/

int cf_closesocket(int sd)

{
int res;

#ifdef MINGW
res = closesocket(sd);
#else
res = close(sd);
#endif

if (res != 0)
  {
  CfOut(cf_error,"cf_closesocket","!! Could not close socket");
  }

return res;
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

/*******************************************************************/

#ifdef MINGW
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
  if (af == AF_INET)
    {
      struct sockaddr_in in;
      memset(&in, 0, sizeof(in));
      in.sin_family = AF_INET;
      memcpy(&in.sin_addr, src, sizeof(struct in_addr));
      getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
      return dst;
    }
  else if (af == AF_INET6)
    {
      struct sockaddr_in6 in;
      memset(&in, 0, sizeof(in));
      in.sin6_family = AF_INET6;
      memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
      getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
      return dst;
    }
  return NULL;
}


int inet_pton(int af, const char *src, void *dst)
{
  struct addrinfo hints, *res, *ressave;
  
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = af;
  
  if (getaddrinfo(src, NULL, &hints, &res) != 0)
    {
      CfOut(cf_error,"getaddrinfo", "!! Could not resolve host \"%s\"", src);
      return -1;
    }
  
  ressave = res;
  
  while (res)
    {
      memcpy(dst, res->ai_addr, res->ai_addrlen);
      res = res->ai_next;
    }
  
  freeaddrinfo(ressave);
  return 0;
}
#endif  /* MINGW */

/*******************************************************************/

int LinkOrCopy(const char *from, const char *to, int sym)
/**
 *  Creates symlink to file on platforms supporting it, copies on
 *  others.
 **/
{

#ifdef MINGW  // only copy on Windows for now


if (!CopyFile(from,to,TRUE))
  {
  return false;
  }


#else  /* NOT MINGW */


if(sym)
  {
    if (symlink(from,to) == -1)
      {
	return false;
      }
  }
 else  // hardlink
   {
     if(link(from,to) == -1)
       {
	 return false;
       }
   }

   
#endif  /* NOT MINGW */


 return true;
}
