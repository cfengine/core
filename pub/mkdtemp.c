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

#ifdef HAVE_CONFIG_H
# include "../src/conf.h"
#endif

#if !HAVE_DECL_MKDTEMP
char *mkdtemp(char *template);
#endif

#if !HAVE_DECL_STRRSTR
char *strrstr(const char *haystack, const char *needle);
#endif

#define MAXTRY 999999

char *mkdtemp(char *template)
{
    char *xxx = strrstr(template, "XXXXXX");

    if (xxx == NULL || strcmp(xxx, "XXXXXX") != 0)
    {
        errno = EINVAL;
        return NULL;
    }

    for (int i = 0; i <= MAXTRY; ++i)
    {
        snprintf(xxx, 7, "%06d", i);

        int fd = mkdir(template, S_IRUSR | S_IWUSR | S_IXUSR);
        if (fd >= 0)
        {
            close(fd);
            return template;
        }

        if (errno != EEXIST)
        {
            return NULL;
        }
    }

    errno = EEXIST;
    return NULL;
}
