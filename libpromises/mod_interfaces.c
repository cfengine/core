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
    ConstraintSyntaxNewOption("bonding", "balance-rr,active-backup,balance-xor,broadcast,802.3ad,balance-tlb,balance-alb", "The Link Aggregation Control Protocol is enabled to bond interfaces", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("manager", "cfengine,native,nativefirst", "Which source of configuration is considered authoritative?", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("state", "up,down", "Status of interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("duplex", "half,full", "Duplex wiring configuration", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("auto_negotiation", "Auto-negotiation for the interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("spanning_tree", "on,off", "Status of local spanning tree protocol", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("mtu", CF_INTRANGE, "MTU setting", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("speed", CF_INTRANGE, "Link speed in MB/s", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("minimum_bond_aggregation", CF_INTRANGE, "Smallest number of links up to allow bonding", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax linkservice_constraints[] =
{
    // OSPF

    ConstraintSyntaxNewInt("ospf_hello_interval", CF_INTRANGE, "OSPF Link database area number", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("ospf_priority", CF_INTRANGE, "OSPF Link database area number", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("ospf_link_type", "broadcast,non-broadcast,point-to-multipoint,point-to-point", "OSPF interface type", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("ospf_authentication_digest", CF_ANYSTRING, "Authentication digest for interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("ospf_passive_interface", "No service updates over this channel", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("ospf_abr_summarization", "Allow Area Border Router to inject summaries into a stub area via this interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("ospf_area_type", "stub,nssa,normal", "Stub type area", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("ospf_area", CF_INTRANGE, "OSPF Link database area number", SYNTAX_STATUS_NORMAL),

    // BGP

    ConstraintSyntaxNewString("bgp_declare_session_source", CF_ANYSTRING, "Redefine identity of source IP on a connection (aka bgp-update-source)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("bgp_session_neighbors", CF_IPRANGE, "A list of IP addresses or the current (unnumbered) interface to establish a bgp connection to one or more remote peers", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("bgp_peer_as", CF_INTRANGE, "The remote peer's AS number", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("bgp_route_reflector", "client,server", "For iBGP, a central route redistribuion hub", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("bgp_ttl_security", CF_INTRANGE, "Do not accept bgp frames more than this number of hops away", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("bgp_maximum_paths", "1,255", "Enable bgp multipath support", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("bgp_advertisement_interval", CF_INTRANGE, "How long do we wait to broadcast changes (default 30 seconds for eBGP and 5 seconds for iBGP)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("bgp_internal_next_hop_self", "iBGP hops within the same AS, not to a different AS", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOptionList("bgp_advertise_families", "ipv4_unicast,ipv6_unicast", "Address families to activate from the list of networks on this router (see bgp control settings)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("bgp_external_soft_reconfiguration_inbound", "Allow updates from a neighbor without full reset of BGP session, cache policy history", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("bgp_graceful_restart", "BGP session restart RFC, default is false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("bgp_ipv6_neighbor_discovery_suppress_route_advertisement", "BGP ipv6 neighbor discovery suppression", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax tunnel_constraints[] =
{
    ConstraintSyntaxNewInt("tunnel_id", CF_VALRANGE, "Tunnel identifier number (VxLAN VNI etc)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("tunnel_address", CF_IPRANGE, "Tunnel local management/loopback address", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("tunnel_multicast_group", CF_IPRANGE, "Authentication digest for interface", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("tunnel_interface", CF_IPRANGE, "Optional particular interface for tunnel", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("tunnel_alien_addresses", CF_IDRANGE, "Name of a CFEngine array variable pointing to remote ADDRESSES data", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};


static const BodySyntax tunnel_body = BodySyntaxNew("tunnel", tunnel_constraints, NULL, SYNTAX_STATUS_NORMAL);
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
    ConstraintSyntaxNewBody("tunnel", &tunnel_body, "Tunnel and overlay configuration", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("purge_addresses", "Remove existing addresses from interface if not defined here", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

/**********************************************************************************************/

static const ConstraintSyntax reachable_constraints[] =
{
    ConstraintSyntaxNewString("gateway_ip", CF_ANYSTRING, "IP address on gateway to next hop", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("gateway_interface", CF_ANYSTRING, "Interface name of gateway to next hop", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("delete_route", "If this route exists, remove it", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax route_body = BodySyntaxNew("reachable_through", reachable_constraints, NULL, SYNTAX_STATUS_NORMAL);

/**********************************************************************************************/

static const ConstraintSyntax route_constraints[] =
{
    ConstraintSyntaxNewBody("reachable_through", &route_body, "A body assigning a forwarding agent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("balanced_destinations", CF_IDRANGE, "A list of nodes to select from", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

/**********************************************************************************************/

static const ConstraintSyntax addresses_constraints[] =
{
    ConstraintSyntaxNewString("link_address", CF_ANYSTRING, "The link level (MAC) address of the promiser", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("delete_link", "If this link mapping exists, remove it", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("interface", CF_IDRANGE, "Interface for neighbour discovery", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

/**********************************************************************************************/

const PromiseTypeSyntax CF_INTERFACES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "interfaces", interface_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("agent", "routes", route_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("agent", "addresses", addresses_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
