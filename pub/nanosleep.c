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
 * Emulating nanosleep(2) with select(3).
 *
 * NB! Does not calculate "remaining time"
 */

#ifdef HAVE_CONFIG_H
#include "../src/conf.h"
#endif

#include <time.h>

#if !HAVE_DECL_NANOSLEEP
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
#endif

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
struct timeval tv =
   {
   .tv_sec = rptq->tv_sec,
   .tv_usec = (rptq->tv_nsec + 1000 - 1) / 1000,
   };

if (rmtp)
   {
   rmtp->tv_sec = 0;
   rmtp->tv_nsec = 0;
   }

return select(0, NULL, NULL, &tv);
}
