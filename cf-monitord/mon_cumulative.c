/*
   Copyright 2019 Northern.tech AS

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

#include <cf3.defs.h>


/*
 * Cumulative statistics support and conversion to instant values
 */

typedef struct PrevValue_ PrevValue;

struct PrevValue_
{
    char *name;
    char *subname;
    union
    {
        unsigned u32;
        unsigned long long u64;
    } value;
    time_t timestamp;
    PrevValue *next;
};

/* Globals */

static PrevValue *values;

/* Implementation */

static PrevValue *AppendNewValue(const char *name, const char *subname, time_t timestamp)
{
    PrevValue *v = xmalloc(sizeof(PrevValue));

    v->name = xstrdup(name);
    v->subname = xstrdup(subname);
    v->timestamp = timestamp;
    v->next = values;
    values = v;
    return v;
}

unsigned GetInstantUint32Value(const char *name, const char *subname, unsigned value, time_t timestamp)
{
    PrevValue *v;

    for (v = values; v; v = v->next)
    {
        if (!strcmp(v->name, name) && !strcmp(v->subname, subname))
        {
            unsigned diff;
            unsigned difft;

            /* Check for wraparound */
            if (value < v->value.u32)
            {
                diff = INT_MAX - v->value.u32 + value;
            }
            else
            {
                diff = value - v->value.u32;
            }
            difft = timestamp - v->timestamp;

            v->value.u32 = value;
            v->timestamp = timestamp;

            if (difft != 0)
            {
                return diff / difft;
            }
            else
            {
                return (unsigned) -1;
            }
        }
    }

    AppendNewValue(name, subname, timestamp)->value.u32 = value;
    return (unsigned) -1;
}

unsigned long long GetInstantUint64Value(const char *name, const char *subname, unsigned long long value,
                                         time_t timestamp)
{
    PrevValue *v;

    for (v = values; v; v = v->next)
    {
        if (!strcmp(v->name, name) && !strcmp(v->subname, subname))
        {
            unsigned long long diff = value - v->value.u64;
            unsigned difft = timestamp - v->timestamp;

            v->value.u64 = value;
            v->timestamp = timestamp;

            if (difft != 0)
            {
                return diff / difft;
            }
            else
            {
                return (unsigned long long) -1;
            }
        }
    }

    AppendNewValue(name, subname, timestamp)->value.u64 = value;
    return (unsigned long long) -1;
}
