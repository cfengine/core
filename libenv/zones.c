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

#include <zones.h>
#include <string_lib.h>

#ifdef HAVE_ZONE_H
# include <zone.h>
#endif

#ifndef __MINGW32__

bool IsGlobalZone()
{
#ifdef HAVE_GETZONEID
    zoneid_t zid;
    char zone[ZONENAME_MAX];

    zid = getzoneid();
    getzonenamebyid(zid, zone, ZONENAME_MAX);

    if (strcmp(zone, "global") == 0)
    {
        return true;
    }
#endif

    return false;
}

bool ForeignZone(char *s)
{
// We want to keep the banner

    if (strstr(s, "PID"))
    {
        return false;
    }

# ifdef HAVE_GETZONEID
    zoneid_t zid;
    char *sp, zone[ZONENAME_MAX];

    zid = getzoneid();
    getzonenamebyid(zid, zone, ZONENAME_MAX);

    if (strcmp(zone, "global") == 0)
    {
        if (StringStartsWith(s, "global") && isspace(s[6]))
        {

            for (sp = s + strlen(s) - 1; isspace(*sp); sp--)
            {
                *sp = '\0';
            }

            return false;
        }
        else
        {
            return true;
        }
    }
# endif
    return false;
}

#ifdef HAVE_GETZONEID
#define ZONE_ONLY
#else
#define ZONE_ONLY ARG_UNUSED
#endif
int CurrentZoneName(ZONE_ONLY const char *s)
{
# ifdef HAVE_GETZONEID
    zoneid_t zid = getzoneid();

    if (zid >= 0)
    {
        return getzonenamebyid(zid, s, ZONENAME_MAX);
    }
# endif
    return -1;
}
#endif // !__MINGW32__
