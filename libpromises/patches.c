/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/*
  Contains any fixes which need to be made because of lack of OS support on a
  given platform These are conditionally compiled, pending extensions or
  developments in the OS concerned.

  FIXME: move to the libcompat/ directory or to the appropriate source file.
*/

#include <cf3.defs.h>

#include <audit.h>

static char *cf_format_strtimestamp(struct tm *tm, char *buf);

/*********************************************************/

#ifndef HAVE_SETNETGRENT
#if SETNETGRENT_RETURNS_INT
int
#else
void
#endif
setnetgrent(const char *netgroup)
{
#if SETNETGRENT_RETURNS_INT
    return 0;
#endif
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
#if ENDNETGRENT_RETURNS_INT
int
#else
void
#endif
endnetgrent(void)
{
#if ENDNETGRENT_RETURNS_INT
    return 1;
#endif
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
    Log(LOG_LEVEL_VERBOSE, "(This system does not have setreuid (patches.c)");
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
    Log(LOG_LEVEL_VERBOSE, "(This system does not have setregid (patches.c)");
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
        Log(LOG_LEVEL_VERBOSE, "Unable to parse passed timestamp. (localtime_r: %s)", GetErrorStr());
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
        Log(LOG_LEVEL_VERBOSE, "Unable to parse passed timestamp. (gmtime_r: %s)", GetErrorStr());
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
        Log(LOG_LEVEL_ERR, "Unable to format timestamp: passed year is out of range: %d", tm->tm_year + 1900);
        return NULL;
    }

/* There is no easy way to replicate ctime output by using strftime */

    if (snprintf(buf, 26, "%3.3s %3.3s %2d %02d:%02d:%02d %04d",
                 DAY_TEXT[tm->tm_wday ? (tm->tm_wday - 1) : 6], MONTH_TEXT[tm->tm_mon],
                 tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900) >= 26)
    {
        Log(LOG_LEVEL_ERR, "Unable to format timestamp: passed values are out of range");
        return NULL;
    }

    return buf;
}

/*******************************************************************/

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
