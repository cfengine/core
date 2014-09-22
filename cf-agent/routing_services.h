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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/
/*****************************************************************************/
/*                                                                           */
/* File: routing_services.h                                                  */
/*                                                                           */
/* Created: Tue Apr  1 14:08:20 2014                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

typedef enum
{
    ROUTING_CONTROL_LOG_FILE,
    ROUTING_CONTROL_PASSWORD,
    OSPF_CONTROL_LOG_ADJACENCY_CHANGES,
    OSPF_CONTROL_LOG_TIMESTAMP_PRECISION,
    OSPF_CONTROL_ROUTER_ID,
    OSPF_CONTROL_REDISTRIBUTE,
    OSPF_CONTROL_REDISTRIBUTE_METRIC_TYPE,
    OSPF_CONTROL_REDISTRIBUTE_KERNEL_METRIC,
    OSPF_CONTROL_REDISTRIBUTE_CONNECTED_METRIC,
    OSPF_CONTROL_REDISTRIBUTE_STATIC_METRIC,
    OSPF_CONTROL_REDISTRIBUTE_BGP_METRIC,
    BGP_LOCAL_AS,
    BGP_ROUTER_ID,
    BGP_LOG_NEIGHBOR_CHANGES,
    BGP_REDISTRIBUTE,
    BGP_V4_NETWORKS,
    BGP_V6_NETWORKS,
    BGP_GRACEFUL_RESTART,
    ROUTE_CONTROL_NONE
} RouteServiceControl;

/*************************************************************************/
/* Interfaces and their services                                         */
/*************************************************************************/

typedef struct LinkState_ LinkState;
typedef struct LinkStateOSPF_ LinkStateOSPF;

struct LinkState_
{
    char *name;
    char *parent;
    Rlist *v4_addresses;
    Rlist *v6_addresses;
    char *hw_address;
    bool multicast;
    bool up;
    bool is_parent;
    int mtu;
    LinkStateOSPF *ospf;
    LinkState *next;
};

struct LinkStateOSPF_
{
    int ospf_hello_interval;
    int ospf_priority;
    char *ospf_link_type;
    char *ospf_authentication_digest;
    bool ospf_passive_interface;
    bool ospf_abr_summarization; // Not "no-summary"
    char ospf_area_type; // stub, nssa etc
    int ospf_area;
};

typedef enum
{
    CF_RC_INITIAL,
    CF_RC_INTERFACE,
    CF_RC_OSPF,
    CF_RC_BGP
} RouterCategory;


typedef struct CommonRouting_ CommonRouting;

struct CommonRouting_
{
    int log_timestamp_precision; // 0,6
    char *log_file;
    char *password;
    bool enable_password;

    char *ospf_log_adjacency_changes;
    char *ospf_router_id;

    bool ospf_redistribute_kernel;
    bool ospf_redistribute_connected;
    bool ospf_redistribute_static;
    bool ospf_redistribute_bgp;

    int ospf_redistribute_kernel_metric;
    int ospf_redistribute_connected_metric;
    int ospf_redistribute_static_metric;
    int ospf_redistribute_bgp_metric;

    int ospf_redistribute_kernel_metric_type;
    int ospf_redistribute_connected_metric_type;
    int ospf_redistribute_static_metric_type;
    int ospf_redistribute_bgp_metric_type;
    int ospf_redistribute_external_metric_type;

    bool bgp_log_neighbor_changes;

    int bgp_local_as;
    char *bgp_router_id;

    bool bgp_redistribute_kernel;
    bool bgp_redistribute_connected;
    bool bgp_redistribute_static;
    bool bgp_redistribute_ospf;
    bool bgp_graceful_restart;
    Rlist *bgp_advertisable_v4_networks;
    Rlist *bgp_advertisable_v6_networks;
};

typedef struct LinkStateBGP_ LinkStateBGP;
typedef struct BGPNeighbour_ BGPNeighbour;

struct BGPNeighbour_
{
    char *bgp_neighbour;
    int bgp_remote_as;
    int bgp_advert_interval;
    int bgp_multihop;
    int bgp_ttl_security;

    bool bgp_soft_inbound;
    bool bgp_reflector; // i.e. we are the server
    bool bgp_next_hop_self;
    BGPNeighbour *next;
};

struct LinkStateBGP_
{
    BGPNeighbour *bgp_peers;
    int bgp_local_as;

    bool have_ibgp_peers;
    bool have_ebgp_peers;

    int bgp_maximum_paths_internal;
    int bgp_maximum_paths_external;

    Item *bgp_advertise_families;
    char *bgp_ipv6_neighbor_discovery_route_advertisement;
};

typedef struct Bridges_ Bridges;

struct Bridges_
{
    char *name;
    char *id;
    Item *interfaces;
    int stp;
    Bridges *next;
};

typedef struct FIBState_ FIBState;

struct FIBState_
{
    char *network;
    char *gateway;
    char *device;
    FIBState *next;
};

typedef struct ARPState_ ARPState;

struct ARPState_
{
    char *ip;
    char *mac;
    char *device;
    char *state;
    ARPState *next;
};

/*************************************************************************/

extern CommonRouting *ROUTING_ACTIVE;
extern CommonRouting *ROUTING_POLICY;

/*************************************************************************/

void InitializeRoutingServices(const Policy *policy, EvalContext *ctx);
bool HaveRoutingService(EvalContext *ctx);
int QueryRoutingServiceState(EvalContext *ctx, CommonRouting *ospfp);
int QueryOSPFInterfaceState(EvalContext *ctx, const Attributes *a, const Promise *pp, LinkStateOSPF *ospfp);
int QueryBGPInterfaceState(EvalContext *ctx, const Attributes *a, const Promise *pp, LinkStateBGP *bgp);
void KeepOSPFInterfacePromises(EvalContext *ctx, const Attributes *a, const Promise *pp, PromiseResult *result, LinkStateOSPF *ospfp);
CommonRouting *NewRoutingState(void);
void DeleteRoutingState(CommonRouting *state);
void KeepOSPFLinkServiceControlPromises(CommonRouting *policy, CommonRouting *state);
void KeepBGPInterfacePromises(EvalContext *ctx, const Attributes *a, const Promise *pp, PromiseResult *result, LinkStateBGP *bgpp);
void KeepBGPLinkServiceControlPromises(CommonRouting *policy, CommonRouting *state);
