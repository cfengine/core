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

#include <platform.h>

#include <stdint.h>
#include <ctype.h>
#include <alloc.h>
#include <cfversion.h>

struct Version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t extra;
    uint8_t build;
};

#define Char2Dec(o, c) \
    (o * 10) + c - '0'

Version *VersionNew()
{
    Version *version = NULL;
    version = xmalloc(sizeof(Version));
    version->major = 0;
    version->minor = 0;
    version->patch = 0;
    version->extra = 0;
    version->build = 0;
    return version;
}

Version *VersionNewFromCharP(const char *version, unsigned int size)
{
    if (!version)
    {
        return NULL;
    }
    /*
     * Parse the version string
     *
     *                          +-> Done
     *   '.'      '.'     '-'   |
     * 0 ---- > 1 ----> 2 ----> 3 <-+
     *                  | '.'       |
     *                  +-----> 4 --+
     */
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t patch = 0;
    uint16_t extra = 0;
    uint16_t build = 0;
    uint16_t value = 0;
    unsigned int i = 0;
    int state = 0;
    int error_condition = 0;

    for (i = 0; i < size; ++i)
    {
        int is_period = 0;
        int is_number = 0;
        int is_dash = 0;
        char current = 0;

        current = version[i];
        is_period = (current == '.') ? 1 : 0;
        is_number = isdigit(current);
        is_dash = (current == '-') ? 1 : 0;

        if (value > 255)
        {
            error_condition = 1;
            break;
        }
        switch (state)
        {
        case 0:
            if (is_period)
            {
                state = 1;
                major = (uint8_t)value;
                value = 0;
                break;
            }
            if (is_number)
            {
                value = Char2Dec(value, current);
            }
            else
            {
                error_condition = 1;
                i = size + 1;
                break;
            }
            break;
        case 1:
            if (is_period)
            {
                state = 2;
                minor = (uint8_t)value;
                value = 0;
                break;
            }
            if (is_number)
            {
                value = Char2Dec(value, current);
            }
            else
            {
                error_condition = 1;
                i = size + 1;
                break;
            }
            break;
        case 2:
            if (is_period)
            {
                state = 4;
                patch = (uint8_t)value;
                value = 0;
                break;
            }
            if (is_dash)
            {
                state = 3;
                patch = value;
                value = 0;
                break;
            }
            if (is_number)
            {
                value = Char2Dec(value, current);
            }
            else
            {
                error_condition = 1;
                i = size + 1;
                break;
            }
            break;
        case 3:
            if (is_number)
            {
                value = Char2Dec(value, current);
            }
            else
            {
                error_condition = 1;
                i = size + 1;
                break;
            }
            break;
        case 4:
            if (is_dash)
            {
                state = 3;
                extra = (uint8_t)value;
                value = 0;
                break;
            }
            if (is_number)
            {
                value = Char2Dec(value, current);
            }
            else
            {
                error_condition = 1;
                i = size + 1;
                break;
            }
            break;
        default:
            error_condition = 1;
            break;
        }
    }
    if (error_condition)
    {
        return NULL;
    }
    /* There are two valid exit states: 2 & 3, all other states indicate error */
    if ((state == 2) || (state == 3))
    {
        if (state == 2)
        {
            if (value > 255)
            {
                return NULL;
            }
            patch = (uint8_t)value;
        }
        else
        {
            if (value > 255)
            {
                return NULL;
            }
            build = (uint8_t)value;
        }
    }
    else
    {
        return NULL;
    }
    Version *v = VersionNew();
    v->major = major;
    v->minor = minor;
    v->patch = patch;
    v->extra = extra;
    v->build = build;
    return v;
}

Version *VersionNewFrom(Buffer *buffer)
{
    if (!buffer)
    {
        return NULL;
    }
    return VersionNewFromCharP(BufferData(buffer), BufferSize(buffer));
}

void VersionDestroy(Version **version)
{
    if (!version || !(*version))
    {
        return;
    }
    free (*version);
    *version = NULL;
}

int VersionCompare(Version *a, Version *b)
{
    int comparison = 0;
    if (a->major < b->major)
    {
        comparison = -10;
    }
    else if (a->major == b->major)
    {
        if (a->minor < b->minor)
        {
            comparison = -10;
        }
        else if (a->minor == b->minor)
        {
            if (a->patch < b->patch)
            {
                comparison = -10;
            }
            else if (a->patch == b->patch)
            {
                if (a->build < b->build)
                {
                    comparison = -10;
                }
                else if (a->build == b->build)
                {
                    comparison = 0;
                }
                else
                {
                    comparison = 10;
                }
            }
            else
            {
                comparison = 10;
            }
        }
        else
        {
            comparison = 10;
        }
    }
    else
    {
        comparison = 10;
    }
    return comparison;
}

int VersionMajor(Version *version)
{
    if (!version)
    {
        return -1;
    }
    return (int)version->major;
}

int VersionMinor(Version *version)
{
    if (!version)
    {
        return -1;
    }
    return (int)version->minor;
}

int VersionPatch(Version *version)
{
    if (!version)
    {
        return -1;
    }
    return (int)version->patch;
}

int VersionExtra(Version *version)
{
    if (!version)
    {
        return -1;
    }
    return (int)version->extra;
}

int VersionBuild(Version *version)
{
    if (!version)
    {
        return -1;
    }
    return (int)version->build;
}
