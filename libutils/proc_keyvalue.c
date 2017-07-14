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

#include <proc_keyvalue.h>

typedef struct
{
    void *orig_param;
    KeyNumericValueCallback orig_callback;
} KeyNumericParserInfo;

bool KeyNumericParserCallback(const char *field, const char *value, void *param)
{
    KeyNumericParserInfo *info = param;
    long long numeric_value;

    if (sscanf(value,
#if defined(__MINGW32__)
               "%I64d",
#else
               "%lli",
#endif
               &numeric_value) != 1)
    {
        /* Malformed file */
        return false;
    }

    return (*info->orig_callback) (field, numeric_value, info->orig_param);
}

bool ParseKeyNumericValue(FILE *fd, KeyNumericValueCallback callback, void *param)
{
    KeyNumericParserInfo info = { param, callback };

    return ParseKeyValue(fd, KeyNumericParserCallback, &info);
}

bool ParseKeyValue(FILE *fd, KeyValueCallback callback, void *param)
{
    char buf[1024];

    while (fgets(buf, sizeof(buf), fd))
    {
        char *s = strchr(buf, ':');

        if (!s)
        {
            /* Malformed file */
            return false;
        }

        *s = 0;

        if (!(*callback) (buf, s + 1, param))
        {
            return false;
        }
    }

    return !ferror(fd);
}
