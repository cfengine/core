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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>

/* Under MinGW putenv('var=') will remove variable from environment */

#ifdef __MINGW32__

int unsetenv(const char *name)
{
int retval;
char *buf;

if (name == NULL || *name == 0 || strchr(name, '=') != 0)
   {
   errno = EINVAL;
   return -1;
   }

buf = malloc(strlen(name) + 2);

if (!buf)
   {
   errno = ENOMEM;
   return -1;
   }

sprintf(buf, "%s=", name);
retval = putenv(buf);
free(buf);
return retval;
}

#endif

/* Under Solaris8/9 we need to manually update 'environ' variable */

#ifdef __sun

/*
 * Note: this function will leak memory as we don't know how to free data
 * previously used by environment variables.
 */

extern char **environ;

int unsetenv(const char *name)
{
char **c;
int len;

if (name == NULL || *name == 0 || strchr(name, '=') != 0)
   {
   errno = EINVAL;
   return -1;
   }

len = strlen(name);

/* Find variable */
for (c = environ; *c; ++c)
   {
   if (strncmp(name, *c, len) == 0 && ((*c)[len] == '=' || (*c)[len] == 0))
      {
      break;
      }
   }

/* Shift remaining values */
for(; *c; ++c)
   {
   *c = *(c+1);
   }

return 0;
}

#endif
