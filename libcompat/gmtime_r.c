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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <time.h>
#include <string.h>

#if !HAVE_DECL_GMTIME_R
struct tm *gmtime_r(const time_t *timep, struct tm *result);
#endif

#if defined(__MINGW32__)

/* On Windows, gmtime buffer is thread-specific, so using it in place of the
   reentrant version is safe */

struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
    struct tm *ret = gmtime(timep);
    if (ret)
    {
        memcpy(result, ret, sizeof(struct tm));
    }
    return ret;
}

#endif
