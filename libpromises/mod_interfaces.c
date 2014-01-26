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

#include <mod_interfaces.h>
#include <syntax.h>

/**********************************************************************************************/

static const ConstraintSyntax linkstate_constraints[] =
{
    ConstraintSyntaxNewBool("bonding", "If true, the Link Aggregation Control Protocol is enabled to bond interfaces", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("state", "up,down", "Status of interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("duplex", "half,full,auto", "Duplex wiring configuration", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("spanning_tree", "on,off", "Status of local spanning tree protocol", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("mtu", CF_INTRANGE, "MTU setting", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("speed", CF_INTRANGE, "Link speed in MB/s", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("minimum_allowed_aggregation", CF_INTRANGE, "Smallest number of links up to allow bonding", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax linkstate_body = BodySyntaxNew("link_state", linkstate_constraints, NULL, SYNTAX_STATUS_NORMAL);

/**********************************************************************************************/

static const ConstraintSyntax proxy_constraints[] =
{
    ConstraintSyntaxNewString("generate_file", CF_PATHRANGE, "Filename of output", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax proxy_body = BodySyntaxNew("proxy", proxy_constraints, NULL, SYNTAX_STATUS_NORMAL);

/**********************************************************************************************/

static const ConstraintSyntax interface_constraints[] =
{
    ConstraintSyntaxNewStringList("bridge_interfaces", CF_ANYSTRING, "List of interfaces to bridge with IP forwarding", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("spanning_tree", "Spanning tree protocol active", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("aggregate", CF_ANYSTRING, "List of interfaces to bond with LACP", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("tagged_vlans", CF_IDRANGE, "List of labelled (trunk) vlan identifers for this interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("untagged_vlan", CF_IDRANGE, "Unlabelled (access) vlan", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("ipv4_address", CF_IPRANGE, "A static IPV4 address", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("ipv4_broadcast", CF_IPRANGE, "A static IPV4 address for broadcast messages", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("ipv6_addresses", CF_IPRANGE, "A static IPV6 address", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("link_state", &linkstate_body, "The desired state of the interface link", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("proxy", &proxy_body, "For treating a remote device as a peripheral", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

/**********************************************************************************************/

static const ConstraintSyntax relay_constraints[] =
{
    ConstraintSyntaxNewStringList("relay_networks", CF_ANYSTRING, "List of local networks", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("rip_metric", CF_INTRANGE, "RIP route metric", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("rip_timeout", CF_INTRANGE, "RIP timeout on updates", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("rip_split_horizon", "RIP Horizon control", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("rip_passive", "Passive mode", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("ospf_area", CF_INTRANGE, "OSPF Link database area number", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("ospf_passive", "Passive mode", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax relay_body = BodySyntaxNew("relay", relay_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax balance_constraints[] =
{
    ConstraintSyntaxNewString("algorithm", CF_ANYSTRING, "Load sharing algorithm, e.g. RR, LRU", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("nat_pool", CF_INTRANGE, "Port range for NAT traffoc", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax balancing_body = BodySyntaxNew("route_select", balance_constraints, NULL, SYNTAX_STATUS_NORMAL);

/**********************************************************************************************/

static const ConstraintSyntax network_constraints[] =
{
    ConstraintSyntaxNewBody("routed_to", &relay_body, "A body assigning a forwarding agent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("advertised_by", &relay_body, "A body assigning a protocol service", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("equivalent_gateways", CF_IDRANGE, "A list of nodes to select from", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("route_select", &balancing_body, "Settings for load balancing with balanced_relay", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

/**********************************************************************************************/

const PromiseTypeSyntax CF_INTERFACES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "interfaces", interface_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("agent", "networks", network_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};


