/*
   Copyright (C) CFEngine AS

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <math.h>

#if !HAVE_DECL_ROUND
double round(double x);
#endif

double round(double x)
{
    if (x >= 0.0)
    {
        double y = floor(x);

        if (x - y >= 0.5)
        {
            return y + 1.0;
        }
        else
        {
            return y;
        }
    }
    else
    {
        double y = ceil(x);

        if (y - x >= 0.5)
        {
            return y - 1.0;
        }
        else
        {
            return y;
        }
    }
}
