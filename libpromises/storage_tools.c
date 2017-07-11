/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <cf3.defs.h>

#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_VFS_H
# include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif

/************************************************************************/

#ifndef __MINGW32__

off_t GetDiskUsage(char *file, CfSize type)
{
# if defined __sun || defined sco || defined __OpenBSD__ || (defined(__NetBSD__) && __NetBSD_Version__ >= 200040000)
    struct statvfs buf;
# else
    struct statfs buf;
# endif
    int64_t used = 0, avail = 0;
    int capacity = 0;

    memset(&buf, 0, sizeof(buf));

# if defined __sun || defined sco || defined __OpenBSD__ || (defined(__NetBSD__) && __NetBSD_Version__ >= 200040000)
    if (statvfs(file, &buf) != 0)
    {
        Log(LOG_LEVEL_ERR, "statvfs", "Couldn't get filesystem info for %s", file);
        return CF_INFINITY;
    }
# elif defined __SCO_DS || defined _CRAY || (defined(__NetBSD__) && __NetBSD_Version__ >= 200040000)
    if (statfs(file, &buf, sizeof(struct statfs), 0) != 0)
    {
        Log(LOG_LEVEL_ERR, "statfs", "Couldn't get filesystem info for %s", file);
        return CF_INFINITY;
    }
# else
    if (statfs(file, &buf) != 0)
    {
        Log(LOG_LEVEL_ERR, "Couldn't get filesystem info for '%s'. (statfs: %s)", file, GetErrorStr());
        return CF_INFINITY;
    }
# endif

# if defined __sun
    used = (buf.f_blocks - buf.f_bfree) * (int64_t)buf.f_frsize;
    avail = buf.f_bavail * (int64_t)buf.f_frsize;
# endif

# if defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__ || defined __hpux || defined __APPLE__
    used = (buf.f_blocks - buf.f_bfree) * (int64_t)buf.f_bsize;
    avail = buf.f_bavail * (int64_t)buf.f_bsize;
# endif

# if defined _AIX || defined __SCO_DS || defined _CRAY
    used = (buf.f_blocks - buf.f_bfree) * (int64_t)buf.f_bsize;
    avail = buf.f_bfree * (int64_t)buf.f_bsize;
# endif

# if defined __linux__
    used = (buf.f_blocks - buf.f_bfree) * (int64_t)buf.f_bsize;
    avail = buf.f_bavail * (int64_t)buf.f_bsize;
# endif

    capacity = (double) (avail) / (double) (avail + used) * 100;

    Log(LOG_LEVEL_DEBUG, "GetDiskUsage(%s) = %jd/%jd", file, (intmax_t) avail, (intmax_t) capacity);

    if (type == CF_SIZE_ABS)
    {
        // TODO: This should be handled better by actually returning a bigger
        // data type, but for now just protect against overflow by hitting the
        // ceiling.
        if (sizeof(off_t) < sizeof(int64_t) && avail > 0x7fffffffLL)
        {
            return 0x7fffffff;
        }
        else
        {
            return avail;
        }
    }
    else
    {
        return capacity;
    }
}

#endif /* __MINGW32__ */
