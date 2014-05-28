/*****************************************************************************/
/*                                                                           */
/* File: routing_services.h                                                  */
/*                                                                           */
/* Created: Tue Apr  1 14:08:20 2014                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

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
