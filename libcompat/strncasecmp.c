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

#if !HAVE_DECL_STRCASECMP
int strncasecmp(const char *s1, const char *s2);
#endif

int strncasecmp(const char *s1, const char *s2, size_t n)
{
    while (n-- > 0)
    {
        if (*s1 == '\0' && *s2 == '\0')
        {
            break;
        }
        if (*s1 == '\0')
        {
            return -1;
        }
        if (*s2 == '\0')
        {
            return 1;
        }
        if (tolower(*s1) < tolower(*s2))
        {
            return -1;
        }
        if (tolower(*s1) > tolower(*s2))
        {
            return 1;
        }
        s1++;
        s2++;
    }
    return 0;
}
