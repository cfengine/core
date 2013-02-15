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

#include "cf3.defs.h"
#include "mod_interfaces.h"

static const BodySyntax CF_TCPIP_BODY[] =
{
    {"ipv4_address", DATA_TYPE_STRING, "[0-9.]+/[0-4]+", "IPv4 address for the interface"},
    {"ipv4_netmask", DATA_TYPE_STRING, "[0-9.]+/[0-4]+", "Netmask for the interface"},
    {"ipv6_address", DATA_TYPE_STRING, "[0-9a-fA-F:]+/[0-9]+", "IPv6 address for the interface"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_INTERFACES_BODIES[] =
{
    {"tcp_ip", DATA_TYPE_BODY, CF_TCPIP_BODY, "Interface tcp/ip properties"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const SubTypeSyntax CF_INTERFACES_SUBTYPES[] =
{
    {"agent", "interfaces", CF_INTERFACES_BODIES},
    {NULL, NULL, NULL},
};
