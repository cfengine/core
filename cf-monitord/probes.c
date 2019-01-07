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


#include <probes.h>


/* Structs */

typedef struct
{
    const char *name;
    ProbeInit init;
} Probe;

/* Constants */

static const Probe ENTERPRISE_PROBES[] =
{
    {"Input/output", &MonIoInit},
    {"Memory", &MonMemoryInit},
};

/* Globals */

static ProbeGatherData ENTERPRISE_PROBES_GATHERERS[sizeof(ENTERPRISE_PROBES) / sizeof(ENTERPRISE_PROBES[0])];


void MonOtherInit()
{
    int i;

    Log(LOG_LEVEL_VERBOSE, "Starting initialization of static Nova monitoring probes.");

    for (i = 0; i < sizeof(ENTERPRISE_PROBES) / sizeof(ENTERPRISE_PROBES[0]); ++i)
    {
        const Probe *probe = &ENTERPRISE_PROBES[i];
        const char *provider;
        const char *error;

        if ((ENTERPRISE_PROBES_GATHERERS[i] = (probe->init) (&provider, &error)))
        {
            Log(LOG_LEVEL_VERBOSE, " * %s: %s.", probe->name, provider);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, " * %s: Disabled: %s.", probe->name, error);
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Initialization of static Nova monitoring probes is finished.");
}

/****************************************************************************/

void MonOtherGatherData(double *cf_this)
{
    int i;

    Log(LOG_LEVEL_VERBOSE, "Gathering data from static Nova monitoring probes.");

    for (i = 0; i < sizeof(ENTERPRISE_PROBES) / sizeof(ENTERPRISE_PROBES[0]); ++i)
    {
        const char *probename = ENTERPRISE_PROBES[i].name;
        ProbeGatherData gatherer = ENTERPRISE_PROBES_GATHERERS[i];

        if (gatherer)
        {
            Log(LOG_LEVEL_VERBOSE, " * %s", probename);
            (*gatherer) (cf_this);
        }
    }
    Log(LOG_LEVEL_VERBOSE, "Gathering data from static Nova monitoring probes is finished.");
}
