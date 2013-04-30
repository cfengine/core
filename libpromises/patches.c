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

/*
  Contains any fixes which need to be made because of lack of OS support on a
  given platform These are conditionally compiled, pending extensions or
  developments in the OS concerned.

  FIXME: move to the libcompat/ directory or to the apropriate source file.
*/

#include "cf3.defs.h"

#include "logging_old.h"
#include "audit.h"

static char *cf_format_strtimestamp(struct tm *tm, char *buf);

/*********************************************************/

/* We assume that s is at least MAX_FILENAME large.
 * MapName() is thread-safe, but the argument is modified. */

#ifdef _WIN32
# if defined(__MINGW32__)

char *MapNameCopy(const char *s)
{
    char *str = xstrdup(s);

    char *c = str;
    while ((c = strchr(c, '/')))
    {
        *c = '\\';
    }

    return str;
}

char *MapName(char *s)
{
    char *c = s;

    while ((c = strchr(c, '/')))
    {
        *c = '\\';
    }
    return s;
}

# elif defined(__CYGWIN__)

char *MapNameCopy(const char *s)
{
    Writer *w = StringWriter();

    /* c:\a\b -> /cygdrive/c\a\b */
    if (s[0] && isalpha(s[0]) && s[1] == ':')
    {
        WriterWriteF(w, "/cygdrive/%c", s[0]);
        s += 2;
    }

    for (; *s; s++)
    {
        /* a//b//c -> a/b/c */
        /* a\\b\\c -> a\b\c */
        if (IsFileSep(*s) && IsFileSep(*(s + 1)))
        {
            continue;
        }

        /* a\b\c -> a/b/c */
        WriterWriteChar(w, *s == '\\' ? '/' : *s);
    }

    return StringWriterClose(w);
}

char *MapName(char *s)
{
    char *ret = MapNameCopy(s);

    if (strlcpy(s, ret, MAX_FILENAME) >= MAX_FILENAME)
    {
        FatalError(ctx, "Expanded path (%s) is longer than MAX_FILENAME ("
                   TOSTRING(MAX_FILENAME) ") characters",
                   ret);
    }
    free(ret);

    return s;
}

# else/* !__MINGW32__ && !__CYGWIN__ */
#  error Unknown NT-based compilation environment
# endif/* __MINGW32__ || __CYGWIN__ */
#else /* !_WIN32 */

char *MapName(char *s)
{
    return s;
}

char *MapNameCopy(const char *s)
{
    return xstrdup(s);
}

#endif /* !_WIN32 */

/*********************************************************/

char *MapNameForward(char *s)
/* Like MapName(), but maps all slashes to forward */
{
    while ((s = strchr(s, '\\')))
    {
        *s = '/';
    }
    return s;
}

/*********************************************************/

#ifndef HAVE_SETNETGRENT

int setnetgrent(const char *netgroup)
{
    return 0;
}

#endif

/**********************************************************/

#ifndef HAVE_GETNETGRENT

int getnetgrent(char **machinep, char **userp, char **domainp)
{
    *machinep = NULL;
    *userp = NULL;
    *domainp = NULL;
    return 0;
}

#endif

/***********************************************************/

#ifndef HAVE_ENDNETGRENT

int endnetgrent(void)
{
    return 1;
}

#endif

#ifndef HAVE_SETEUID

# if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */

#  ifndef const
#   define const
#  endif
# endif

int seteuid(uid_t uid)
{
# ifdef HAVE_SETREUID
    return setreuid(-1, uid);
# else
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "(This system does not have setreuid (patches.c)\n");
    return -1;
# endif
}

#endif

/***********************************************************/

#ifndef HAVE_SETEGID

int setegid(gid_t gid)
{
# ifdef HAVE_SETREGID
    return setregid(-1, gid);
# else
    CfOut(OUTPUT_LEVEL_VERBOSE, "", "(This system does not have setregid (patches.c)\n");
    return -1;
# endif
}

#endif

/*******************************************************************/

int IsPrivileged()
{
#ifdef _WIN32
    return true;
#else
    return (getuid() == 0);
#endif
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
 * (non-threadsafe) ctime.
 */

/*******************************************************************/

char *cf_strtimestamp_local(const time_t time, char *buf)
{
    struct tm tm;

    if (localtime_r(&time, &tm) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "localtime_r", "Unable to parse passed timestamp");
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "gmtime_r", "Unable to parse passed timestamp");
        return NULL;
    }

    return cf_format_strtimestamp(&tm, buf);
}

/*******************************************************************/

static char *cf_format_strtimestamp(struct tm *tm, char *buf)
{
    /* Security checks */
    if ((tm->tm_year < -2899) || (tm->tm_year > 8099))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to format timestamp: passed year is out of range: %d", tm->tm_year + 1900);
        return NULL;
    }

/* There is no easy way to replicate ctime output by using strftime */

    if (snprintf(buf, 26, "%3.3s %3.3s %2d %02d:%02d:%02d %04d",
                 DAY_TEXT[tm->tm_wday ? (tm->tm_wday - 1) : 6], MONTH_TEXT[tm->tm_mon],
                 tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900) >= 26)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to format timestamp: passed values are out of range");
        return NULL;
    }

    return buf;
}

/*******************************************************************/

int cf_closesocket(int sd)
{
    int res;

#ifdef __MINGW32__
    res = closesocket(sd);
#else
    res = close(sd);
#endif

    if (res != 0)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "cf_closesocket", "!! Could not close socket");
    }

    return res;
}

int LinkOrCopy(const char *from, const char *to, int sym)
/**
 *  Creates symlink to file on platforms supporting it, copies on
 *  others.
 **/
{

#ifdef __MINGW32__                    // only copy on Windows for now

    if (!CopyFile(from, to, TRUE))
    {
        return false;
    }

#else /* !__MINGW32__ */

    if (sym)
    {
        if (symlink(from, to) == -1)
        {
            return false;
        }
    }
    else                        // hardlink
    {
        if (link(from, to) == -1)
        {
            return false;
        }
    }

#endif /* !__MINGW32__ */

    return true;
}

#if !defined(__MINGW32__)

int ExclusiveLockFile(int fd)
{
    struct flock lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
    };

    while (fcntl(fd, F_SETLKW, &lock) == -1)
    {
        if (errno != EINTR)
        {
            return -1;
        }
    }

    return 0;
}

int ExclusiveUnlockFile(int fd)
{
    return close(fd);
}

#endif
