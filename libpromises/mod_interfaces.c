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
    ConstraintSyntaxNewOption("manager", "cfengine,native,nativefirst", "Which source of configuration is considered authoritative?", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("state", "up,down", "Status of interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("duplex", "half,full", "Duplex wiring configuration", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("auto_negotiation", "Auto-negotiation for the interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("spanning_tree", "on,off", "Status of local spanning tree protocol", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("mtu", CF_INTRANGE, "MTU setting", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("speed", CF_INTRANGE, "Link speed in MB/s", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("minimum_bond_aggregation", CF_INTRANGE, "Smallest number of links up to allow bonding", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("ospf_link_type", "broadcast,non-broadcast,point-to-multipoint,point-to-point", "Network type across this interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax linkservice_constraints[] =
{
    ConstraintSyntaxNewOption("ospf_service_type", "broadcast,non-broadcast,point-to-multipoint,point-to-point", "OSPF interface type", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("ospf_authentication_digest", CF_ANYSTRING, "Authentication digest for interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("ospf_passive_interface", "No service updates over this channel", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("ospf_area_type", "stub,nssa", "Stub type area", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("ospf_area", CF_INTRANGE, "OSPF Link database area number", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("ospf_area_authentication_digest", CF_ANYSTRING, "Authentication digest", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("ospf_substitute_prefix", CF_ANYSTRING, "Replacement prefix during rewriting", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("ospf_prefix_range", CF_ANYSTRING, "Search prefix during route rewriting", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};


static const BodySyntax linkstate_body = BodySyntaxNew("link_state", linkstate_constraints, NULL, SYNTAX_STATUS_NORMAL);
static const BodySyntax linkservice_body = BodySyntaxNew("link_services", linkservice_constraints, NULL, SYNTAX_STATUS_NORMAL);

/**********************************************************************************************/

static const ConstraintSyntax interface_constraints[] =
{
    ConstraintSyntaxNewBool("delete", "Delete an interface or bridge altogether", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("bridge_interfaces", "", "List of interfaces to bridge with IP forwarding", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("bond_interfaces", "", "List of interfaces to bond with LACP", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("tagged_vlans", "", "List of labelled (trunk) vlan identifers for this interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("untagged_vlan", CF_IDRANGE, "Unlabelled (access) vlan", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("ipv4_addresses", CF_IPRANGE, "A static IPV4 address", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("ipv6_addresses", CF_IPRANGE, "A static IPV6 address", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("link_state", &linkstate_body, "The desired state of the interface link", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("link_services", &linkservice_body, "Services configured on the interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("purge_addresses", "Remove existing addresses from interface if not defined here", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

/**********************************************************************************************/

static const ConstraintSyntax route_constraints[] =
{
    ConstraintSyntaxNewString("gateway_ip", CF_ANYSTRING, "IP address on gateway to next hop", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("gateway_interface", CF_ANYSTRING, "Interface name of gateway to next hop", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("delete_route", "If this route exists, remove it", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax route_body = BodySyntaxNew("reachable_through", route_constraints, NULL, SYNTAX_STATUS_NORMAL);

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
    ConstraintSyntaxNewBody("routed_to", &route_body, "A body assigning a forwarding agent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("balanced_destinations", CF_IDRANGE, "A list of nodes to select from", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("route_select", &balancing_body, "Settings for load balancing with balanced_relay", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

/**********************************************************************************************/

const PromiseTypeSyntax CF_INTERFACES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "interfaces", interface_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("agent", "routes", network_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
